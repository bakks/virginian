
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
 * This file contains the virg_table_insert() function.
 */
        

/**
 * @ingroup table
 * @brief Insert a row into a table
 *
 * Insert a new row by adding it to the end of a table. This function locates
 * the tablet where we have set the write_cursor. If the tablet is full, we
 * attempt to add more row space with virg_tablet_addrows(), and if we can't,
 * then we move onto the next tablet in the tablet string. The key and data
 * arguments are passed as pointers to their buffer because the size of their
 * variable types is unknown. The data buffer should contain all the columns in
 * order immediately adjacent to each other. For example, the following code
 * adds a row to a table with an integer key, and an integer, double, and float
 * column:
 *
 * @code
 * int i = 150;
 * char buff[sizeof(int) + sizeof(double) + sizeof(float)];
 * char *x = &buff[0];
 * ((int*)x)[0] = 100;
 * x += sizeof(int);
 * ((double*)x)[0] = 100.0;
 * x += sizeof(double);
 * ((float*)x)[0] = 100.0;
 * virg_table_insert(v, table_id, &i, &buff[0], NULL);
 * @endcode
 *
 * @param v 		Pointer to the state struct of the database system
 * @param table_id 	Table in which to insert the row
 * @param key 		Buffer containing the key value for this row
 * @param data		Buffer containing the row data
 * @param blob		Buffer containing variable size data associated with the
 * key (unimplemented)
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_insert(virginian *v, unsigned table_id, char *key, char *data, char *blob)
{
	// TODO add support for blob ptr
	unsigned i;
	size_t stride;
	char *dest;
	char *src = data;
	virg_tablet_meta *tab;

	assert(blob == NULL);

	// load the table's table on which we have a write cursor
	virg_db_load(v, v->db.write_cursor[table_id], &tab);

	// check for corruption
	assert(tab->rows <= tab->possible_rows);

	// if the current tablet is full
	if(tab->rows == tab->possible_rows) {
		// if there is room to add more fixed-size rows
		if(tab->size < VIRG_TABLET_SIZE - tab->row_stride)
			virg_tablet_addrows(v, tab, VIRG_TABLET_KEY_INCREMENT);
		// otherwise move on to the next tablet
		else {
			virg_db_loadnext(v, &tab);
			v->db.write_cursor[tab->table_id] = tab->id;
		}
	}

	// copy key from buffer
	char *fixed_ptr = (char*)tab + tab->fixed_block;
	char *key_ptr = (char*)tab + tab->key_block + tab->rows * tab->key_stride;
	memcpy(key_ptr, key, tab->key_stride);

	assert(tab->rows < tab->possible_rows);

	// copy over all columns from buffer
	for(i = 0; i < tab->fixed_columns; i++) {
		stride = tab->fixed_stride[i];
		dest = fixed_ptr +
			tab->fixed_offset[i] + tab->rows * stride;
		memcpy(dest, src, stride);
		src += stride;
	}

	tab->rows++;

	virg_tablet_unlock(v, tab->id);

	return VIRG_SUCCESS;
}

