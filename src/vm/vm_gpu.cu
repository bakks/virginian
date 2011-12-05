// vim: filetype=c

#include <cuda.h>

/**
 * @file
 * @author Peter Bakkum <pbb7c@virginia.edu>
 *
 * @section License
 *
 * This software is provided "as is" and any expressed or implied warranties,
 * including, but not limited to, the implied warranties of merchantibility and
 * fitness for a particular purpose are disclaimed. In no event shall the
 * regents or contributors be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *      
 * @section Description 
 *
 * This file contains the virg_vm_gpu() function and virginia_gpu(), the cuda
 * kernel virtual machine. This is the only file that is compiled with nvcc,
 * rather than gcc or icc.
 */

extern "C" {
#include "virginian.h"
}

// defining VIRG_NOTWOSTEP disables the two step result writing procedure that
// is efficent for mapped memory
#ifdef VIRG_NOTWOSTEP
#define VIRG_NOTWOSTEP 1
#else
#define VIRG_NOTWOSTEP 0
#endif

/// Used in testing to return size test array from gpu-compiled code
const size_t *virg_gpu_getsizes()
{
	return &virg_testsizes[0];
}

/// GPU constant memory variable to hold the virtual machine execution context
__constant__ virg_vm vm;

/** 
 * GPU constant memory array to hold the tablet meta information for each of
 * the tablet slots allocated on the GPU
 */
__constant__ virg_tablet_meta meta[VIRG_GPU_TABLETS];

/// total number of result rows output during mapped memory vm execution
__device__ unsigned row_counter;

/// total number of result rows that have reached global memory 
__device__ unsigned rowbuff_counter;

/// counter incremented by threadblocks when they write
__device__ unsigned threadblock_order;

__global__ void virginia_gpu(unsigned tab_slot, unsigned res_slot, void* tab_,
	void* res_, unsigned start_row, unsigned num_rows, void *scratch);

