
/**
 * This is a simple example of a program that uses the Virginian database. This
 * is intended to help begin experimenting with the code, there is significant
 * functionality that is not utilized. For more information on the functions
 * used below, see the documentation or source files. A make file is included to
 * show how compilation works.
 */

#include <stdio.h>
#include <unistd.h>
#include "virginian.h"

int main()
{
	// declare db state struct
	virginian v;

	// delete database file if it exists
	unlink("testdb");

	// initialize state
	virg_init(&v);

	// create new database in testdb file
	virg_db_create(&v, "testdb");

	// create table called test with an integer column called col0
	virg_table_create(&v, "test", VIRG_INT);
	virg_table_addcolumn(&v, 0, "col0", VIRG_INT);

	// insert 100 rows, using i as the row key and x as the value for col0
	int i, x;
	for(i = 0; i < 100; i++) {
		x = i * 5;
		virg_table_insert(&v, 0, (char*)&i, (char*)&x, NULL);
	}

	// declare reader pointer
	virg_reader *r;

	// set optional query parameters
	v.use_multi = 0;	// not multithreaded
	v.use_gpu = 0;		// don't use GPU
	v.use_mmap = 0;		// don't use mapped memory

	// execute query
	virg_query(&v, &r, "select id, col0 from test where col0 <= 25");

	// output result column names
	unsigned j;
	for(j = 0; j < r->res->fixed_columns; j++)
		printf("%s\t", r->res->fixed_name[j]);
	printf("\n");

	// output result data
	while(virg_reader_row(&v, r) != VIRG_FAIL) {
		int *results = (int*)r->buffer;

		printf("%i\t%i\n", results[0], results[1]);
	}

	// clean up after query
	virg_reader_free(&v, r);
	virg_vm_cleanup(&v, r->vm);
	free(r);

	// close database
	virg_close(&v);

	// delete database file
	unlink("testdb");
}

