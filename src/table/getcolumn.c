#include "virginian.h"

/**
 * @ingroup table
 * @brief Find the ID of a table column given its name
 *
 *
 * @param v		Pointer to the state struct of the database system
 * @param tid	ID of the table of the column
 * @param name	Name of the column
 * @param id	Pointer to an unsigned integer through which the id is returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_getcolumn(virginian *v, unsigned tid, const char* name, unsigned *id)
{
	virg_tablet_meta *tab;

	virg_db_load(v, v->db.first_tablet[tid], &tab);

	for(unsigned i = 0; i < tab->fixed_columns; i++)
	{
		if(strcmp(&tab->fixed_name[i][0], name) == 0) {
			id[0] = i;
			virg_tablet_unlock(v, tab->id);
			return VIRG_SUCCESS;
		}
	}

	virg_tablet_unlock(v, tab->id);

	return VIRG_FAIL;
}

