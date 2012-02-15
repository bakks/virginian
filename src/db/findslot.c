#include "virginian.h"

/**
 * @ingroup database
 * @brief Find an empty or unlocked tablet slot
 *
 * Attempt to find a tablet slot that is unoccupied. If all tablet slots are
 * occupied, then attempt to find one that is not locked. If one is found, the
 * contents of that tablet are written to disk, and the slot number is returned.
 * Otherwise return a failure. This function is not thread-safe, so the tablet
 * slot array must be locked outside of it in a multi-threaded environment.
 *
 * @param v Pointer to the state struct of the database system
 * @param slot Pointer to an unsigned integer through which the found slot will be returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */

int virg_db_findslot(virginian *v, unsigned *slot_)
{
	unsigned slot;

	if(v->tablet_slots_taken < VIRG_MEM_TABLETS) { // theres an empty slot
		for(slot = 0; ; slot++) { // assume we'll find empty slot before end
			// check that this assumption is true
			assert(slot < VIRG_MEM_TABLETS);

			// if the tablet is empty, break bc we found our slot
			if(v->tablet_slot_status[slot] == 0)
				break;
		}
		// lock the tablet
		v->tablet_slots_taken++;
		v->tablet_slot_status[slot] = 2;
	}
	else {
		// get our round-robin starting location and increment it
		slot = v->tablet_slot_counter++;
		if(v->tablet_slot_counter >= VIRG_MEM_TABLETS)
			v->tablet_slot_counter = 0;

		unsigned checked = 0;

		// search for an occupied but unlocked tablet
		for( ; checked < VIRG_MEM_TABLETS && v->tablet_slot_status[slot] > 1;
			checked++, slot = (slot + 1) % VIRG_MEM_TABLETS);

		// fail if everything is locked
		VIRG_CHECK(slot == VIRG_MEM_TABLETS, "All tablets locked")

		assert(v->tablet_slot_status[slot] == 1);
		v->tablet_slot_status[slot]++;

		// write the slot contents to disk but don't assign new id bc we don't
		// know it
		virg_db_write(v, slot);
	}

	// return tablet slot number
	slot_[0] = slot;

	return VIRG_SUCCESS;
}

