#include "virginian.h"

/**
 * @ingroup reader
 * @brief Count the number of result rows still to be read
 *
 * Count the number of rows between the current location of the reader and the
 * last possible row. This function does not advance the reader. This function
 * fails if the reader has not been initialized, and returns the rows found in
 * the unsigned integer pointed to by the 3rd argument.
 *
 * @param v     Pointer to the state struct of the database system
 * @param r     Pointer to the reader state
 * @param rows  Pointer to an unsigned int through which the rows found will be
 * returned
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_getrows(virginian *v, virg_reader *r, unsigned *rows_)
{
	unsigned rows = 0;

	if(r->res == NULL)
		return VIRG_FAIL;

	// use our own pointer so we don't advance the reader
	virg_tablet_meta *res = r->res;

	// iterate to the end of the tablet string
	while(1) {
		rows += res->rows;

		if(res->last_tablet)
			break;

		virg_db_loadnext(v, &res);
	}

	// subtract the position in the current tablet
	rows -= r->row;

	rows_[0] = rows;
	return VIRG_SUCCESS;
}

