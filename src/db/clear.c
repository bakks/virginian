#include "virginian.h"
    

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

