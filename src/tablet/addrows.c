
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
 * This file contains the virg_tablet_addrows() function.
 */
        

/**
 * @ingroup tablet
 * @brief Add fixed-size rows to a tablet
 *
 * This function handles adding a certain number of fixed-size row spaces to a
 * tablet. It is guaranteed to add this amount of row space because it will
 * continue adding tablets onto the tail of the current tablet until this goal
 * is achieved. Note that the number of possible rows in a tablet must be a
 * multiple of 16, and this function performs rounding to ensure that this is
 * the case. This function is called by virg_tablet_insert() to add a new block
 * of possible rows whenever the insert operation does not have any space to add
 * a single new row.
 *
 * @param v     Pointer to the state struct of the database system
 * @param tab	Pointer to tablet in which the rows are added
 * @param id    Number of rows to add to tablet and subsequent tablets
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_addrows(virginian *v, virg_tablet_meta *tab, unsigned rows)
{
	// TODO add resizing of variable block
	unsigned i;
	size_t row_stride = tab->row_stride;

	unsigned max_new_rows = (VIRG_TABLET_SIZE - tab->size) / row_stride;

	// round the maximum number of new rows down to a multiple of 16
	max_new_rows &= 0xFFFFFFF0;

	// round the number of rows to be added up to a multiple of 16
	rows = (rows - 1 + 16) & 0xFFFFFFF0;
	unsigned new_rows = VIRG_MIN(max_new_rows, rows);

	VIRG_DEBUG_CHECK(tab->size + row_stride * new_rows > VIRG_TABLET_SIZE, "new rows over tablet size")

	size_t new_fixed_block = tab->fixed_block +
		new_rows * (tab->key_stride + tab->key_pointer_stride);

	// if we can fit more rows into this tablet
	if(new_rows != 0) {
		virg_tablet_growfixed(tab, row_stride * new_rows);
		tab->possible_rows += new_rows;

		// generate new fixed column offsets
		size_t new_offsets[tab->fixed_columns];
		new_offsets[0] = 0;

		for(i = 1; i < tab->fixed_columns; i++)
			new_offsets[i] = new_offsets[i-1] +
				tab->fixed_stride[i-1] * tab->possible_rows;

		// move each column individually
		for(i = tab->fixed_columns-1; i < tab->fixed_columns; i--) {
			VIRG_DEBUG_CHECK((new_fixed_block + new_offsets[i] + tab->rows * tab->fixed_stride[i]) > VIRG_TABLET_SIZE, "memmove exceeds tablet limit")
			memmove((char*)tab + new_fixed_block + new_offsets[i],
				(char*)tab + tab->fixed_block + tab->fixed_offset[i],
				tab->rows * tab->fixed_stride[i]);
		}

		// copy new offsets
		tab->fixed_block = new_fixed_block;
		for(i = 0; i < tab->fixed_columns; i++)
			tab->fixed_offset[i] = new_offsets[i];

		// move key pointer block
		size_t new_key_pointers_block = tab->key_block + tab->key_stride * tab->possible_rows;
		memmove((char*)tab + new_key_pointers_block,
			(char*)tab + tab->key_pointers_block,
			tab->rows * tab->key_pointer_stride);
		tab->key_pointers_block = new_key_pointers_block;
	}

	// if we can't fit all of the new rows onto the current tablet
	if(max_new_rows <= rows) {
		virg_tablet_lock(v, tab->id);

		tab->size = VIRG_TABLET_SIZE; // max out tablet size
		unsigned rows_left = rows - new_rows;
		unsigned max_tablet_rows = (VIRG_TABLET_SIZE - 
			sizeof(virg_tablet_meta) - VIRG_TABLET_INITIAL_FIXED) / row_stride;

		virg_tablet_meta *node = tab;
		virg_tablet_meta *tail;

		// add tablets until we can fit in all the rows we need
		while(rows_left > 0) {
			unsigned r = VIRG_MIN(rows_left, max_tablet_rows);
			virg_tablet_addtail(v, node, &tail, r);
			//virg_print_slots(v);
			rows_left -= r;
			node = tail;
		}

		// if this is a data tablet, change the table to reflect the tablets
		// that have been added
		if(tab->in_table) {
			v->db.last_tablet[tab->table_id] = tail->id;
			v->db.table_tablets[tab->table_id]++;
		}
		virg_tablet_unlock(v, node->id);
	}

	return VIRG_SUCCESS;
}

