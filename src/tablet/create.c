
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
 * This file contains the virg_tablet_create() function.
 */
        

/**
 * @ingroup tablet
 * @brief Create an empty tablet for a new table
 *
 * This function creates a brand new empty tablet with all of the default tablet
 * settings. This is called only by virg_table_create() to produce the first
 * empty tablet for a new table, most new tablets are created with
 * virg_tablet_addtail(). Columns are added later to the tablet with
 * virg_tablet_addcolumn().
 *
 * @param v     	Pointer to the state struct of the database system
 * @param key_type	Type of the primary key of the tablet
 * @param table_id	ID of the table to which this tablet will belong
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_create(virginian *v, int *id, virg_t key_type, unsigned table_id)
{
	virg_tablet_meta *meta;

	// assign tablet id
	int tablet_id = v->db.tablet_id_counter++;

	// allocate tablet slot with this id
	virg_db_alloc(v, &meta, tablet_id);

#ifdef VIRG_DEBUG
	// 0 out meta block for valgrind
	memset(meta, 0, sizeof(virg_tablet_meta));
#endif

	// set tablet meta information
	meta->rows = 0;
	meta->key_type = key_type;
	meta->key_stride = virg_sizeof(key_type);
	meta->key_pointer_stride = sizeof(size_t);
	meta->row_stride = meta->key_stride + meta->key_pointer_stride;
	meta->last_tablet = 1;
	meta->id = tablet_id;
	id[0] = tablet_id;
	if(table_id > VIRG_MAX_TABLES) {
		meta->in_table = 0;
		meta->table_id = 0;
	}
	else {
		meta->in_table = 1;
		meta->table_id = table_id;
	}

	// set block sizes according to possible rows
	meta->possible_rows = VIRG_TABLET_INITIAL_KEYS;
	meta->key_block = sizeof(virg_tablet_meta);
	meta->key_pointers_block = meta->key_block +
		meta->key_stride * VIRG_TABLET_INITIAL_KEYS;
	meta->fixed_block = meta->key_pointers_block +
		meta->key_pointer_stride * VIRG_TABLET_INITIAL_KEYS;
	meta->variable_block = meta->fixed_block + VIRG_TABLET_INITIAL_FIXED;
	meta->size = meta->variable_block + VIRG_TABLET_INITIAL_VARIABLE;

#ifdef VIRG_DEBUG
	// 0 out the rest of the tablet for valgrind
	memset(meta + meta->key_block, 0, meta->size - sizeof(virg_tablet_meta));
#endif

	meta->info = NULL;
	meta->fixed_columns = 0;

	virg_tablet_unlock(v, tablet_id);

	return VIRG_SUCCESS;
}

