#include "virginian.h"

/**
 * @ingroup table
 * @brief Create a new table
 *
 * Create and add a new table to the database. When a table is created, only its
 * name and the type of its primary key must be specified. The table can be
 * given more structure later with virg_table_addcolumn(). This function is
 * responble for setting the appropriate meta information in the virg_db struct
 * and creating an empty tablet which is set to both the head and tail of the
 * tablet string.
 *
 * @param v Pointer to the state struct of the database system
 * @param name Name of the table
 * @param key_type The variable type of the table primary key column
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_table_create(virginian *v, const char *name, virg_t key_type)
{
	virg_db *db = &v->db;

	unsigned table_id;

	// find an empty table slot
	for(table_id = 0; table_id < VIRG_MAX_TABLES; table_id++)
		if(db->table_status[table_id] == 0)
			break;
	VIRG_CHECK(table_id == VIRG_MAX_TABLES, "Too many tables")
	db->table_status[table_id] = 1;

	// copy the table name string
	VIRG_CHECK(strlen(name) >= VIRG_MAX_TABLE_NAME, "Table name too long")
	strcpy(&db->tables[table_id][0], name);

	int tablet_id;

	// create an empty tablet
	virg_tablet_create(v, &tablet_id, key_type, table_id);

	// initialize the table elements
	db->first_tablet[table_id] = tablet_id;
	db->last_tablet[table_id] = tablet_id;
	db->write_cursor[table_id] = tablet_id;
	db->table_tablets[table_id]++;

	return VIRG_SUCCESS;
}

