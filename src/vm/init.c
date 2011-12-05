
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
 * This file contains the virg_vm_init() function.
 */
        

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

