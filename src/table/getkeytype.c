#include "virginian.h"

/**
 * @ingroup table
 * @brief Find the type of a table's key
 *
 * @param v		Pointer to the state struct of the database system
 * @param tid	ID of the table of the column
 * @param type	Pointer to a data type variable through which the result is returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_getkeytype(virginian *v, unsigned tid, virg_t *type)
{
	virg_tablet_meta *tab;

	virg_db_load(v, v->db.first_tablet[tid], &tab);
	type[0] = tab->key_type;
	virg_tablet_unlock(v, tab->id);

	return VIRG_SUCCESS;
}

