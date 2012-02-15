#include "virginian.h"


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

