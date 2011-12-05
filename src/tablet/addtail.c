
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
 * This file contains the virg_tablet_addtail() function.
 */
        

/**
 * @ingroup tablet
 * @brief Initialize a result tablet reader
 *
 * This function adds a new tail tablet to the tablet passed in and updates the
 * tail pointer to reflect this addition. The tail tablet is constructed by
 * copying all of the meta information then changing only what needs to be
 * changed.
 *
 * @param v     Pointer to the state struct of the database system
 * @param head	Pointer to the tablet receiving the new tail
 * @param tail	Pointer to the pointer used to manage the tail node of the
 * tablet string
 * @param possible_rows Set the initial possible rows of the tablet
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_addtail(virginian *v, virg_tablet_meta *head,
	virg_tablet_meta **tail, unsigned possible_rows)
{
	virg_tablet_meta *meta;

	// assign tablet id
	int tablet_id = v->db.tablet_id_counter++;

	// allocate a tablet slot using id
	virg_db_alloc(v, &meta, tablet_id);

	// first just copy over all meta information
	memcpy(meta, head, sizeof(virg_tablet_meta));
	head->last_tablet = 0;
	head->next = tablet_id;

	virg_tablet_unlock(v, head->id);

	// then change meta information as appropriate
	tail[0] = meta;
	meta->rows = 0;
	meta->id = tablet_id;

	meta->key_pointers_block = meta->key_block +
		meta->key_stride * possible_rows;
	meta->fixed_block = meta->key_pointers_block +
		meta->key_pointer_stride * possible_rows;

	unsigned i;
	for(i = 1; i < meta->fixed_columns; i++)
		meta->fixed_offset[i] = meta->fixed_offset[i-1] + meta->fixed_stride[i-1] * possible_rows;

	meta->variable_block = meta->key_block + meta->row_stride * possible_rows;
	meta->size = meta->variable_block + VIRG_TABLET_INITIAL_VARIABLE;

	meta->info = NULL;
	meta->possible_rows = possible_rows;

	return VIRG_SUCCESS;
}