/**
 * @ingroup vm
 * @brief Execute the data-parallel portion of an opcode program on a GPU
 *
 * This function handles setting up and launching the virtual machine kernel
 * that executes the virtual machine on the GPU. The user has a choice between
 * serial kernel executions for each tablet, streaming executions with
 * overlapping memcpys, and mapped execution, which is currently the fastest
 * option.
 *
 * If streaming and memory mapping are both disabled in the virginian struct,
 * then kernels will be processed on the GPU serially. This means that the data
 * tablet will be transferred to GPU memory, the virtual machine executed, then
 * the result tablet transferred back for each tablet with no overlap. This is
 * the slowest method of GPU execution, and it does not require pinned memory.
 *
 * If streaming is enabled, regardless of the mapped memory setting, then it is
 * used. This works by allocating a fixed number of tablet streams, set equal to
 * half of the number of allocated GPU tablet slots, and overlapping data
 * transfer, kernel execution, and result transfer for tablets. Each stream gets
 * a data and result tablet slot on the GPU, and the loop iterates through each
 * stream in a round-robin fashion executing virtual machines on tablets. Note
 * that when we loop back and re-use streams then we block to wait for execution
 * if the previous asynchronous launches. On Tesla C1060 hardware you cannot
 * overlap transfers to the GPU with transfers from it, and thus streaming is
 * about as fast as serial execution. Also note that on this hardware the
 * semantics for asynchronous operations are somewhat frusterating. Based on
 * informal tests, it appears that even though asynchronous operations are
 * completely asynchronous with respect to the calling thread, the order in
 * which they are added to streams has an effect on when they are run. Thus,
 * there are non-optimal orderings of memory copies and kernel executions when
 * multiple asynchronous operations are queued in multiple streams. Even with
 * smarter ordering, however, the inability of current hardware to transfer both
 * ways across the PCI bus simultaneously means that this method of execution is
 * currently only useful if either the data or results of a query are resident
 * in GPU memory for the entire processing operation. For some reason I could
 * only create 6 streams at a time during my testing, so you may encounter
 * problems if you allocate more than 12 gpu tablet slots.
 *
 * Mapped memory is currently the fastest method for GPU processing by far. It
 * is also much simpler, since we have fewer memory copies and we don't have to
 * manage multiple streams at the same time. The tablet row counter is
 * implemented as a separate variable for mapped memory, since it would be very
 * very expensive to perform atomic operations on a mapped location.
 *
 * @param v     Pointer to the state struct of the database system
 * @param vm	Pointer to the context struct of the virtual machine
 * @param tab	Pointer to the pointer to the current data tablet to process
 * @param res	Pointer to the pointer to the current result tablet
 * @param num_tablets Number of tablets to process on the GPU, 0 if as many as
 * possible
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_gpu(virginian *v, virg_vm *vm_, virg_tablet_meta **tab, virg_tablet_meta **res, unsigned num_tablets)
{
	unsigned proced = 0;
	//num_tablets = 5;

	VIRG_CHECK(v->threads_per_block != VIRG_THREADSPERBLOCK,
		"Cannot change compile-time threads per block");
	VIRG_CHECK(v->threads_per_block == v->threads_per_block & 0xFFFFFFC0,
		"Threads per block must be a multiple of 64");

	// execute GPU kernels in serial with no overlapping memory copies
	if(v->use_stream == 0 && v->use_mmap == 0)
	{
		// copy virtual machine context to constant memory
		cudaMemcpyToSymbol((char*)&vm, (char*)vm_,
			sizeof(virg_vm), 0, cudaMemcpyHostToDevice);
		VIRG_CUDCHK("serial const memcpy 1");

		// copy result meta information to constant memory
		// we only need to do this once for multiple kernel calls because the
		// information about the column spacing is identical between all result
		// tablets
		cudaMemcpyToSymbol((char*)&meta, (char*)res[0],
			sizeof(virg_tablet_meta), sizeof(virg_tablet_meta),
			cudaMemcpyHostToDevice);
		VIRG_CUDCHK("serial const memcpy 2");

		void *tab_slot = v->gpu_slots;
		void *res_slot = (char*)v->gpu_slots + VIRG_TABLET_SIZE;

		// create timers
		cudaEvent_t start, data, kernel, results;
		cudaEventCreate(&start);
		cudaEventCreate(&data);
		cudaEventCreate(&kernel);
		cudaEventCreate(&results);
		vm_->timing1 = 0;
		vm_->timing2 = 0;
		vm_->timing3 = 0;

		while(1) {
			VIRG_CUDCHK("const clear");

			VIRG_CUDCHK("before serial const 2 memcpy");
			// copy data tablet meta information to constant memory
			cudaMemcpyToSymbol((char*)&meta, (char*)tab[0],
				sizeof(virg_tablet_meta), 0, cudaMemcpyHostToDevice);
			VIRG_CUDCHK("serial const 2 memcpy");

			// round threadblocks up given number of rows to process and threads
			// per block
			unsigned rows = tab[0]->rows;
			int blocks = (rows + v->threads_per_block - 1) /
				v->threads_per_block;
			assert(blocks < 65536);

			// start timer
			cudaEventRecord(start, 0);

			// copy entire data tablet to GPU memory
			cudaMemcpy(tab_slot, (char*)tab[0],
				tab[0]->size, cudaMemcpyHostToDevice);
			// copy res meta information to GPU memory, where the rows element
			// will be updated as result rows are output
			cudaMemcpy(res_slot, (char*)res[0],
				sizeof(virg_tablet_meta), cudaMemcpyHostToDevice);
			VIRG_CUDCHK("data memcpy");

#ifdef VIRG_DEBUG
			cudaMemset((char*)res_slot + sizeof(virg_tablet_meta),
				0xDEADBEEF, VIRG_TABLET_SIZE - sizeof(virg_tablet_meta));
#endif

			// record we're done with data transfer
			cudaEventRecord(data, 0);

			virg_timer_start();

			// kernel launch
			void* tab_arg = v->gpu_slots;
			void* res_arg = (char*)v->gpu_slots + VIRG_TABLET_SIZE;

			virginia_gpu<<<blocks, v->threads_per_block>>>
				(0, 1, tab_arg, res_arg, 0, 0, NULL);
	cudaDeviceSynchronize();

			VIRG_CUDCHK("Single kernel launch");

			// record we're done with the kernel call
			cudaEventRecord(kernel, 0);

			// transfer result tablet back from GPU memory
			cudaMemcpy((char*)res[0], res_slot,
				VIRG_TABLET_SIZE, cudaMemcpyDeviceToHost);

			//virg_print_tablet_meta(res[0]);

			// record we're done with the results transfer
			cudaEventRecord(results, 0);

			// output timing results for this tablet
			float f[3];
			cudaEventElapsedTime(&f[0], start, data);
			cudaEventElapsedTime(&f[1], data, kernel);
			cudaEventSynchronize(results);
			cudaEventElapsedTime(&f[2], kernel, results);
			//fprintf(stderr, "serial block %u: %f %f %f %f\n", proced, cum, f[0], f[1], f[2]);
			vm_->timing1 += f[0];
			vm_->timing2 += f[1];
			vm_->timing3 += f[2];

			proced++;

			// if we've processed enough tablets exit the loop
			if(tab[0]->last_tablet || (num_tablets != 0 && proced >= num_tablets))
				break;

			// load next data tablet
			virg_db_loadnext(v, tab);
			// if this tablet has no rows, break from this loop
			// this occurs when a new data tablet is created during an insert
			// operation but no rows have been added to it yet
			if(tab[0]->rows == 0)
				break;

			// safely allocate next result tablet
			virg_tablet_meta *temp = res[0];
			virg_vm_allocresult(v, vm_, res, res[0]);
			virg_tablet_unlock(v, temp->id);
		}

		vm_->timing1 /= 1000;
		vm_->timing2 /= 1000;
		vm_->timing3 /= 1000;

		// destruct timers
		cudaEventDestroy(start);
		cudaEventDestroy(data);
		cudaEventDestroy(kernel);
		cudaEventDestroy(results);
	}
	// if the streaming functionality is turned on
	else if(v->use_stream)
	{
		// copy virtual machine context to GPU constant memory
		cudaMemcpyToSymbol((char*)&vm, (char*)vm_,
			sizeof(virg_vm), 0, cudaMemcpyHostToDevice);
		VIRG_CUDCHK("const memcpy");

		// we should always have an even number of tablets
		assert(VIRG_GPU_TABLETS % 2 == 0);
		unsigned stream_width = VIRG_GPU_TABLETS / 2;
		cudaStream_t stream[stream_width];
		unsigned slot_ids[stream_width];
		int slot_wait = 0;
		unsigned i;

		// construct streams 
		for(i = 0; i < stream_width; i++)
			cudaStreamCreate(&stream[i]);
		VIRG_CUDCHK("stream create");

		// create timers for each stream independently
		cudaEvent_t ev_create[stream_width], ev_start[stream_width],
				ev_data[stream_width], ev_kernel[stream_width],
				ev_results[stream_width];
		for(i = 0; i < stream_width; i++) {
			cudaEventCreate(&ev_create[i]);
			cudaEventCreate(&ev_start[i]);
			cudaEventCreate(&ev_data[i]);
			cudaEventCreate(&ev_kernel[i]);
			cudaEventCreate(&ev_results[i]);
		}

		// start timer for each stream
		for(i = 0; i < stream_width; i++)
			cudaEventRecord(ev_create[i], stream[i]);

		// process tablets until finished
		for(i = 0; 1; i++) {
			// if every stream has been used, go back to use the first stream
			// again
			if(i >= stream_width) {
				i = 0;
				slot_wait = 1;
			}

			// if we are re-using streams then we need to block until they are
			// actually finished
			if(slot_wait) {
				VIRG_CUDCHK("before stream synchronize");
				// block
				cudaStreamSynchronize(stream[i]);
				VIRG_CUDCHK("stream synchronize");
				// unlock the tablets that the stream was using
				virg_tablet_unlock(v, slot_ids[i * 2]);
				virg_tablet_unlock(v, slot_ids[i * 2 + 1]);

				// record processing completion and output times
				cudaEventSynchronize(ev_results[i]);
				float f[4];
				cudaEventElapsedTime(&f[0], ev_create[i], ev_start[i]);
				cudaEventElapsedTime(&f[1], ev_start[i], ev_data[i]);
				cudaEventElapsedTime(&f[2], ev_data[i], ev_kernel[i]);
				cudaEventElapsedTime(&f[3], ev_kernel[i], ev_results[i]);
				fprintf(stderr, "stream %u: %f %f %f %f\n", i, f[0], f[1], f[2], f[3]);
			}

			// if the data tablet doesn't have any rows then we're finished
			if(tab[0]->rows == 0) {
				proced++;
				slot_ids[i * 2] = tab[0]->id;
				slot_ids[i * 2 + 1] = res[0]->id;
				break;
			}

#ifdef VIRG_DEBUG
			virg_tablet_check(tab[0]);
#endif

			// round up blocks given rows to process and threads per block	
			int blocks = (tab[0]->rows + v->threads_per_block - 1) / v->threads_per_block;
			assert(blocks < 65536);

			virg_tablet_meta *temp_tab, *temp_res;

			// if there are still tablets to process load, otherwise don't
			// note that we don't exit here because we need to wait for the
			// other streams to finish
			if(!tab[0]->last_tablet && !(num_tablets != 0 && proced + 1 > num_tablets)) {
				virg_db_load(v, tab[0]->next, &temp_tab);
				virg_vm_allocresult(v, vm_, &temp_res, res[0]);
			}

			// start timer for this stream
			cudaEventRecord(ev_start[i], stream[i]);

			// start tablet memcpy for this stream
			cudaMemcpyAsync((char*)v->gpu_slots + (i * 2) * VIRG_TABLET_SIZE,
				(char*)tab[0], tab[0]->size, cudaMemcpyHostToDevice, stream[i]);
			//virg_print_tablet_meta(tab[0]);
			VIRG_CUDCHK("tab memcpy");

			// start tablet meta to constant memory memcpy for this stream
			cudaMemcpyToSymbolAsync((char*)&meta, (char*)tab[0],
				sizeof(virg_tablet_meta), i * 2 * sizeof(virg_tablet_meta),
				cudaMemcpyHostToDevice, stream[i]);
			VIRG_CUDCHK("tab meta");

			// if we haven't put the result meta information in this stream's
			// constant memory area yet
			if(!slot_wait) {
				// copy result meta information for this stream
				cudaMemcpyToSymbolAsync((char*)&meta, (char*)res[0],
					sizeof(virg_tablet_meta), (i * 2 + 1) * sizeof(virg_tablet_meta),
					cudaMemcpyHostToDevice, stream[i]);
				VIRG_CUDCHK("res meta");
			}

			// copy meta information to global memory as well so that the rows
			// variable can be updated during query execution
			cudaMemcpyAsync((char*)v->gpu_slots + (i * 2 + 1) * VIRG_TABLET_SIZE,
				(char*)res[0], sizeof(virg_tablet_meta), cudaMemcpyHostToDevice, stream[i]);
			VIRG_CUDCHK("res setup memcpy");

			// record we're done with data transfer for this stream
			cudaEventRecord(ev_data[i], stream[i]);

			// launch the kernel for this stream
			void *tab_arg = (char*)v->gpu_slots + (i * 2) * VIRG_TABLET_SIZE;
			void *res_arg = (char*)v->gpu_slots + (i * 2 + 1) * VIRG_TABLET_SIZE;
			virginia_gpu<<<blocks, v->threads_per_block, 0, stream[i]>>>
				(i * 2, i * 2 + 1, tab_arg, res_arg, 0, 0, NULL);
			VIRG_CUDCHK("kernel");

			// record that the kernel execution has finished
			cudaEventRecord(ev_kernel[i], stream[i]);

			// copy result tablet back for this stream
			cudaMemcpyAsync((char*)res[0], (char*)v->gpu_slots + (i * 2 + 1) * VIRG_TABLET_SIZE,
				VIRG_TABLET_SIZE, cudaMemcpyDeviceToHost, stream[i]);
			VIRG_CUDCHK("res memcpy");

			// record that we're done with the result transfer for this stream
			cudaEventRecord(ev_results[i], stream[i]);

			// store the current data and result tablet pointers in the stream's
			// slot
			proced++;
			slot_ids[i * 2] = tab[0]->id;
			slot_ids[i * 2 + 1] = res[0]->id;

			// check if we've processed enough tablets
			if(tab[0]->last_tablet || (num_tablets != 0 && proced >= num_tablets))
				break;

			tab[0] = temp_tab;
			res[0] = temp_res;
		}

		i++;

		unsigned j;
		if(!slot_wait)
			i = 0;
		// for each unfinished stream
		for(j = 0; j < VIRG_MIN(stream_width, proced); j++, i++) {
			if(i >= stream_width)
				i = 0;

			// wait for the stream to finish and print timing information
			cudaStreamSynchronize(stream[i]);
			cudaEventSynchronize(ev_results[i]);
			float f[4];
			cudaEventElapsedTime(&f[0], ev_create[i], ev_start[i]);
			cudaEventElapsedTime(&f[1], ev_start[i], ev_data[i]);
			cudaEventElapsedTime(&f[2], ev_data[i], ev_kernel[i]);
			cudaEventElapsedTime(&f[3], ev_kernel[i], ev_results[i]);
			//fprintf(stderr, "stream %u: %f %f %f %f\n", i, f[0], f[1], f[2], f[3]);

			// leave last data and result tablets locked
			if(j < stream_width - 1 && j < proced - 1) {
				virg_tablet_unlock(v, slot_ids[i * 2]);
				virg_tablet_unlock(v, slot_ids[i * 2 + 1]);
			}
		}

		// destruct timers and streams
		for(i = 0; i < stream_width; i++) {
			cudaEventDestroy(ev_create[i]);
			cudaEventDestroy(ev_start[i]);
			cudaEventDestroy(ev_data[i]);
			cudaEventDestroy(ev_kernel[i]);
			cudaEventDestroy(ev_results[i]);
			cudaStreamDestroy(stream[i]);
		}
	}
	// memory mapped kernel execution
	else if(v->use_mmap)
	{
		assert(VIRG_GPU_TABLETS >= 2);
#ifdef VIRG_NOPINNED
		VIRG_CHECK(1, "cannot use mapped execution without pinned memory");
#endif

		// copy virtual machine context into gpu constant memory
		cudaMemcpyToSymbol((char*)&vm, (char*)vm_,
			sizeof(virg_vm), 0, cudaMemcpyHostToDevice);
		// copy result tablet meta data into gpu constant memory
		// this needs to be done only once since the column sizes etc don't
		// change
		cudaMemcpyToSymbol((char*)&meta, (char*)res[0],
			sizeof(virg_tablet_meta), sizeof(virg_tablet_meta), cudaMemcpyHostToDevice);
		VIRG_CUDCHK("mapped const memcpy");

		cudaMemcpyToSymbol((char*)&meta, (char*)tab[0],
			sizeof(virg_tablet_meta), 0, cudaMemcpyHostToDevice);
		VIRG_CUDCHK("mapped const 2 memcpy");

		// construct timers
		cudaEvent_t start, data, kernel, results;
		cudaEventCreate(&start);
		cudaEventCreate(&data);
		cudaEventCreate(&kernel);
		cudaEventCreate(&results);
		float cum = 0;

		while(1) {
			// start timer
			cudaEventRecord(start, 0);

			//virg_print_tablet_meta(tab[0]);

			//fprintf(stderr, "::::%u\n", sizeof(virg_tablet_meta));

			// copy tablet meta information to gpu constant memory
			VIRG_CUDCHK("before const 2 memcpy");
			cudaMemcpyToSymbol((char*)&meta, (char*)tab[0],
				sizeof(virg_tablet_meta), 0, cudaMemcpyHostToDevice);
			VIRG_CUDCHK("const 2 memcpy");

			// round number of thread blocks up given the number of rows to
			// process and the threads per block
			unsigned rows = tab[0]->rows;
			int blocks = (rows + v->threads_per_block - 1) / v->threads_per_block;
			assert(blocks < 65536);

			unsigned zero = 0;
			// copy 0 into the result row counter
			cudaMemcpyToSymbol((char*)&row_counter, (char*)&zero,
				sizeof(unsigned), 0, cudaMemcpyHostToDevice);
			// copy 0 into the result row buffer counter
			cudaMemcpyToSymbol((char*)&rowbuff_counter, (char*)&zero,
				sizeof(unsigned), 0, cudaMemcpyHostToDevice);
			cudaMemcpyToSymbol((char*)&threadblock_order, (char*)&zero,
				sizeof(unsigned), 0, cudaMemcpyHostToDevice);
			cudaMemset((char*)v->gpu_slots + VIRG_TABLET_SIZE, 0,
				sizeof(unsigned) * blocks);
			VIRG_CUDCHK("row_counter set");

			// record that we're done transferring data
			// since we're using mapped memory this is negligible since we just
			// need to set constant memory and 2 variables
			cudaEventRecord(data, 0);

			// get gpu pointers to the data and result tablets in main memory
			void *tab_arg;
			void *res_arg;
			cudaHostGetDevicePointer(&tab_arg, tab[0], 0);
			VIRG_CUDCHK("get tab device ptr");
			cudaHostGetDevicePointer(&res_arg, res[0], 0);
			VIRG_CUDCHK("get res device ptr");

			// launch kernel using mapped pointers
			virginia_gpu<<<blocks, v->threads_per_block>>>
				(0, 1, tab_arg, res_arg, 0, 0, v->gpu_slots);
			VIRG_CUDCHK("Single mapped kernel launch");

			// record we're done with the kernel call
			cudaEventRecord(kernel, 0);

			// copy the number of tablet result rows from gpu memory
			cudaMemcpyFromSymbol((char*)&res[0]->rows, (char*)&row_counter,
				sizeof(unsigned), 0, cudaMemcpyDeviceToHost);

			// record that we're done transferring results information
			// this should also be negligible
			cudaEventRecord(results, 0);

			// get timing results
			float f[3];
			cudaEventElapsedTime(&f[0], start, data);
			cudaEventElapsedTime(&f[1], data, kernel);
			cudaEventSynchronize(results);
			cudaEventElapsedTime(&f[2], kernel, results);

			// print timing information
			//fprintf(stderr, "block %u: %f %f %f %f\n", proced, cum, f[0], f[1], f[2]);
			
			// add to cumulative time
			cum += f[0] + f[1] + f[2];

			proced++;

			// check if we're done processing tablets
			if(tab[0]->last_tablet || (num_tablets != 0 && proced >= num_tablets))
				break;

			// load next data tablet
			virg_db_loadnext(v, tab);

			// if this data tablet has no rows, finish
			if(tab[0]->rows == 0)
				break;

			virg_tablet_meta *temp = res[0];
			virg_vm_allocresult(v, vm_, res, res[0]);
			virg_tablet_unlock(v, temp->id);
		}

		// destruct timers
		cudaEventDestroy(start);
		cudaEventDestroy(data);
		cudaEventDestroy(kernel);
		cudaEventDestroy(results);
	}

	// wait for all cuda operations to finish
	cudaDeviceSynchronize();

	return VIRG_SUCCESS;
}

/// shared memory used for reductions
__shared__ int reduct[512];
/// shared memory location for the start of this threadblock's result area
__shared__ unsigned bstart;
/// shared memory variable for the number of rows output by this threadblock
__shared__ unsigned block;

/// where in the order this threadblock writes its results
__shared__ unsigned shared_blockorder;
///	how many rows have been written in the first result block
__shared__ unsigned thisblockwritten;
///	how many rows have been written in the second result block
__shared__ unsigned nextblockwritten;


#define OPARGS (															   \
	virg_op op,																   \
	virg_vm_context &context, 												   \
	virg_tablet_meta *meta_tab,												   \
	virg_tablet_meta *meta_res,												   \
	void *tab,																   \
	virg_tablet_meta *res,													   \
	void *scratch,															   \
	int &valid,																   \
	unsigned &pc,															   \
	unsigned &pc_wait)

__device__ __forceinline__ void op_Column OPARGS
{
	char *p = (char*)tab + meta_tab->fixed_block;
	unsigned row = blockIdx.x * blockDim.x + threadIdx.x;

	p += meta_tab->fixed_offset[op.p2] + meta_tab->fixed_stride[op.p2] * row;

	switch(meta_tab->fixed_stride[op.p2]) {
		case 4: context.reg[op.p1].i = *((int*)p); break;
		case 8: context.reg[op.p1].d = *((double*)p); break;
		case 1: context.reg[op.p1].c = *p; break;
	}																
	context.type[op.p1] = meta_tab->fixed_type[op.p2];
	context.stride[op.p1] = meta_tab->fixed_stride[op.p2];
}

__device__ __forceinline__ void op_Rowid OPARGS
{
	unsigned row = blockIdx.x * blockDim.x + threadIdx.x;
	char *p = (char*)tab + meta_tab->key_block + meta_tab->key_stride * row;

	switch(meta_tab->fixed_stride[op.p2]) {
		case 4: context.reg[op.p1].i = *((int*)p); break;
		case 8: context.reg[op.p1].d = *((double*)p); break;
		case 1: context.reg[op.p1].c = *p; break;
	}																
	context.type[op.p1] = meta_tab->fixed_type[op.p2];
	context.stride[op.p1] = meta_tab->fixed_stride[op.p2];
}

__device__ __forceinline__ void op_Integer OPARGS
{
	context.reg[op.p1].i = op.p2;
	context.type[op.p1] = VIRG_INT;
	context.stride[op.p1] = sizeof(int);
}

__device__ __forceinline__ void op_Float OPARGS
{
	context.reg[op.p1].f = op.p4.f;
	context.type[op.p1] = VIRG_FLOAT;
	context.stride[op.p1] = sizeof(float);
}

__device__ __forceinline__ void op_Invalid OPARGS
{
	valid = 0;
}

/**
 * A macro to compare identically typed register values and manipulate each
 * thread's program counter based on the result of this comparison. This is
 * implemented as a macro so that the comparison operator can be easily changed,
 * for example REGCMP(<=), used by the Le opcode.
 */
