
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
 * This file contains the virg_tablet_remove() function.
 */
        

/**
 * @ingroup tablet
 * @brief Deletes a tablet from memory and disk
 *
 * Sets the in-memory tablet slot and disk slot of a tablet to unused, removing
 * that tablet. Note that this function is used for removing result tablets and
 * does not change the variables of other tablets in the tablet string, so it
 * will leave that string inconsistent.
 *
 * @param v     Pointer to the state struct of the database system
 * @param id    ID of the tablet to be removed
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_remove(virginian *v, unsigned id)
{
	unsigned i;

#ifdef VIRG_DEBUG_SLOTS
	fprintf(stderr, "trying to remove %i\n", id);
	virg_print_slots(v);
#endif

	// find tablet with id
	for(i = 0; i < VIRG_MEM_TABLETS; i++) {
		if(v->tablet_slot_status[i] != 0 && v->tablet_slot_ids[i] == id) {
			VIRG_DEBUG_CHECK(v->tablet_slot_status[i] > 1, "Trying to remove locked tablet");

			// set in-memory tablet slot to unused
			virg_tablet_meta *tab = v->tablet_slots[i];
			virg_tablet_info *info = tab->info;
			if(info != NULL)
				info->used = 0;

			v->tablet_slot_status[i] = 0;
			v->tablet_slots_taken--;

			return VIRG_SUCCESS;
		}
	}

	// look for the tablet on disk and set its spot to unused if found
	virg_db *db = &v->db;
	for(i = 0; i < db->alloced_tablets; i++) {
		if(db->tablet_info[i].used == 1 && db->tablet_info[i].id == id) {
			db->tablet_info[i].used = 0;
			return VIRG_SUCCESS;
		}
	}

	fprintf(stderr, "Could not find tablet to remove\n");
	return VIRG_FAIL;
}

