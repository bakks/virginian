
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
 * This file contains the virg_table_getid() function.
 */
        

/**
 * @ingroup table
 * @brief Find the ID of a table given its name
 *
 * Iterate over each table in the database to find the one who's name matches
 * the given string. If no such table is found, the function returns VIRG_FAIL,
 * otherwise the unsigned integer pointed to by the 3rd argument is set to the
 * table's id.
 *
 * @param v		Pointer to the state struct of the database system
 * @param name	Name of the table
 * @param id	Pointer to an unsigned integer through which the id is returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_getid(virginian *v, const char* name, unsigned *id)
{
	for(unsigned i = 0; i < VIRG_MAX_TABLES; i++) {
		if(v->db.table_status[i] > 0 && strcmp(name, &v->db.tables[i][0]) == 0) {
			id[0] = i;
			return VIRG_SUCCESS;
		}
	}

	return VIRG_FAIL;
}

