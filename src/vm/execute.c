
#include "virginian.h"

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
 * This file contains the virg_vm_execute() function.
 */
        

/**
 * @ingroup vm
 * @brief Execute the virtual machine using its stored statement
 *
 * Execute the opcodes that have been stored in the passed virtual machine
 * struct, choosing the execution location based on the options set in the
 * virginian struct. This function executes the top-level opcodes that must be
 * handled serially. Like the CPU virtual machine, this function uses a jump
 * table to access code blocks with opcodes, rather than a switch statement. The
 * jump table is simply an array of label addresses that is accessed using the
 * integer value of each opcode, then jumped to with a goto. This function is
 * also responsible for fetching the first data tablet of each loaded table and
 * allocating the first result tablet, then releasing the final locks on the
 * data and result tablet pointers at the end of the query, probably after the
 * pointers have been altered by the lower-level execution functions.
 * virg_vm_gpu() or virg_vm_cpu() is chosen based on the options in the
 * virginian struct, and there is currently no capability to handle both
 * simultaneously, as this would probably involve a more complex threading
 * system.
 *
 * @param v     Pointer to the state struct of the database system
 * @param vm	Pointer to the context struct of the virtual machine
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_execute(virginian *v, virg_vm *vm)
{
	// label jump table
	static void *jump[] = { &&op_Table, &&op_ResultColumn, &&op_Parallel, &&op_Finish,
		&&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP,
		&&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP, &&NOP,
		&&NOP };

	int p1, p2, p3;
	virg_tablet_meta *tab, *res;

	vm->num_tables = 0;
	vm->pc = 0;
	vm->head_result = NULL;

	// get a new result tablet
	virg_vm_allocresult(v, vm, &res, NULL);

next:
	// load the first 3 arguments automatically for convenience
	p1 = vm->stmt[vm->pc].p1;
	p2 = vm->stmt[vm->pc].p2;
	p3 = vm->stmt[vm->pc].p3;

#ifdef VIRG_DEBUG
	// print the op and its arguments
	//fprintf(stderr, "<OP>  %-*s%*i%*i%*i\n", 14,
	//	virg_opstring(vm->stmt[vm->pc].op), 5, p1, 5, p2, 5, p3);
#endif

	// jump to the label in the jump table indexed with the integer value of the
	// current opcode
	goto *jump[vm->stmt[vm->pc].op];

op_Table:
	// check that we haven't loaded too many tables
	assert(vm->num_tables < VIRG_VM_TABLES);
	// load the table into the first available table slot
	vm->table[vm->num_tables++] = p1;
	// get a lock on the first tablet of the loaded table
	virg_db_load(v, v->db.first_tablet[p1], &tab);
	vm->pc++;
	goto next;

op_ResultColumn: // type
	virg_tablet_addcolumn(res, vm->stmt[vm->pc].p4.s, (virg_t)p1);
	vm->pc++;
	goto next;

op_Parallel:
	// make the result tablet as large as possible given the columns that have
	// been added to it
	virg_tablet_addmaxrows(v, res);
	// start the data parallel section on the next opcode
	vm->pc++;
	// choose the execution location based on the properties of the virginian
	// state struct
	if(v->use_gpu)
		virg_vm_gpu(v, vm, &tab, &res, 0);
	else
		virg_vm_cpu(v, vm, &tab, &res, 0);
	vm->pc = p3;
	goto next;

op_Finish:
	// unlock our hold on the current data and result tablets
	virg_tablet_unlock(v, tab->id);
	virg_tablet_unlock(v, res->id);
	return VIRG_SUCCESS;

// opcode problem that resulted in a weird pc
NOP:
	fprintf(stderr, "Invalid OP\n");
	return VIRG_FAIL;
}

