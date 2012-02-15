#include "virginian.h"

/**
 * @file
 * @section Description 
 *
 * This file contains both the virginia_single() and virginia_multi() functions.
 * If the __SINGLE macro is set, the former will be compiled from this file,
 * otherwise the latter. It is constructed this way because these two functions
 * share most of their code, but the multi-core version must be thread-safe and
 * handles getting a new data tablet to process itself rather than returning.
 * Additionally the function arguments are all stored in a single struct in the
 * multi-core version because pthread_create() can only pass a single argument,
 * so these values must be unpacked to variables.
 */
        

// ensure macros make sense
#ifndef __SINGLE
	#define __MULTI
#else
	#undef __MULTI
#endif

/**
 * Convenience macro to load the 1st argument of the current op into variable p1
 */
#define GETP1 p1 = vm->stmt[context.pc].p1;

/**
 * Convenience macro to load the 2nd argument of the current op into variable p2
 */
#define GETP2 p2 = vm->stmt[context.pc].p2;

/**
 * Convenience macro to load the 3rd argument of the current op into variable p3
 */
#define GETP3 p3 = vm->stmt[context.pc].p3;

/**
 * This is a convenience macro for the opcodes that change the program counter
 * based on a comparison between two registers, such as Ge and Lt. I've written
 * it this way because this is the only way to handle switching the operator in
 * an equation at compile time, and I did not want to duplicate this code for 5
 * or 6 different ops. It is intended to be called like REGCMP(<=) for a less
 * than or equal comparison.
 */
#define REGCMP(op) {														   \
	GETP1																	   \
	GETP2																	   \
	GETP3																	   \
	assert(context.type[p1] == context.type[p2]);							   \
																			   \
	switch(context.type[p1]) {												   \
		case VIRG_INT:														   \
			for(i = 0; i < simd_rows; i++) {								   \
				if(context.row_pc[i] == context.pc) {						   \
					int x = (context.reg[p1].i[i] op context.reg[p2].i[i]);	   \
					if(x) {													   \
						if(valid[i])										   \
							valid[i] = vm->stmt[context.pc].p4.i;			   \
						context.row_pc[i] = p3;								   \
					}														   \
					else													   \
						context.row_pc[i]++;								   \
				}															   \
			}																   \
			break;															   \
		case VIRG_FLOAT:													   \
			for(i = 0; i < simd_rows; i++) {								   \
				if(context.row_pc[i] == context.pc) {						   \
					int x = (context.reg[p1].f[i] op context.reg[p2].f[i]);	   \
					if(x) {													   \
						if(valid[i])										   \
							valid[i] = vm->stmt[context.pc].p4.i;			   \
						context.row_pc[i] = p3;								   \
					}														   \
					else													   \
						context.row_pc[i]++;								   \
				}															   \
			}																   \
			break;															   \
		case VIRG_INT64:													   \
			for(i = 0; i < simd_rows; i++) {								   \
				if(context.row_pc[i] == context.pc) {						   \
					int x = (context.reg[p1].li[i] op context.reg[p2].li[i]);  \
					if(x) {													   \
						if(valid[i])										   \
							valid[i] = vm->stmt[context.pc].p4.i;			   \
						context.row_pc[i] = p3;								   \
					}														   \
					else													   \
						context.row_pc[i]++;								   \
				}															   \
			}																   \
			break;															   \
		case VIRG_DOUBLE:													   \
			for(i = 0; i < simd_rows; i++) {								   \
				if(context.row_pc[i] == context.pc) {						   \
					int x = (context.reg[p1].d[i] op context.reg[p2].d[i]);	   \
					if(x) {													   \
						if(valid[i])										   \
							valid[i] = vm->stmt[context.pc].p4.i;			   \
						context.row_pc[i] = p3;								   \
					}														   \
					else													   \
						context.row_pc[i]++;								   \
				}															   \
			}																   \
			break;															   \
		case VIRG_CHAR:														   \
			for(i = 0; i < simd_rows; i++) {								   \
				if(context.row_pc[i] == context.pc) {						   \
					int x = (context.reg[p1].c[i] op context.reg[p2].c[i]);	   \
					if(x) {													   \
						if(valid[i])										   \
							valid[i] = vm->stmt[context.pc].p4.i;			   \
						context.row_pc[i] = p3;								   \
					}														   \
					else													   \
						context.row_pc[i]++;								   \
				}															   \
			}																   \
			break;															   \
		default:															   \
			assert(0);														   \
			break;															   \
	}																		   \
	context.pc++; }															   \
	goto next;


