
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
 * This file contains the virg_vm_addop() function.
 */
        

/**
 * @ingroup vm
 * @brief Add an op to a virtual machine's statement
 *
 * Copies the arguments into the statement of the virtual machine as a new
 * operation, incrementing the total number of operations.
 *
 * @param vm	Pointer to the context struct of the virtual machine
 * @param op	The opcode of the new operation
 * @param p1	Argument 1 of the new operation
 * @param p2	Argument 2 of the new operation
 * @param p3	Argument 3 of the new operation
 * @param p4	Argument 4, the union argument, of the new operation
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_addop(virg_vm *vm, int op, int p1, int p2, int p3, virg_var p4)
{
	VIRG_DEBUG_CHECK(vm->num_ops + 1 >= VIRG_OPS, "Too many ops")
	vm->stmt[vm->num_ops].op = op;
	vm->stmt[vm->num_ops].p1 = p1;
	vm->stmt[vm->num_ops].p2 = p2;
	vm->stmt[vm->num_ops].p3 = p3;
	vm->stmt[vm->num_ops].p4 = p4;
	vm->num_ops++;

	return VIRG_SUCCESS;
}

