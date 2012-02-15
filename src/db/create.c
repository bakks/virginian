#include "virginian.h"

/**
 * @ingroup database
 * @brief Create and initialize a new database
 *
 * Create a database by initializing the virg_db struct that is stored within
 * the virginian struct. The Virginian database can only have a single database
 * open at once, so this should not be called if another database has been
 * opened and has not yet been closed. Within the database struct we initialize
 * our listing of tables and tablets, showing that each is empty. Additionally,
 * we open the database file based on the the file argument, since we may need
 * to write to it before virg_db_close() is called.
 *
 * @param v Pointer to the state struct of the database system
 * @param file Location on disk where the database file should be written
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_db_create(virginian *v, const char *file)
{
	unsigned i;

	virg_db *db = &v->db;
	db->num_tablets = 0;
	db->tablet_id_counter = 0;
	v->dbfd = -1;

	// initialize the listing of tables
	for(i = 0; i < VIRG_MAX_TABLES; i++) {
		db->tables[i][0] = '\0';
		db->table_status[i] = 0;
		db->table_tablets[i] = 0;
		db->write_cursor[i] = 0;
	}

	// initialize tablet information
	db->alloced_tablets = VIRG_TABLET_INFO_SIZE;
	size_t size = VIRG_TABLET_INFO_SIZE * sizeof(virg_tablet_info);
	db->tablet_info = malloc(size);
	VIRG_CHECK(db->tablet_info == NULL, "Out of memory");
	db->block_size = sizeof(virg_db) + size;

#ifdef VIRG_DEBUG
	// zero out malloced block for valgrind
	memset(db->tablet_info, 0xDEADBEEF, size);
#endif

	// make sure every tablet slot is noted as not being used
	for(i = 0; i < db->alloced_tablets; i++)
		db->tablet_info[i].used = 0;

	// open database file on disk
	v->dbfd = open(file, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	VIRG_CHECK(v->dbfd < 0, "Problem creating file")

	return VIRG_SUCCESS;
}