/**
 * This is a convenience macro for opcodes such as Add and Mul to handle
 * switching the math operator in this block of code so that it doesn't need to
 * be duplicated. It is intended to be called like MATHOP(+).
 */
#define MATHOP(op) 															   \
{																			   \
	GETP1																	   \
	GETP2																	   \
	GETP3																	   \
	assert(context.type[p2] == context.type[p3]);							   \
	context.type[p1] = context.type[p2];									   \
	context.stride[p1] = context.stride[p2];								   \
																			   \
	switch(context.type[p1]) {												   \
		case VIRG_INT:														   \
			for(i = 0; i < simd_rows; i++)									   \
				if(context.row_pc[i] == context.pc) {						   \
					context.reg[p1].i[i] = 									   \
						context.reg[p2].i[i] op context.reg[p3].i[i];		   \
					context.row_pc[i]++;									   \
				}															   \
			break;															   \
		case VIRG_FLOAT:													   \
			for(i = 0; i < simd_rows; i++)									   \
				if(context.row_pc[i] == context.pc) {						   \
					context.reg[p1].f[i] = 									   \
						context.reg[p2].f[i] op context.reg[p3].f[i];		   \
					context.row_pc[i]++;									   \
				}															   \
			break;															   \
		case VIRG_INT64:													   \
			for(i = 0; i < simd_rows; i++)									   \
				if(context.row_pc[i] == context.pc) {						   \
					context.reg[p1].li[i] =									   \
						context.reg[p2].li[i] op context.reg[p3].li[i];		   \
					context.row_pc[i]++;									   \
				}															   \
			break;															   \
		case VIRG_DOUBLE:													   \
			for(i = 0; i < simd_rows; i++)									   \
				if(context.row_pc[i] == context.pc) {						   \
					context.reg[p1].d[i] =									   \
						context.reg[p2].d[i] op context.reg[p3].d[i];		   \
					context.row_pc[i]++;									   \
				}															   \
			break;															   \
		case VIRG_CHAR:														   \
			for(i = 0; i < simd_rows; i++)									   \
				if(context.row_pc[i] == context.pc) {						   \
					context.reg[p1].c[i] =									   \
						context.reg[p2].c[i] op context.reg[p3].c[i];		   \
					context.row_pc[i]++;									   \
				}															   \
			break;															   \
		default:															   \
			assert(0);														   \
			break;															   \
	}																		   \
	context.pc++;															   \
	goto next;																   \
}