#define REGCMP(cmpop)														   \
	int x = 0;																   \
	switch(context.type[op.p1]) {											   \
		case VIRG_INT:														   \
			x = (context.reg[op.p1].i cmpop context.reg[op.p2].i);		   	   \
			break;															   \
		case VIRG_FLOAT:													   \
			x = (context.reg[op.p1].f cmpop context.reg[op.p2].f);			   \
			break;															   \
		case VIRG_INT64:													   \
			x = (context.reg[op.p1].li cmpop context.reg[op.p2].li);		   \
			break;															   \
		case VIRG_DOUBLE:													   \
			x = (context.reg[op.p1].d cmpop context.reg[op.p2].d);			   \
			break;															   \
		case VIRG_CHAR:														   \
			x = (context.reg[op.p1].c cmpop context.reg[op.p2].c);			   \
			break;															   \
	}																		   \
	if(x) {																	   \
		if(valid)															   \
			valid = op.p4.i;												   \
		pc_wait = op.p3 - pc - 1;											   \
	}


__device__ __forceinline__ void op_Neq	OPARGS { REGCMP(!=) }
__device__ __forceinline__ void op_Gt	OPARGS { REGCMP(>)  }
__device__ __forceinline__ void op_Ge	OPARGS { REGCMP(>=) }
__device__ __forceinline__ void op_Lt	OPARGS { REGCMP(<)  }
__device__ __forceinline__ void op_Le	OPARGS { REGCMP(<=) }
__device__ __forceinline__ void op_And	OPARGS { REGCMP(&&) }
__device__ __forceinline__ void op_Or	OPARGS { REGCMP(||) }

