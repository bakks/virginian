#include "virginian.h"

/**
 * @ingroup database
 * @brief Allocate a tablet slot and return a pointer to it
 *
 * Find a free main-memory tablet slot using the virg_db_findslot() function,
 * assign the passed ID to this tablet slot, and return a pointer to the slot
 * through the 2nd argument. This function is called whenever a data or
 * result tablet needs to be placed in memory.
 *
 * @param v Pointer to the state struct of the database system
 * @param meta Pointer to a pointer to a tablet, set during allocation
 * @param id ID of the tablet for which a slot is being allocated
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_db_alloc(virginian *v, virg_tablet_meta **meta, int id)
{
	// lock tablet slots
	pthread_mutex_lock(&v->slot_lock);

	unsigned slot;

	// locate empty tablet slot
	virg_db_findslot(v, &slot);

	// set id of this tablet slot, and return a ptr to it
	meta[0] = v->tablet_slots[slot];
	v->tablet_slot_ids[slot] = id;
	meta[0]->id = id;

	// unloc
	pthread_mutex_unlock(&v->slot_lock);

#ifdef VIRG_DEBUG_SLOTS
	fprintf(stderr, "After alloc\n");
	virg_print_slots(v);
#endif

	return VIRG_SUCCESS;
}

