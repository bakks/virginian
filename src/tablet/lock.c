
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
 * This file contains the virg_tablet_lock() and virg_tablet_unlock() functions.
 */
        

/**
 * @ingroup tablet
 * @brief Add a lock to a tablet
 *
 * Add a lock on a certain tablet to ensure that it remains in that tablet slot.
 * Locks accumulate; all slots start with a status of 0, which is raised to 1
 * when a tablet is loaded into it. A lock increments this number, so a tablet
 * with a status of 2 or above can not be removed from that slot, even when the
 * database is closed. Thus, all locks held on a tablet must be released before
 * closing the tablet's database. If the tablet cannot be found to be locked,
 * then return VIRG_FAIL.
 *
 * @param v     Pointer to the state struct of the database system
 * @param id    ID of the tablet to be locked
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_lock(virginian *v, unsigned tablet_id)
{
#ifdef VIRG_DEBUG_SLOTS
	fprintf(stderr, "trying to lock %i\n", tablet_id);
	virg_print_slots(v);
#endif

	// tablet slot lock
	pthread_mutex_lock(&v->slot_lock);

	// look for tablet to lock
	unsigned i;
	for(i = 0; i < VIRG_MEM_TABLETS; i++)
		if(v->tablet_slot_status[i] > 0 && v->tablet_slot_ids[i] == tablet_id)
			break;

	// check to make sure the tablet was found
#ifdef VIRG_DEBUG
	if(i == VIRG_MEM_TABLETS) {
		virg_print_slots(v);
		fprintf(stderr, "looking for %u\n", tablet_id);
	}
#endif
	VIRG_DEBUG_CHECK(i == VIRG_MEM_TABLETS, "Couldn't find tablet to lock");

	// increase the number of read locks
	v->tablet_slot_status[i]++;

	// unlock tablet slots
	pthread_mutex_unlock(&v->slot_lock);

	return VIRG_SUCCESS;
}

/**
 * @ingroup tablet
 * @brief Release a lock to a tablet
 *
 * Releases a lock on a tablet. If the tablet cannot be found, or if it is not
 * locked when the lock attempts to release, then VIRG_FAIL is returned.
 *
 * @param v     Pointer to the state struct of the database system
 * @param id    ID of the tablet to be unlocked
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_unlock(virginian *v, unsigned tablet_id)
{
#ifdef VIRG_DEBUG_SLOTS
	fprintf(stderr, "trying to unlock %i\n", tablet_id);
	virg_print_slots(v);
#endif

	// tablet slot lock
	pthread_mutex_lock(&v->slot_lock);
	
	// look for tablet, which must have a locked status
	unsigned i;
	for(i = 0; i < VIRG_MEM_TABLETS; i++)
		if(v->tablet_slot_status[i] > 1 && v->tablet_slot_ids[i] == tablet_id)
			break;

	// check to make sure the tablet was found
#ifdef VIRG_DEBUG
	if(i == VIRG_MEM_TABLETS) {
		virg_print_slots(v);
		fprintf(stderr, "looking for %u\n", tablet_id);
	}
#endif
	VIRG_DEBUG_CHECK(i == VIRG_MEM_TABLETS, "Couldn't find tablet to unlock");

	// decrease the number of read locks
	v->tablet_slot_status[i]--;

	// unlock tablet slots
	pthread_mutex_unlock(&v->slot_lock);

	return VIRG_SUCCESS;
}