__device__ __forceinline__ void op_Eq	OPARGS
{
	int x = 0;														
	switch(context.type[op.p1]) {									
		case VIRG_INT:												
			x = (context.reg[op.p1].i == context.reg[op.p2].i);	
			break;													
		case VIRG_FLOAT:
		{
			float f = context.reg[op.p1].f - context.reg[op.p2].f;
			x = (f <= VIRG_FLOAT_ERROR && f >= -VIRG_FLOAT_ERROR);
			break;													
		}
		case VIRG_INT64:											
			x = (context.reg[op.p1].li == context.reg[op.p2].li);
			break;													
		case VIRG_DOUBLE:											
		{
			float d = context.reg[op.p1].f - context.reg[op.p2].f;
			x = (d < VIRG_FLOAT_ERROR && d > -VIRG_FLOAT_ERROR);
			break;													
		}
		case VIRG_CHAR:												
			x = (context.reg[op.p1].c == context.reg[op.p2].c);	
			break;													
	}																
	if(x) {															
		if(valid)													
			valid = op.p4.i;										
		pc_wait = op.p3 - pc - 1;									
	}
}

__device__ __forceinline__ void op_Not OPARGS
{
	int x = 0;
	switch(context.type[op.p1]) {
		case VIRG_INT:
			x = (context.reg[op.p1].i ? 1 : 0);
			break;
		case VIRG_FLOAT:
			x = (context.reg[op.p1].f ? 1 : 0);
			break;
		case VIRG_INT64:
			x = (context.reg[op.p1].li ? 1 : 0);
			break;
		case VIRG_DOUBLE:
			x = (context.reg[op.p1].d ? 1 : 0);
			break;
		case VIRG_CHAR:
			x = (context.reg[op.p1].c ? 1 : 0);
			break;
	}
	if(!x) {
		if(valid)
			valid = op.p4.i;
		pc_wait = op.p3 - pc - 1;
	}
}

