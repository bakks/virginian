#include "virginian.h"

/**
 * @ingroup table
 * @brief 
 *
 * Iterate over each table in the database to find the one who's name matches
 * the given string. If no such table is found, the function returns VIRG_FAIL,
 * otherwise the unsigned integer pointed to by the 3rd argument is set to the
 * table's id.
 *
 * @param v		Pointer to the state struct of the database system
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_numrows(virginian *v, unsigned id, unsigned *rows)
{
	unsigned i = 0;
	virg_tablet_meta *tab;

	unsigned tab_id = v->db.first_tablet[id];

	virg_db_load(v, tab_id, &tab);
	i += tab->rows;

	while(!tab->last_tablet) {
		virg_db_loadnext(v, &tab);
		i += tab->rows;
	}

	virg_tablet_unlock(v, tab->id);

	rows[0] = i;

	return VIRG_SUCCESS;
}

