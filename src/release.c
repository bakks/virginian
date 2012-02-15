#include "virginian.h"

/**
 * @ingroup virginian
 * @brief Cleans up after a SQL query
 *
 * Call this after you are done with the results of a SQL query to unlock and
 * purge the result tablets and release the reader. See the documentation of
 * virg_query() for an example.
 *
 * @param v Pointer to the state struct of the database system
 * @param reader Pointer to a pointer to a reader of results
 */
void virg_release(virginian *v, virg_reader *reader)
{
	virg_reader_free(v, reader);
	virg_vm_cleanup(v, reader->vm);
	free(reader);
}

