#include "virginian.h"

/**
 * @ingroup table
 * @brief Load as many tablets from a table into memory as possible
 *
 * Fill up a maximum of half of the memory allocated tablet slots with tablets
 * from a table. This is useful for guaranteeing that tablets are in memory
 * before executing a query.
 *
 * @param v			Pointer to the state struct of the database system
 * @param table_id	ID of the table to load
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_loadmem(virginian *v, unsigned table_id)
{
	unsigned max_tablets = VIRG_MEM_TABLETS / 2;
	unsigned i;
	virg_tablet_meta *tab;

	// load the first tablet from the string
	virg_db_load(v, v->db.first_tablet[table_id], &tab);
	virg_tablet_unlock(v, tab->id);

	// load tablets until we hit max_tablets
	for(i = 1; i < max_tablets && !tab->last_tablet; i++) {
		virg_db_load(v, tab->next, &tab);
		virg_tablet_unlock(v, tab->id);
	}

	return VIRG_SUCCESS;
}

