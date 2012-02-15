#include "virginian.h"

/**
 * @ingroup virginian
 * @brief Executes a SQL query
 *
 * This function is the primary driver of SQL queries. Though it is simple and
 * its actions can be called directly, this interface should be sufficient for
 * most purposes. The reader pointer should not point to an allocated block, as
 * it will be reassigned. See the virg_reader documentation for more information
 * on accessing results. Once results are no longer needed, virg_release()
 * should be called to clean up after the query, otherwise you will probably
 * leak memory.
 *
 * @param v Pointer to the state struct of the database system
 * @param reader Pointer to a pointer to a reader of results
 * @param query SQL query string
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_query(virginian *v, virg_reader **reader, const char *query)
{
	virg_vm *vm = virg_vm_init();
	virg_sql(v, query, vm);

	virg_vm_execute(v, vm);
	reader[0] = (virg_reader*)malloc(sizeof(virg_reader));
	virg_reader_init(v, reader[0], vm);

	return VIRG_SUCCESS;
}

