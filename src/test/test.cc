extern "C" {
#include "virginian.h"
}

#include "test/test.h"

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

virginian *VirginianTest::simpledb_create()
{
	virginian *v = (virginian*)malloc(sizeof(virginian));
	unlink("testdb");
	virg_init(v);
	virg_db_create(v, "testdb");

	virg_table_create(v, "test", VIRG_INT);
	virg_table_addcolumn(v, 0, "col0", VIRG_INT);
	virg_table_addcolumn(v, 0, "col1", VIRG_INT);
	virg_table_addcolumn(v, 0, "col2", VIRG_INT);

	return v;
}

void VirginianTest::simpledb_addrows(virginian *v, int numrows)
{
	unsigned x;
	int y[3];
	virg_table_getid(v, "test", &x);
	virg_table_numrows(v, x, &x);

	for(int i = 0; i < numrows; i++, x++) {
		y[0] = (int)x;
		y[1] = y[0] + 1;
		y[2] = y[1] + 1;
		virg_table_insert(v, 0, (char*)&x, (char*)&y, NULL);
	}
}

void VirginianTest::simpledb_clear(virginian *v)
{
	virg_close(v);
	free(v);
	cudaThreadExit();
	unlink("testdb");
}