/**
 * @ingroup vm
 * @brief Execute the data-parallel portion of an opcode program on a single
 * core
 *
 * This function executes the opcodes inbetween Parallel and Converge. The
 * single core version is assigned a tablet, and executes the opcodes on up to
 * num_rows in that tablet, or all if num_rows is 0.
 *
 * Like virg_vm_execute(), virginia_single() and virginia_multi() use a jump
 * table rather than a switch to access the code for each opcode. This is
 * accomplished with an array of label addresses, which is accessed with the
 * value of each opcode, then jumped to with a goto.
 *
 * The CPU virtual machine is written with tight inner loops to process blocks
 * of rows at the same time, which we'll call a SIMD block. Since both cells in
 * a column, and cells in a SIMD virtual machine register are stored adjacent to
 * each other, this allows for efficient cache locality and direct memcpy
 * operations.
 *
 * @param v     Pointer to the state struct of the database system
 * @param vm	Pointer to the context struct of the virtual machine
 * @param tab	Pointer to the current data tablet to process
 * @param res	Pointer to the pointer to the current result tablet
 * @param row	The row to begin processing the current data tablet
 * @param num_rows The number of rows to process in this function, 0 for as many
 * as possible
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
void virginia_single(virginian *v, virg_vm *vm, virg_tablet_meta *tab,
	virg_tablet_meta **res_, unsigned row, unsigned num_rows);

#ifdef __SINGLE

void virginia_single(virginian *v, virg_vm *vm, virg_tablet_meta *tab,
	virg_tablet_meta **res_, unsigned row, unsigned num_rows)
{

#else

/**
 * @ingroup vm
 * @brief Execute the data-parallel portion of an opcode program on multiple
 * cores
 *
 * This function is called by pthread_create() to run as an independent thread,
 * which greedily processes data until there is none left. The multi-core
 * version includes mutexes to protect accesses to data and result tablets.
 *
 * Like virg_vm_execute(), virginia_single() and virginia_multi() use a jump
 * table rather than a switch to access the code for each opcode. This is
 * accomplished with an array of label addresses, which is accessed with the
 * value of each opcode, then jumped to with a goto.
 *
 * The CPU virtual machine is written with tight inner loops to process blocks
 * of rows at the same time, which we'll call a SIMD block. Since both cells in
 * a column, and cells in a SIMD virtual machine register are stored adjacent to
 * each other, this allows for efficient cache locality and direct memcpy
 * operations.
 *
 * @param arg A struct containing all of the arguments for this function
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
void *virginia_multi(void *arg_)
{
	// unpack arguments from the passed struct
	virg_vm_arg *arg = (virg_vm_arg*) arg_;
	virginian *v = arg->v;
	virg_vm *vm = arg->vm;
	virg_tablet_meta *tab = NULL;
	virg_tablet_meta **res_ = &arg->res;
	unsigned row;
	unsigned num_rows = arg->num_rows;
#endif

	virg_tablet_meta *res = res_[0];

	virg_vm_simdcontext context;
	unsigned i;
	int j;
	int p1 = 0, p2 = 0, p3 = 0;
	void *ptr1, *ptr2;
	int valid[VIRG_CPU_SIMD];
	unsigned last_row;
	unsigned simd_rows;
	unsigned total_valid;

	// jump table for opcodes handled in this function
	static void *jump[] = { &&NOP, &&NOP, &&NOP, &&NOP,
		&&op_Column, &&op_Rowid, &&op_Result, &&op_Converge, &&op_Invalid, &&op_Cast,
		&&op_Integer, &&op_Float, &&op_Le, &&op_Lt, &&op_Ge, &&op_Gt, &&op_Eq,
		&&op_Neq, &&op_Add, &&op_Sub, &&op_Mul, &&op_Div, &&op_And, &&op_Or,
		&&op_Not };

#ifdef __MULTI
	virg_tablet_lock(v, res->id);
#endif

	while(1) {
#ifdef __MULTI 
		/**
		 * This multi-core version assigns data to process in a greedy way. If
		 * the thread is just starting or if the all its data has been
		 * processed, it locks the tablet pointer and gets a new block, or
		 * entire tablet to process, then updates this pointer. Thus, all
		 * threads execute until the data runs out and they are never idle
		 */
		pthread_mutex_lock(&arg->tab_lock);
		// if the current row position is at the end of the current tablet
		if(arg->row >= arg->tab->rows) {
			// if this is the last tablet
			if(arg->tab->last_tablet) {
				// unlock data and result tablets and exit
				pthread_mutex_unlock(&arg->tab_lock);
				if(tab != NULL)
					virg_tablet_unlock(v, tab->id);
				virg_tablet_unlock(v, res->id);
				pthread_exit(NULL);
			}

			// if we haven't yet locked a tablet in this thread
			if(tab != NULL)
				virg_tablet_unlock(v, tab->id);
			virg_db_loadnext(v, &arg->tab);
			virg_tablet_lock(v, arg->tab->id);
			tab = arg->tab;
			arg->row = 0;
		}

		// if we are just starting and no tablet has yet been assigned
		if(tab == NULL) {
			virg_tablet_lock(v, arg->tab->id);
			tab = arg->tab;
		}

		// set the local row cursor to the global row cursor
		row = arg->row;
#endif

		// only process num_row rows, or as many as possible if num_rows is 0
		if(num_rows == 0)
			last_row = tab->rows;
		else
			last_row = VIRG_MIN(row + num_rows, tab->rows);

#ifdef __MULTI
		// update global row cursor
		arg->row = last_row;
		pthread_mutex_unlock(&arg->tab_lock);
