#include "virginian.h"

/**
 * @ingroup vm
 * @brief Cleanup a virtual machine
 *
 *
 * @param vm Pointer to the context struct of the virtual machine
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
void virg_vm_cleanup(virginian *v, virg_vm *vm)
{
	if(vm->head_result != NULL)
		virg_vm_freeresults(v, vm);

	for(unsigned i = 0; i < vm->num_ops; i++)
		switch(vm->stmt[i].op) {
			case OP_ResultColumn :
				free(vm->stmt[i].p4.s);
				break;
		}

	free(vm);
}

