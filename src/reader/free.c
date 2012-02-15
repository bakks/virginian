#include "virginian.h"

/**
 * @ingroup reader
 * @brief Frees the resources used by the reader
 *
 * Closes the reader by unlocking the current tablet in use. This must be
 * called only after virg_tablet_init() and before virg_db_close().
 *
 * @param v     Pointer to the state struct of the database system
 * @param r     Pointer to the reader state
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_free(virginian *v, virg_reader *r)
{
	if(r->res != NULL)
		virg_tablet_unlock(v, r->res->id);

	return VIRG_SUCCESS;
}