/** A macro to perform a mathematical operation of the form
 * reg[p1] = reg[p2] operator reg[p3]. Like regcmp, this is used so that
 * multiple opcodes can use this code and easily change the math operator, as in
 * MATHOP(+).
 */
#define MATHOP(mop)															   \
	switch(context.type[op.p2]) {											   \
		case VIRG_INT:														   \
			context.reg[op.p1].i =											   \
				 (context.reg[op.p2].i mop context.reg[op.p3].i);			   \
			break;															   \
		case VIRG_FLOAT:													   \
			context.reg[op.p1].f =											   \
				(context.reg[op.p2].f mop context.reg[op.p3].f);			   \
			break;															   \
		case VIRG_INT64:													   \
			context.reg[op.p1].li =											   \
				(context.reg[op.p2].li mop context.reg[op.p3].li);			   \
			break;															   \
		case VIRG_DOUBLE:													   \
			context.reg[op.p1].d = 											   \
				(context.reg[op.p2].d mop context.reg[op.p3].d);			   \
			break;															   \
		case VIRG_CHAR:														   \
			context.reg[op.p1].c =											   \
				(context.reg[op.p2].c mop context.reg[op.p3].c);			   \
			break;															   \
	}																		   \
	context.type[op.p1] = context.type[op.p2];							       \
	context.stride[op.p1] = context.stride[op.p2];

