
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
 * This file contains the virg_vm_allocresult() function.
 */
        

/**
 * @ingroup vm
 * @brief Allocate and create a new result tablet or copy an existing tablet
 *
 * Creates a new result tablet for use with the passed virtual machine. If the
 * template argument is NULL, then a completely new result tablet with default
 * settings will be create, otherwise the template will be duplicated with
 * minimal changes for the new tablet. This function also handles adding this
 * tablet to the virtual machine's linked list of result tablets.
 *
 * @param v     Pointer to the state struct of the database system
 * @param vm	Pointer to the context structure of the virtual machine
 * @param meta	Pointer to a tablet pointer through which the new tablet will be
 * returned
 * @param template Pointer to an existing result tablet, whose meta settings
 * will be duplicated
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_allocresult(virginian *v, virg_vm *vm, virg_tablet_meta **meta, virg_tablet_meta *template_)
{
	virg_tablet_meta *tab;

	// get a new tablet slot using new id
	unsigned id = v->db.tablet_id_counter++;
	virg_db_alloc(v, &tab, id);

#ifdef VIRG_DEBUG
	memset((char*)tab + sizeof(virg_tablet_meta), 0xDEADBEEF,
		VIRG_TABLET_SIZE - sizeof(virg_tablet_meta));
#endif

	// if we don't have a result tablet to copy set attributes to defaults
	if(template_ == NULL) {
		tab->key_type = VIRG_INT;
		tab->key_stride = virg_sizeof(VIRG_INT);
		tab->last_tablet = 1;
		tab->key_pointer_stride = sizeof(size_t);
		tab->key_block = sizeof(virg_tablet_meta);
		tab->possible_rows = 0;
		tab->key_pointers_block = tab->key_block +
			tab->possible_rows * tab->key_stride;
		tab->fixed_block = tab->key_pointers_block +
			tab->possible_rows * tab->key_pointer_stride;
		tab->variable_block = tab->fixed_block;
		tab->size = tab->variable_block;
		tab->row_stride = tab->key_stride + tab->key_pointer_stride;
		tab->fixed_columns = 0;
		tab->info = NULL;
		tab->in_table = 0;
	}
	// otherwise copy everything from the previous result tablet and set only
	// what we need
	else {
		memcpy(tab, template_, sizeof(virg_tablet_meta));
		template_->last_tablet = 0;
		tab->id = id;
		template_->next = tab->id;
	}

	tab->rows = 0;
	tab->next = 0;

	// allocate a new node for the linked list of results
	virg_result_node *node = malloc(sizeof(virg_result_node));
	VIRG_CHECK(node == NULL, "Out of memory")

	node->id = id;
	node->next = NULL;

	// add newly allocated tablet to the end of the linked list
	if(vm->head_result == NULL)
		vm->head_result = node;
	else
		vm->tail_result->next = node;
	vm->tail_result = node;
	
	// return pointer to new result tablet
	meta[0] = tab;

	return VIRG_SUCCESS;
}

