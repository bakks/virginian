
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
 * This file contains the virg_table_getcolumn() function.
 */
        

/**
 * @ingroup table
 * @brief Find the ID of a table column given its name
 *
 *
 * @param v		Pointer to the state struct of the database system
 * @param tid	ID of the table of the column
 * @param name	Name of the column
 * @param id	Pointer to an unsigned integer through which the id is returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_getcolumn(virginian *v, unsigned tid, const char* name, unsigned *id)
{
	virg_tablet_meta *tab;

	virg_db_load(v, v->db.first_tablet[tid], &tab);

	for(unsigned i = 0; i < tab->fixed_columns; i++)
	{
		if(strcmp(&tab->fixed_name[i][0], name) == 0) {
			id[0] = i;
			virg_tablet_unlock(v, tab->id);
			return VIRG_SUCCESS;
		}
	}

	virg_tablet_unlock(v, tab->id);

	return VIRG_FAIL;
}

