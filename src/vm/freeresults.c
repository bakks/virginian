
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
 * This file contains the virg_vm_freeresults() function.
 */
        

/**
 * @ingroup vm
 * @brief Initialize a result tablet reader
 *
 * Initialize a reader object to traverse the results of a query, given its
 * virtual machine state struct. This should only be called with a virg_vm
 * struct once a query that output results is run.
 *
 * @param v     Pointer to the state struct of the database system
 * @param vm	Pointer to the context struct of the virtual machine
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_freeresults(virginian *v, virg_vm *vm)
{
	// pointers for walking the list
	virg_result_node *node = vm->head_result;
	virg_result_node *next;

	while(node != NULL) {
#ifdef VIRG_DEBUG
		unsigned i;
		unsigned taken = 0;
		for(i = 0; i < VIRG_MEM_TABLETS; i++)
			if(v->tablet_slot_status[i])
				taken++;
		assert(taken == v->tablet_slots_taken);
#endif
		// delete the result tablet from its slot
		virg_tablet_remove(v, node->id);

		// free the current node and move to the next
		next = node->next;
		free(node);
		node = next;
	}

	vm->head_result = NULL;
	vm->tail_result = NULL;

	return VIRG_SUCCESS;
}

