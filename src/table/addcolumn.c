
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
 * This file contains the virg_table_addcolumn() function.
 */
    

/**
 * @ingroup table
 * @brief Add a column to every tablet in the table
 *
 * This function adds a column to a table by iterating over every table in the
 * table and calling virg_tablet_addcolumn(). This is not a thread-safe
 * function.
 *
 * @param v Pointer to the state struct of the database system
 * @param table_id Table to which the column is added
 * @param name Name of the column
 * @param type The variable type of the column
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_addcolumn(virginian *v, unsigned table_id, const char *name, virg_t type)
{
	virg_tablet_meta *tab;

	// check that the name isn't too long
	int i;
	for(i = 0; name[i] != '\0' && i < VIRG_MAX_COLUMN_NAME; i++);
	VIRG_CHECK(i == VIRG_MAX_COLUMN_NAME, "Column name too long");

	// load the first table of the table into memory
	virg_db_load(v, v->db.first_tablet[table_id], &tab);

	// iterate over every tablet of the table
	while(1) {
		virg_tablet_addcolumn(tab, name, type);

		if(tab->last_tablet)
			break;

		virg_db_loadnext(v, &tab);
	}

	virg_tablet_unlock(v, tab->id);

	return VIRG_SUCCESS;
}

