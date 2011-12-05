
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
 * This file contains the virg_db_loadnext() function.
 */
    

/**
 * @ingroup database
 * @brief Advance a tablet pointer to the next tablet in its string of tablets
 *
 * This is a convenience function used to handle the correct locking and
 * unlocking as we walk along a string of tablets by using the
 * virg_tablet_meta.next information. It uses a temporary pointer to safely load
 * the next tablet before unlocking the current one. It then advances the passed
 * pointer to this new tablet. Note that a check to ensure that the current
 * tablet is not the last in the tablet string should be performed before
 * calling this function.
 *
 * @param v Pointer to the state struct of the database system
 * @param tab Pointer to a tablet pointer to be pointed to the next tablet
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */

int virg_db_loadnext(virginian *v, virg_tablet_meta **tab)
{
	// temporary tablet pointer
	virg_tablet_meta *t = tab[0];

	// load the next pointer while the current one is still locked
	virg_db_load(v, t->next, tab);

	// unlock the old tablet
	virg_tablet_unlock(v, t->id);

	return VIRG_SUCCESS;
}

