#include "virginian.h"

/**
 * @ingroup reader
 * @brief Move the reader to the next tablet
 *
 * Move the reader state onto the next result tablet, resetting the row mark to
 * the beginning of that next tablet. Returns VIRG_FAIL if the reader is already
 * at the last tablet.
 *
 * @param v     Pointer to the state struct of the database system
 * @param r     Pointer to the reader state
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_nexttablet(virginian *v, virg_reader *r)
{
	VIRG_CHECK(r->res->last_tablet, "Reached the last tablet");
	r->row = 0;

	return virg_db_loadnext(v, &r->res);
}

