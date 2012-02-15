#include "virginian.h"

/**
 * @ingroup database
 * @brief Load a tablet into a slot based on its ID and return a pointer
 *
 * This function is used when you are attempting to access a tablet based on its
 * ID. It will add a read-lock for the tablet and return a pointer to it. For
 * efficiency, it first checks to see if the tablet already resides in a
 * main-memory tablet slot. If it does, we simple add the lock and return the
 * pointer. Otherwise we must fetch the tablet from the database file on disk
 * and read it into a tablet slot. This function is thread-safe and performs
 * several checks to ensure that the tablet ID actually exists and that the
 * expected size of the tablet is actually read.
 *
 * @param v Pointer to the state struct of the database system
 * @param tablet_id The ID of the tablet being loaded
 * @param tab A pointer to a tablet pointer through which the location of the loaded tablet is returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_db_load(virginian *v, unsigned tablet_id, virg_tablet_meta **tab)
{
	unsigned i;
	unsigned slot;

	// lock all the tablet slots
	pthread_mutex_lock(&v->slot_lock);

	// check if the tablet is already loaded
	for(i = 0; i < VIRG_MEM_TABLETS; i++)
		if(v->tablet_slot_status[i] != 0 && v->tablet_slot_ids[i] == tablet_id) {
			if(tab != NULL)
				tab[0] = v->tablet_slots[i];

			// if so, add an extra tablet lock and return the pointer
			v->tablet_slot_status[i]++;
			pthread_mutex_unlock(&v->slot_lock);
			return VIRG_SUCCESS;
		}

	// if not already loaded, find an empty slot
	virg_db_findslot(v, &slot);

	// find tablet on disk using the virg_db meta information
	for(i = 0; i < v->db.alloced_tablets; i++)
		if(v->db.tablet_info[i].used == 1
			&& v->db.tablet_info[i].id == tablet_id)
			break;

	// make sure the tablet id was found
	// the tablet should never not be found, and it indicates a corrupt database
	// file
#ifdef VIRG_DEBUG
	if(i == v->db.alloced_tablets) {
		fprintf(stderr, "LOOKING FOR %u\n", tablet_id);
		virg_print_tablet_info(v);
	}
#endif
	VIRG_CHECK(i == v->db.alloced_tablets, "Could not find tablet id")

	// get tablet meta information from disk
	off_t x = v->db.block_size + i * VIRG_TABLET_SIZE;
	unsigned r = pread(v->dbfd, v->tablet_slots[slot], sizeof(virg_tablet_meta), x);
	VIRG_CHECK(r < (int)sizeof(virg_tablet_meta), "Failed to get tablet meta data")

	// get the rest of the tablet from disk
	r = pread(v->dbfd,
		(char*)v->tablet_slots[slot] + sizeof(virg_tablet_meta),
		v->tablet_slots[slot]->size - sizeof(virg_tablet_meta),
		x + sizeof(virg_tablet_meta));

	// if for some reason the pread() call returned fewer bytes than it was
	// supposed to, complain because the database file is corrupted
#ifdef VIRG_DEBUG
	if(r < v->tablet_slots[slot]->size - sizeof(virg_tablet_meta)) {
		fprintf(stderr, "COULDN'T GET TABLET DATA\n");
		fprintf(stderr, "LOOKING FOR %u\n", tablet_id);
		virg_print_tablet_meta(v->tablet_slots[slot]);
		virg_print_tablet_info(v);
	}
#endif
	VIRG_CHECK(r < v->tablet_slots[slot]->size -
		sizeof(virg_tablet_meta), "Failed to get tablet data")

	// set the appropriate tablet data
	v->tablet_slots[slot]->info = &v->db.tablet_info[i];
	v->tablet_slot_ids[slot] = tablet_id;

	// return a pointer to the loaded tablet
	if(tab != NULL)
		tab[0] = v->tablet_slots[slot];

	// unlock tablet array lock
	pthread_mutex_unlock(&v->slot_lock);

	return VIRG_SUCCESS;
}

