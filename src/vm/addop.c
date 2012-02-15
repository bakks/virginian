#include "virginian.h"

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

