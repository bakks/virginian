
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
 * This file contains the virg_db_clear() function.
 */
    

/**
 * @ingroup database
 * @brief Clear out a certain tablet slot
 *
 * Clear out a tablet slot, specified with the 2nd argument, by writing the
 * content of that tablet, then setting the appropriate values to indicate that
 * the slot is now empty. This function is called for every tablet slot in
 * virg_db_close() to ensure that all tablet has been moved to disk. This
 * function is not thread-safe, the tablet slot array must be locked outide of
 * it.
 *
 * @param v Pointer to the state struct of the database system
 * @param slot Tablet slot to be cleared
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_db_clear(virginian *v, unsigned slot)
{
	// check the slot argument
	if(v->tablet_slot_status[slot] == 0)
		return VIRG_SUCCESS;

	// check to make sure that the tablet isn't locked
	VIRG_CHECK(v->tablet_slot_status[slot] > 1, "Trying to clear a locked slot")

	// write the contents of the tablet to disk
	virg_db_write(v, slot);

	// Note that the tablet slot is now empty
	v->tablet_slot_status[slot] = 0;
	v->tablet_slots_taken--;

	return VIRG_SUCCESS;
}