__device__ __forceinline__ void op_Add OPARGS { MATHOP(+) }
__device__ __forceinline__ void op_Sub OPARGS { MATHOP(-) }
__device__ __forceinline__ void op_Mul OPARGS { MATHOP(*) }
__device__ __forceinline__ void op_Div OPARGS { MATHOP(/) }


/**
 * A convenience macro for castng a register from one type to another
 */
#define CASTREG(reg_, destkey, t, srckey)									   \
	context.reg[reg_].destkey = (t) context.reg[reg_].srckey;


__device__ __forceinline__ void op_Cast OPARGS
{
	switch(op.p1) {
		case VIRG_INT:
			switch(context.type[op.p2]) {
				case VIRG_FLOAT:
					CASTREG(op.p2, i, int, f);
					break;
				case VIRG_INT64:
					CASTREG(op.p2, i, int, li);
					break;
				case VIRG_DOUBLE:
					CASTREG(op.p2, i, int, d);
					break;
				case VIRG_CHAR:
					CASTREG(op.p2, i, int, c);
					break;
			}
			context.stride[op.p2] = sizeof(int);
			break;
		case VIRG_FLOAT:
			switch(context.type[op.p2]) {
				case VIRG_INT:
					CASTREG(op.p2, f, float, i);
					break;
				case VIRG_INT64:
					CASTREG(op.p2, f, float, li);
					break;
				case VIRG_DOUBLE:
					CASTREG(op.p2, f, float, d);
					break;
				case VIRG_CHAR:
					CASTREG(op.p2, f, float, c);
					break;
			}
			context.stride[op.p2] = sizeof(int);
			break;
		case VIRG_INT64:
			switch(context.type[op.p2]) {
				case VIRG_INT:
					CASTREG(op.p2, li, long long int, i);
					break;
				case VIRG_FLOAT:
					CASTREG(op.p2, li, long long int, f);
					break;
				case VIRG_DOUBLE:
					CASTREG(op.p2, li, long long int, d);
					break;
				case VIRG_CHAR:
					CASTREG(op.p2, li, long long int, c);
					break;
			}
			context.stride[op.p2] = sizeof(int);
			break;
		case VIRG_DOUBLE:
			switch(context.type[op.p2]) {
				case VIRG_INT:
					CASTREG(op.p2, d, double, i);
					break;
				case VIRG_FLOAT:
					CASTREG(op.p2, d, double, f);
					break;
				case VIRG_INT64:
					CASTREG(op.p2, d, double, li);
					break;
				case VIRG_CHAR:
					CASTREG(op.p2, d, double, c);
					break;
			}
			context.stride[op.p2] = sizeof(int);
			break;
		case VIRG_CHAR:
			switch(context.type[op.p2]) {
				case VIRG_INT:
					CASTREG(op.p2, c, char, i);
					break;
				case VIRG_FLOAT:
					CASTREG(op.p2, c, char, f);
					break;
				case VIRG_INT64:
					CASTREG(op.p2, c, char, li);
					break;
				case VIRG_CHAR:
					CASTREG(op.p2, c, char, d);
					break;
			}
			context.stride[op.p2] = sizeof(int);
			break;
	}
	context.type[op.p2] = (virg_t)op.p1;
}


