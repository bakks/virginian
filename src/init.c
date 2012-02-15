#include "virginian.h"

/**
 * @ingroup virginian
 * @brief Initializes the Virginian database
 *
 * Initializes or re-initializes the struct that holds the
 * state of the database. It sets a number of options via defaults hard-coded or
 * defined in virginian.h. Additionally, this function is responsible for
 * allocating the tablet memory areas in both main memory and GPU memory. If the
 * VIRG_DEBUG macro is defined, both the state struct and the tablet memory
 * areas are set to 0xDEADBEEF. Finally, this function also initializes the CUDA
 * context. The allocations made in this function are freed with virg_close().
 *
 * @param v Pointer to the state struct of the database system
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_init(virginian *v)
{
	int i;

	cudaError_t r;

#ifdef VIRG_DEBUG
	// zero out db struct for valgrind
	memset(&v->db, 0xDEADBEEF, sizeof(virg_db));
#endif

	// set struct defaults
	v->tablet_slot_counter = 0;
	v->tablet_slots_taken = 0;
	v->threads_per_block = VIRG_THREADSPERBLOCK;
	v->multi_threads = VIRG_MULTITHREADS;
	v->use_multi = 0;
	v->use_gpu = 0;
	v->use_stream = 0;
	v->use_mmap = 0;
	v->dbfd = -1;

	// init mutex for locking tablet slots
	VIRG_CHECK(pthread_mutex_init(&v->slot_lock, NULL), "Could not init mutex")

	// set proper flags, first one enables mapped memory
	cudaSetDeviceFlags(cudaDeviceMapHost | cudaDeviceScheduleSpin | cudaDeviceScheduleBlockingSync);
	VIRG_CUDCHK("set device flags");

	// initialize cuda context
	cudaSetDevice(VIRG_CUDADEVICE);
	VIRG_CUDCHK("set device");

	// initialize tablets
	for(i = 0; i < VIRG_MEM_TABLETS; i++) {
		v->tablet_slot_status[i] = 0;

		// if pinned, then use cuda alloc
#ifndef VIRG_NOPINNED
		r = cudaHostAlloc((void**)&v->tablet_slots[i], VIRG_TABLET_SIZE, cudaHostAllocMapped);
		VIRG_CUDCHK("Allocating pinned tablet memory");
#else
		v->tablet_slots[i] = malloc(VIRG_TABLET_SIZE);
		VIRG_CHECK(v->tablet_slots[i] == NULL, "Problem allocating tablet memory");
#endif

#ifdef VIRG_DEBUG
		// this should only be read when status is nonzero
		v->tablet_slot_ids[i] = 0;

		// zero out memory for debugging
		memset(v->tablet_slots[i], 0xDEADBEEF, VIRG_TABLET_SIZE);
#endif
	}

	if(VIRG_GPU_TABLETS > 0) {
		// initialize gpu tablets as a single block
		r = cudaMalloc((void**)&v->gpu_slots, VIRG_TABLET_SIZE * VIRG_GPU_TABLETS);
		VIRG_CHECK(r != cudaSuccess, "Problem allocating GPU tablet memory");
#ifdef VIRG_DEBUG
		cudaMemset(v->gpu_slots, 0xDEADBEEF, VIRG_TABLET_SIZE * VIRG_GPU_TABLETS);
#endif
	}

	return VIRG_SUCCESS;
}

