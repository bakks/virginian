
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
 * This file contains the virg_db_open() function.
 */


/**
 * @ingroup database
 * @brief Open an existing database file
 *
 * Open a database that already exists from its location on disk. This function
 * reads the database meta information into memory, but does not load any
 * tablets into memory. Open databases should be closed with the virg_db_close()
 * function and created with the virg_db_create() function. This function sets
 * the information in the virg_db struct stored within the passed virginian
 * struct. Only one database can be open at a time, so this function should only
 * be called if there are no other open databases.
 *
 * @param v Pointer to the state struct of the database system
 * @param file Location on disk of the database file
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_db_open(virginian *v, const char *file)
{
	int fd;
	size_t r;

	// ensure that no database is currently open
	VIRG_CHECK(v->dbfd != -1, "Database already open")

	// open the database file for reading and writing
	fd = open(file, O_RDWR, S_IRUSR | S_IWUSR);
	VIRG_CHECK(fd == -1, "Problem opening database file")
	v->dbfd = fd;

	// read the fixed size meta information into our virg_db struct
	r = read(fd, &v->db, sizeof(virg_db));
	VIRG_CHECK(r < sizeof(virg_db), "Corrupt database file")

	// allocate an area for the variable-size tablet tracking information
	// even if there are no tablets, alloced_tablets is set to non-zero when the
	// database is created
	size_t size = v->db.alloced_tablets * sizeof(virg_tablet_info);
	v->db.tablet_info = (virg_tablet_info*) malloc(size);
	VIRG_CHECK(v->db.tablet_info == NULL, "Out of memory")

	// read the meta-information from disk
	r = read(fd, v->db.tablet_info, size);
	VIRG_CHECK(r < size, "Problem reading tablet info")

	return VIRG_SUCCESS;
}

