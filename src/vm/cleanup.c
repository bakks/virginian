
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
 * This file contains the virg_vm_cleanup() function.
 */
        

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

