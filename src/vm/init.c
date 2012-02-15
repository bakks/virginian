#include "virginian.h"

/**
 * @ingroup vm
 * @brief Initialize a virtual machine context
 *
 * Allocates and sets the default values of a virtual machine context
 *
 * @param vm Pointer to the context struct of the virtual machine
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
virg_vm *virg_vm_init()
{
    virg_vm *vm = (virg_vm*)malloc(sizeof(virg_vm));

	memset(vm, 0xDEADBEEF, sizeof(virg_vm));

	vm->pc = 0;
	vm->head_result = NULL;
    vm->num_ops = 0;
    vm->num_tables = 0;
    return vm;
}    

