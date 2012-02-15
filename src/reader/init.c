#include "virginian.h"

/**
 * @ingroup reader
 * @brief Initialize a result tablet reader
 *
 * Initialize a reader object to traverse the results of a query, given its
 * virtual machine state struct. This should only be called with a virg_vm
 * struct once a query that output results is run.
 *
 * @param v		Pointer to the state struct of the database system
 * @param r		Pointer to the reader state
 * @param id	Pointer to the virtual machine state containing the result
 * tablets to be read
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_init(virginian *v, virg_reader *r, virg_vm *vm)
{
	r->vm = vm;

	VIRG_CHECK(vm->head_result == NULL, "No results\n");
	virg_db_load(v, vm->head_result->id, &r->res);
	r->row = 0;

	return VIRG_SUCCESS;
}