__device__ __forceinline__ void op_Result OPARGS
{
	/**
	 * To manage outputting result rows, every thread atomically increments a
	 * shared variable to determine how many result rows there will be, then the
	 * first thread in the block atomically adds this number to a global
	 * variable, with appropriate threadblock synchronization in between these
	 * calls. This is more efficient than a scan operation in this case, since
	 * we don't care about the order of the rows and shared memory atomic
	 * operations are actually fairly efficient when compared to reduction
	 * operations, which tend to cause shared memory bank conflicts.
	 */
	unsigned place;

	__syncthreads();

	// if this is a valid row, update the shared variable
	if(valid)
		place = atomicAdd(&block, 1);

	__syncthreads();

	unsigned num_rows = block;

	// the first thread in the block updates the global variable
	if(threadIdx.x == 0) {
		if(scratch != NULL) // mapped
			bstart = atomicAdd(&row_counter, block);
		else {
			bstart = atomicAdd(&res->rows, block);
		}
	}

	__syncthreads();

	// TODO check for result tablet overflow

	unsigned block_start = bstart;
	//block = 0;
	unsigned j;
	char *p;

	// if this is a result row
	if(valid)
		// for every register to be output for this result row
		for(j = op.p1; j < op.p1 + op.p2; j++) {
			// register/column stride
			unsigned stride = context.stride[j];
			unsigned col_location = stride * (block_start + place);

			// if not mapped, write to the result tablet
			if(scratch == NULL || VIRG_NOTWOSTEP)
				p = (char*)res + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + col_location;

			// if mapped, write to the scratch memory area for buffering before
			// sending across the PCI bus
			else
				p = (char*)scratch + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + col_location;

			//printf("write row %u\n", place);

			// switch the write based on the variable stride
			switch(stride) {
				case 4:
					((int*)p)[0] = context.reg[j].i;
					break;
				case 8:
					((double*)p)[0] = context.reg[j].d;
					break;
				case 1:
					p[0] = context.reg[j].c;
					break;
			}
		}


	/**
	 * If mapped memory is being used, result rows are written back to main
	 * memory in a two step process. This both reduces the number of accesses
	 * that have to cross the PCI bus, and ensures that the results are being
	 * coalesced properly, as the coalescing rules for mapped memory appear to
	 * be slightly more strict than for GPU global memory. First, result rows
	 * are written to GPU global memory, exactly how they would be if serial
	 * execution and memory transfers were being used. The results are divided
	 * into threadblock-sized blocks, and once we are sure that the results are
	 * written to global memory, we increment the global counters for the blocks
	 * that we have written to. After incrementing, we check to see if the
	 * blocks we have written to are completely filled with rows. If so, every
	 * thread in the threadblock transfers a row from the result block to main
	 * memory, thus maximizing efficiency and coalescing. Tests show that this
	 * is a very good way of managing result transfers of this kind, since
	 * transfers back to main memory proceed efficiently but are overlapped with
	 * kernel execution.
	 */

	// if we are using mapped memory
	if(scratch != NULL && !VIRG_NOTWOSTEP)
	{
		// make sure our result writes have reached global memory
		__threadfence();
		__syncthreads();

		// only do this in the first cuda thread
		if(threadIdx.x == 0) {
			shared_blockorder = atomicAdd(&threadblock_order, 1);
			
			if(num_rows > 0) {
				unsigned *threadswritten = (unsigned*)((char*)scratch +
					VIRG_TABLET_SIZE);
				unsigned result_blockid = block_start / VIRG_THREADSPERBLOCK;//blockDim.x;

				unsigned blockbreak = ((block_start + num_rows - 1) &
					VIRG_THREADSPERBLOCK_MASK);
				unsigned x, y;
			
				// if the results span over two different thread block areas
				if((blockbreak > (block_start & VIRG_THREADSPERBLOCK_MASK)) &&
					(block_start + num_rows != blockbreak))
				{
					// increment the rows that have been written for both
					unsigned thisnewrows = blockbreak - block_start;
					x = atomicAdd(&threadswritten[result_blockid], thisnewrows);
					thisblockwritten = x + thisnewrows;

					unsigned nextnewrows = block_start + num_rows - blockbreak;
					y = atomicAdd(&threadswritten[result_blockid + 1],
						nextnewrows);
					nextblockwritten = y + nextnewrows;
				}
				// otherwise increment the rows that have been written for only
				// this result threadblock area
				else {
					x = atomicAdd(&threadswritten[result_blockid], num_rows);
					thisblockwritten = x + num_rows;
					nextblockwritten = 0;
				}
			}
		}

		__syncthreads();
	
		// if an entire threadblock-sized area has been filled with result rows
		if(num_rows > 0 && thisblockwritten == VIRG_THREADSPERBLOCK)
		{
			unsigned aligned_start = block_start & VIRG_THREADSPERBLOCK_MASK;

			// do coalesced writes from global memory to mapped main memory of
			// this block of results
			for(j = op.p1; j < op.p1 + op.p2; j++) {
				unsigned stride = context.stride[j];

				p = (char*)res + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);
				char *p_src = (char*)scratch + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);

				switch(stride) {
					case 4:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 8:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 1:
						if(threadIdx.x < VIRG_THREADSPERBLOCK / 4)
							((int*)p)[0] = ((int*)p_src)[0];
						break;
				}
			}
		}

		if(shared_blockorder == gridDim.x - 1)
		{
			unsigned aligned_start = row_counter & VIRG_THREADSPERBLOCK_MASK;

			// do coalesced writes from global memory to mapped main memory of
			// this block of results
			for(j = op.p1; j < op.p1 + op.p2; j++) {
				unsigned stride = context.stride[j];

				p = (char*)res + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);
				char *p_src = (char*)scratch + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);

				switch(stride) {
					case 4:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 8:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 1:
						if(threadIdx.x < VIRG_THREADSPERBLOCK / 4)
							((int*)p)[0] = ((int*)p_src)[0];
						break;
				}
			}
		}

		// if a second thread-block sized area has been filled with result rows
		if(num_rows > 0 && nextblockwritten == VIRG_THREADSPERBLOCK)
		{
			unsigned aligned_start = (block_start + num_rows) & VIRG_THREADSPERBLOCK_MASK;
			// do coalesced writes from global memory to mapped main memory of
			// this block of results
			for(j = op.p1; j < op.p1 + op.p2; j++) {
				unsigned stride = context.stride[j];

				p = (char*)res + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);
				char *p_src = (char*)scratch + meta_res->fixed_block +
					meta_res->fixed_offset[j - op.p1] + stride * (aligned_start + threadIdx.x);

				switch(stride) {
					case 4:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 8:
						((int*)p)[0] = ((int*)p_src)[0];
						break;
					case 1:
						if(threadIdx.x < VIRG_THREADSPERBLOCK / 4)
							((int*)p)[0] = ((int*)p_src)[0];
						break;
				}
			}
		}
	} // if we are using mapped memory


}


