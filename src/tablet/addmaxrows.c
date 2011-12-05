
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
 * This file contains the virg_tablet_addmaxrows() function.
 */
        

/**
 * @ingroup tablet
 * @brief Maximize the fixed-size block of the tablet
 *
 * Add as many rows as possible to the fixed-size data block of a tablet. This
 * function returns VIRG_FAIL if the tablet has already been maxed.
 *
 * @param v     Pointer to the state struct of the database system
 * @param tab   Pointer to the tablet to be modified
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_addmaxrows(virginian *v, virg_tablet_meta *tab)
{
	// calculate the the unused space in the tablet
	size_t stride = tab->row_stride;
	size_t avail = VIRG_TABLET_SIZE - sizeof(virg_tablet_meta) - VIRG_TABLET_MAXED_VARIABLE;
	VIRG_CHECK(avail > VIRG_TABLET_SIZE, "already at max, size_t underflow")

	// calculate how many new rows we can fit into that space
	unsigned pr = avail / stride;

	// add the rows
	virg_tablet_addrows(v, tab, pr - tab->rows);

	return VIRG_SUCCESS;
}