#endif

		// while we haven't yet finished our row allocation
		while(row < last_row) {
			context.pc = vm->pc;

			// we want to process VIRG_CPU_SIMD at a time to cache effectively,
			// but we might be at the end of the tablet and not have a full
			// VIRG_CPU_SIMD rows left
			simd_rows = VIRG_MIN(VIRG_CPU_SIMD, last_row - row);

			// set every row as still valid with a pc equal to the global pc
			for(i = 0; i < simd_rows; i++) {
				context.row_pc[i] = vm->pc;
				valid[i] = 1;
			}
			// set every row between simd_rows and VIRG_CPU_SIMD to invalid
			for( ; i < VIRG_CPU_SIMD; i++)
				valid[i] = 0;


//############################################################################
//############################################################################

next:
	// check that the row program counters make sense compared to the global pc
	for(i = 0; i < simd_rows; i++) {
		assert(context.row_pc[i] >= context.pc);
	}
	// jump to the next opcode using the jump table indexed by the opcode value
	goto *jump[vm->stmt[context.pc].op];

op_Converge:
	// go to the next row block
	row += VIRG_CPU_SIMD;
	continue;

op_Integer:
	GETP1
	GETP2
	for(i = 0; i < simd_rows; i++) {
		if(context.row_pc[i] == context.pc) {
			context.reg[p1].i[i] = p2;
			context.row_pc[i]++;
		}
	}
	context.type[p1] = VIRG_INT;
	context.stride[p1] = sizeof(int);
	context.pc++;
	goto next;

op_Float:
	GETP1
	GETP2
	for(i = 0; i < simd_rows; i++) {
		if(context.row_pc[i] == context.pc) {
			context.reg[p1].f[i] = vm->stmt[context.pc].p4.f;
			context.row_pc[i]++;
		}
	}
	context.type[p1] = VIRG_FLOAT;
	context.stride[p1] = sizeof(float);
	context.pc++;
	goto next;

op_Invalid:
	for(i = 0; i < simd_rows; i++) {
		if(context.row_pc[i] == context.pc) {
			valid[i] = 0;
			context.row_pc[i]++;
		}
	}
	context.pc++;
	goto next;

op_Le:
	REGCMP(<=);

op_Lt:
	REGCMP(<);

op_Ge:
	REGCMP(>=);

op_Gt:
	REGCMP(>);

op_Eq:	// col 1, col 2, jmp location, 0: invalid if jmp
	REGCMP(==);

op_Neq:
	REGCMP(!=);

op_Column: // dest reg, src col
	GETP1
	GETP2
	ptr1 = (char*)tab + tab->fixed_block + tab->fixed_offset[p2] + tab->fixed_stride[p2] * row;

	// copy column segment directly into the register struct
	memcpy(&context.reg[p1], ptr1, tab->fixed_stride[p2] * simd_rows);
	context.type[p1] = tab->fixed_type[p2];
	context.stride[p1] = tab->fixed_stride[p2];

	// update row pcs
	for(i = 0; i < simd_rows; i++)
		if(context.row_pc[i] == context.pc)
			context.row_pc[i]++;
	context.pc++;
	goto next;

op_Rowid: // dest reg,       key ptr?
	GETP1
	GETP2
	ptr1 = (char*)tab + tab->key_block + tab->key_stride * row;

	// copy column segment directly into the register struct
	memcpy(&context.reg[p1], ptr1, tab->key_stride * simd_rows);
	context.type[p1] = tab->key_type;
	context.stride[p1] = tab->key_stride;

	// update row pcs
	for(i = 0; i < simd_rows; i++)
		if(context.row_pc[i] == context.pc)
			context.row_pc[i]++;
	context.pc++;
	goto next;

op_Result: // start reg, num regs

	total_valid = 0;
	for(i = 0; i < simd_rows; i++)
		if(valid[i])
			total_valid++;

#ifdef __MULTI
	/**
	 * All threads share a result tablet and lock a mutex on it to get a block
	 * on the tablet where they can place their result rows.
	 */
	pthread_mutex_lock(&arg->res_lock);

	// if the result tablet is full
	if(res->rows + simd_rows >= res->possible_rows) {
		// if the local result tablet is the global result tablet
		if(res == arg->res) {
			// unlock current tablet and allocate another one
			virg_tablet_unlock(v, res->id);
			virg_tablet_unlock(v, res->id);
			virg_vm_allocresult(v, vm, &arg->res, arg->res);
			virg_tablet_lock(v, arg->res->id);
		}
		else {
			// otherwise move to the current global result tablet
			virg_tablet_unlock(v, res->id);
			virg_tablet_lock(v, arg->res->id);
		}
		res = arg->res;
	}

	// allocate area
	unsigned write_row = res->rows;
	res->rows += total_valid;

	pthread_mutex_unlock(&arg->res_lock);

#else

	// check if theres still room in the result tablet and allocate a new one if
	// not
	if(res->rows + simd_rows >= res->possible_rows - 300) {
		virg_tablet_unlock(v, res->id);
		virg_tablet_unlock(v, res->id);
		virg_vm_allocresult(v, vm, &res, res);
		res_[0] = res;
		virg_tablet_lock(v, res->id);
	}

	unsigned write_row = res->rows;
	res->rows += total_valid;
#endif
	
	/**
	 * Outputting result rows is done through memcpy operations, and we make an
	 * effort to minimize the number of these calls. Thus, for every register
	 * being output, we loop over every one of the SIMD rows (which are adjacent
	 * in memory) and lazily perform a copy only when we encounter a row that
	 * will not be output to the result block.
	 */
	unsigned block_start = 0;
	GETP1
	GETP2

	unsigned write_start = write_row;

	// for all the registers in the op
	for(j = p1; j < p1 + p2; j++) {
		unsigned stride = context.stride[j];
		unsigned block_size = 0;
		write_row = write_start;

		// for each valid row in simd width
		for(i = 0; i < simd_rows; i++) {
			// if this is a row we want to output
			if(valid[i]) {
				// increase the size of the block that we will eventually copy
				if(block_size == 0)
					block_start = i;
				block_size++;
			}
			// otherwise copy everything in the block up to this point, except
			// if the block is of size 0
			else if(block_size > 0) {
				ptr1 = (char*)res + res->fixed_block + res->fixed_offset[j - p1] + stride * write_row;
				ptr2 = (char*)&context.reg[j] + stride * block_start;
				memcpy(ptr1, ptr2, block_size * stride);
				write_row += block_size;
				block_size = 0;
			}
		}

		// finish off by copying the last block
		ptr1 = (char*)res + res->fixed_block + res->fixed_offset[j - p1] + stride * write_row;
		ptr2 = (char*)&context.reg[j] + stride * block_start;
		memcpy(ptr1, ptr2, block_size * stride);
	}

	for(i = 0; i < simd_rows; i++)
		context.row_pc[i]++;
	context.pc++;
	goto next;

op_Add:
	MATHOP(+);

op_Sub:
	MATHOP(-);

op_Mul:
	MATHOP(*);

op_Div:
	MATHOP(/);

op_And:
	REGCMP(&&);

op_Or:
	REGCMP(||);

op_Not:
{
	GETP1
	GETP3

	switch(context.type[p1]) {
		case VIRG_INT:
			for(i = 0; i < simd_rows; i++) {
				if(context.row_pc[i] == context.pc) {
					int x = context.reg[p1].i[i];
					if(!x) {
						if(valid[i])
							valid[i] = vm->stmt[context.pc].p4.i;
						context.row_pc[i] = p3;
					}
					else
						context.row_pc[i]++;
				}
			}
			break;
		case VIRG_FLOAT:
			for(i = 0; i < simd_rows; i++) {
				if(context.row_pc[i] == context.pc) {
					float x = context.reg[p1].f[i];
					if(!x) {
						if(valid[i])
							valid[i] = vm->stmt[context.pc].p4.i;
						context.row_pc[i] = p3;
					}
					else
						context.row_pc[i]++;
				}
			}
			break;
		case VIRG_INT64:
			for(i = 0; i < simd_rows; i++) {
				if(context.row_pc[i] == context.pc) {
					long long int x = context.reg[p1].li[i];
					if(!x) {
						if(valid[i])
							valid[i] = vm->stmt[context.pc].p4.i;
						context.row_pc[i] = p3;
					}
					else
						context.row_pc[i]++;
				}
			}
			break;
		case VIRG_DOUBLE:
			for(i = 0; i < simd_rows; i++) {
				if(context.row_pc[i] == context.pc) {
					double x = context.reg[p1].d[i];
					if(!x) {
						if(valid[i])
							valid[i] = vm->stmt[context.pc].p4.i;
						context.row_pc[i] = p3;
					}
					else
						context.row_pc[i]++;
				}
			}
			break;
		case VIRG_CHAR:
			for(i = 0; i < simd_rows; i++) {
				if(context.row_pc[i] == context.pc) {
					char x = context.reg[p1].c[i];
					if(!x) {
						if(valid[i])
							valid[i] = vm->stmt[context.pc].p4.i;
						context.row_pc[i] = p3;
					}
					else
						context.row_pc[i]++;
				}
			}
			break;
		default:
			assert(0);
			break;
	}
	context.pc++;
	goto next;
}

op_Cast: // dest type, reg
	switch(p1) {
		case VIRG_INT:
			switch(context.type[p2]) {
				case VIRG_FLOAT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].i[i] = (int) context.reg[p2].f[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_INT64:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].i[i] = (int) context.reg[p2].li[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_DOUBLE:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].i[i] = (int) context.reg[p2].d[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_CHAR:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].i[i] = (int) context.reg[p2].c[i];
							context.row_pc[i]++;
						}
					break;
				default:
					break;
			}
			context.stride[p2] = sizeof(int);
			break;
		case VIRG_FLOAT:
			switch(context.type[p2]) {
				case VIRG_INT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].f[i] = (float) context.reg[p2].i[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_INT64:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].f[i] = (float) context.reg[p2].li[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_DOUBLE:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].f[i] = (float) context.reg[p2].d[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_CHAR:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].f[i] = (float) context.reg[p2].c[i];
							context.row_pc[i]++;
						}
					break;
				default:
					break;
			}
			context.stride[p2] = sizeof(float);
			break;
		case VIRG_INT64:
			switch(context.type[p2]) {
				case VIRG_INT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].li[i] = (long long int) context.reg[p2].i[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_FLOAT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].li[i] = (long long int) context.reg[p2].f[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_DOUBLE:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].li[i] = (long long int) context.reg[p2].d[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_CHAR:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].li[i] = (long long int) context.reg[p2].c[i];
							context.row_pc[i]++;
						}
					break;
				default:
					break;
			}
			context.stride[p2] = sizeof(long long int);
			break;
		case VIRG_DOUBLE:
			switch(context.type[p2]) {
				case VIRG_INT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].d[i] = (double) context.reg[p2].i[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_FLOAT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].d[i] = (double) context.reg[p2].f[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_INT64:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].d[i] = (double) context.reg[p2].li[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_CHAR:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].d[i] = (double) context.reg[p2].c[i];
							context.row_pc[i]++;
						}
					break;
				default:
					break;
			}
			context.stride[p2] = sizeof(double);
			break;
		case VIRG_CHAR:
			switch(context.type[p2]) {
				case VIRG_INT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].c[i] = (char) context.reg[p2].i[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_FLOAT:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].c[i] = (char) context.reg[p2].f[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_INT64:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].c[i] = (char) context.reg[p2].li[i];
							context.row_pc[i]++;
						}
					break;
				case VIRG_DOUBLE:
					for(i = 0; i < simd_rows; i++)
						if(context.row_pc[i] <= context.pc) {
							context.reg[p2].c[i] = (char) context.reg[p2].c[i];
							context.row_pc[i]++;
						}
					break;
				default:
					break;
			}
			context.stride[p2] = sizeof(char);
			break;
	}
	context.type[p2] = (virg_t)p1;
	context.pc++;
	goto next;

// op for an incorrect jump location in the jump table
NOP:
	fprintf(stderr, "Invalid OP  %u\n", vm->stmt[context.pc].op);
#ifdef __SINGLE
	return;
#else
	pthread_exit(NULL);
#endif


//############################################################################
//############################################################################

		} // while(row < last_row)

// if this is single threaded, we break, otherwise we continue the loop
// indefinitely and check if we are done processing data in its middle
#ifdef __SINGLE
		break;
#endif

	} // while(1)


	// single threaded version unlocks it data and result tablets to finish
	virg_tablet_unlock(v, tab->id);
	virg_tablet_unlock(v, res->id);
}