/**
 * @ingroup vm
 * @brief CUDA virtual machine kernel
 *
 * This function executes a virtual machine context on a data tablet in
 * parallel. Opcodes are are accessed using a switch statement, since there is
 * no support for indirect jumping on current NVIDIA hardware.
 *
 * @param tab_slot		The GPU constant memory slot containing the data
 * tablet's meta information
 * @param res_slot		The GPU constant memory slot containing the result
 * tablet's meta information
 * @param tab_			Pointer to the data tablet
 * @param res_			Pointer to the result tablet
 * @param start_row		The row at which to start processing the data tablet
 * @param num_rows		The number of rows to process from the data tablet, 0
 * for as many as possible
 * @param scratch		Buffer tablet used to store intermediate results in
 * global memory before moving them to mapped main memory, only set for mapped
 * memory execution
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
__global__ void virginia_gpu(
	unsigned tab_slot,
	unsigned res_slot,
	void* tab_,
	void* res_,
	unsigned start_row,
	unsigned num_rows,
	void *scratch)
{
	// tablet pointers
	virg_tablet_meta *res = (virg_tablet_meta*)res_;
	virg_tablet_meta *meta_tab = &meta[tab_slot];
	virg_tablet_meta *meta_res = &meta[res_slot];


	// misc kernel variables
	unsigned pc 		= vm.pc;
	unsigned pc_wait 	= 0;
	int	valid 			= 1;
	unsigned row		= blockIdx.x * blockDim.x + threadIdx.x;
	if(threadIdx.x == 0)
		block = 0;
	virg_vm_context	context;

	// if we've reached the end of the data tablet or the number of rows we're
	// supposed to process in this kernel launch then this row is not valid,
	// otherwise go to the row calculated with the thread id and block id
	if(row >= meta_tab->rows || (row >= num_rows && num_rows != 0))
		valid = 0;
	else
		row += start_row;

//	int op = vm.stmt[pc].op;
//	__asm(".global .u32 jmptbl[2] = {op_Column, op_Integer};");
//	__asm("bra %%op, jmptbl;");
	while(1)
	{
		// if this thread has diverged and is waiting at a later opcode, then
		// don't switch on the current opcode
		if(pc_wait > 0)
			pc_wait--;
		// otherwise switch on the current global opcode
		else {

#define ARG (vm.stmt[pc], context, meta_tab, meta_res, tab_, res, scratch, valid, pc, pc_wait)

			switch(vm.stmt[pc].op) {
				case OP_Column	: op_Column		ARG; break;
				case OP_Rowid	: op_Rowid		ARG; break;
				case OP_Result	: op_Result		ARG; break;
				case OP_Invalid	: op_Invalid	ARG; break;
				case OP_Integer	: op_Integer	ARG; break;
				case OP_Float	: op_Float		ARG; break;
				case OP_Converge: return;
				case OP_Le		: op_Le			ARG; break;
				case OP_Lt		: op_Lt			ARG; break;
				case OP_Ge		: op_Ge			ARG; break;
				case OP_Gt		: op_Gt			ARG; break;
				case OP_Eq		: op_Eq			ARG; break;
				case OP_Neq		: op_Neq		ARG; break;
				case OP_Add 	: op_Add 		ARG; break;
				case OP_Sub		: op_Sub		ARG; break;
				case OP_Mul		: op_Mul		ARG; break;
				case OP_Div		: op_Div		ARG; break;
				case OP_And		: op_And		ARG; break;
				case OP_Or		: op_Or			ARG; break;
				case OP_Not		: op_Not		ARG; break;
				case OP_Cast	: op_Cast		ARG; break;
			}
		}

		pc++;
	}
}

