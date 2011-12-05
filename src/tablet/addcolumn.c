
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
 * This file contains the virg_tablet_addcolumn() function.
 */
        

/**
 * @ingroup tablet
 * @brief Add a column to a tablet
 *
 * Modifies a tablet, including making it larger, to contain a new column, added
 * to the end of the original columns. This function does not check to ensure
 * that there is enough room in the tablet for the new column, so columns should
 * be added only if there is a good amount of empty space in the tablet. The
 * virg_tablet_growfixed() function is used to expand the tablet.
 *
 * @param tab	The tablet to which the column is to be added
 * @param name	The name of the column to add
 * @param type	The variable type of the column to add
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_addcolumn(virg_tablet_meta *tab, const char *name, virg_t type)
{
	int i;
	int col = tab->fixed_columns;

	VIRG_CHECK(col == VIRG_MAX_COLUMNS, "Too many columns")

	// copy column name
	// length checked in virg_table_addcolumn()
	for(i = 0; name[i] != '\0'; i++)
		tab->fixed_name[col][i] = name[i];
	tab->fixed_name[col][i] = '\0';

	// set column information
	tab->fixed_type[col] = type;
	tab->fixed_stride[col] = virg_sizeof(type);
	tab->row_stride += virg_sizeof(type);
	tab->fixed_offset[col] =
		(col == 0) ? 0 : tab->fixed_offset[col-1] + tab->fixed_stride[col-1] * tab->possible_rows;
	tab->fixed_columns++;

	// make room in the tablet for the new column
	virg_tablet_growfixed(tab, tab->fixed_stride[col] * tab->possible_rows);

	return VIRG_SUCCESS;
}

