#include "virginian.h"
#include "test/test.h"

namespace {

class SQLTest : public VirginianTest {
	protected:

};

static const char *query_sql = "select col0 from test";
static const char *query_exprops = "select col0 + 5 * 20 from test";

#define query_conditions_size 9
static const char *query_conditions[query_conditions_size] = {
	"select col0 from test where col0 < 9 and col0 >= 7 or col0 = 3",
	"select col0 from test where col0 >= 8 or col0 = 3 and col0 < 1",
	"select col0 from test where col0 = 8 and col0 = 8",
	"select col0 from test where col0 = 8 or col0 = 8",
	"select col0 from test where col0 = 2", 
	"select col0 from test where (col0 = 2)",
	"select col0 from test where col0 < 5 and (col0 = 7 or col0 = 2)",
	"select col0 from test where col0 = 7 or col0 = 2 or col0 = 8",
	"select col0 from test where col0 < 5 and (col0 = 2 or col0 = 8 or col0 = 1)"
};

static const unsigned query_conditions_rows[query_conditions_size] = {
	3,
	2,
	1,
	1,
	1,
	1,
	1,
	3,
	2
};


TEST_F(SQLTest, Basic) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_vm *vm = virg_vm_init();
	virg_sql(v, query_sql, vm);

	EXPECT_TRUE(vm != NULL);

	int i = 0;

	EXPECT_EQ(vm->stmt[i++].op, OP_Table);
	EXPECT_EQ(vm->stmt[i].op, OP_ResultColumn);
	EXPECT_EQ(strcmp(vm->stmt[i++].p4.s, "col0"), 0);
	EXPECT_EQ(vm->stmt[i].op, OP_Parallel);
	
	virg_vm_cleanup(v, vm);
	simpledb_clear(v);
}


TEST_F(SQLTest, ExecuteSingle) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, query_sql);

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 0);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ExecuteMulti) {
	virginian *v = simpledb_create();
	v->use_multi = 1;
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, query_sql);

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 0);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ExecuteGPU) {
	virginian *v = simpledb_create();
	v->use_gpu = 1;
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, query_sql);

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 0);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ExprOps) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_vm *vm = virg_vm_init();
	virg_sql(v, query_exprops, vm);

	EXPECT_TRUE(vm != NULL);

	int i = 0;

#ifdef VIRG_DEBUG
	//virg_print_stmt(vm);
#endif

	EXPECT_EQ(vm->stmt[i++].op, OP_Table);
	EXPECT_EQ(vm->stmt[i].op, OP_ResultColumn);
	EXPECT_EQ(strcmp(vm->stmt[i++].p4.s, "(col0+(5*20))"), 0);
	EXPECT_EQ(vm->stmt[i].op, OP_Parallel);

	i += 2;
	EXPECT_EQ(vm->stmt[i].op, OP_Integer);
	EXPECT_EQ(vm->stmt[i].p2, 100);
	
	virg_vm_cleanup(v, vm);
	simpledb_clear(v);
}

TEST_F(SQLTest, ExprOpsSingle) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, "select col0 + 10 * (1 + 2) from test");

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 30);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ExprOpsMulti) {
	virginian *v = simpledb_create();
	v->use_multi = 1;
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, query_exprops);

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 100);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ExprOpsGPU) {
	virginian *v = simpledb_create();
	v->use_gpu = 1;
	simpledb_addrows(v, 1);

	virg_reader *r;
	virg_query(v, &r, query_exprops);

	EXPECT_TRUE(r->res->fixed_columns == 1);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 100);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}


TEST_F(SQLTest, Conditions) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_vm *vm = virg_vm_init();
	virg_sql(v, query_conditions[0], vm);

	EXPECT_TRUE(vm != NULL);

	//virg_print_stmt(vm);
	
	virg_vm_cleanup(v, vm);
	simpledb_clear(v);
}

TEST_F(SQLTest, ConditionsSingle) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 10);

	for(int i = 0; i < query_conditions_size; i++) {
		virg_reader *r;
		virg_query(v, &r, query_conditions[i]);

		EXPECT_TRUE(r->res->fixed_columns == 1);
		EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

		unsigned rows;
		virg_reader_getrows(v, r, &rows);
		//printf("%u\n", rows);
		EXPECT_TRUE(rows == query_conditions_rows[i]);

		while(virg_reader_row(v, r) != VIRG_FAIL) {
			int x = ((int*)r->buffer)[0];

			if(i == 0) {
				EXPECT_TRUE(x == 3 || x == 7 || x == 8);
			}
			else if(i == 1) {
				EXPECT_TRUE(x == 8 || x == 9);
			}
		}

		virg_reader_free(v, r);
		virg_vm_cleanup(v, r->vm);
		free(r);
	}
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ConditionsGPU) {
	virginian *v = simpledb_create();
	v->use_gpu = 1;
	simpledb_addrows(v, 10);

	for(int i = 0; i < query_conditions_size; i++) {
		virg_reader *r;
		virg_query(v, &r, query_conditions[i]);

		//virg_print_tablet_meta(r->res);

		EXPECT_TRUE(r->res->fixed_columns == 1);
		EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

		unsigned rows;
		virg_reader_getrows(v, r, &rows);
		//printf("%u\n", rows);
		EXPECT_TRUE(rows == query_conditions_rows[i]);

		while(virg_reader_row(v, r) != VIRG_FAIL) {
			int x = ((int*)r->buffer)[0];

			if(i == 0) {
				EXPECT_TRUE(x == 3 || x == 7 || x == 8);
			}
			else if(i == 1) {
				EXPECT_TRUE(x == 8 || x == 9);
			}
		}

		//virg_print_stmt(r->vm);

		virg_reader_free(v, r);
		virg_vm_cleanup(v, r->vm);
		free(r);
	}
	
	simpledb_clear(v);
}

TEST_F(SQLTest, ConditionsGPUMapped) {
	virginian *v = simpledb_create();
	v->use_gpu = 1;
	v->use_mmap = 1;
	simpledb_addrows(v, 10);

	for(int i = 0; i < query_conditions_size; i++) {
		virg_reader *r;
		virg_query(v, &r, query_conditions[i]);

		EXPECT_TRUE(r->res->fixed_columns == 1);
		EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);

		unsigned rows;
		virg_reader_getrows(v, r, &rows);
		//printf("%u\n", rows);
		EXPECT_TRUE(rows == query_conditions_rows[i]);

		while(virg_reader_row(v, r) != VIRG_FAIL) {
			int x = ((int*)r->buffer)[0];

			if(i == 0) {
				EXPECT_TRUE(x == 3 || x == 7 || x == 8);
			}
			else if(i == 1) {
				EXPECT_TRUE(x == 8 || x == 9);
			}
		}

		//virg_print_stmt(r->vm);

		virg_reader_free(v, r);
		virg_vm_cleanup(v, r->vm);
		free(r);
	}
	
	simpledb_clear(v);
}

TEST_F(SQLTest, Columns) {
	virginian *v = simpledb_create();
	simpledb_addrows(v, 1);

	virg_vm *vm = virg_vm_init();
	virg_sql(v, "select col0, col1 from test", vm);

	//virg_print_stmt(vm);

	EXPECT_TRUE(vm != NULL);

	int i = 0;
	int col0 = -1;
	int col1 = -1;

	EXPECT_EQ(vm->stmt[i++].op, OP_Table);
	for(; i < (int)vm->num_ops; i++) {
		virg_op op = vm->stmt[i];

		if(op.op == OP_Column) {
			if(op.p2 == 0)
				col0 = op.p1;
			if(op.p2 == 1)
				col1 = op.p1;
		}

		if(op.op == OP_Result) {
			EXPECT_EQ(op.p1, col0);
			EXPECT_EQ(op.p2, 2);
		}
	}

	// output column ordering
	EXPECT_LT(col0, col1);
	
	virg_vm_cleanup(v, vm);



	virg_reader *r;
	virg_query(v, &r, "select col0, col1 from test");

	EXPECT_TRUE(r->res->fixed_columns == 2);
	EXPECT_EQ(r->res->fixed_type[0], VIRG_INT);
	EXPECT_EQ(r->res->fixed_type[1], VIRG_INT);

	virg_reader_row(v, r);
	int *x = (int*)&r->buffer[0];

	EXPECT_EQ(x[0], 0);
	EXPECT_EQ(x[1], 1);

	virg_reader_free(v, r);
	virg_vm_cleanup(v, r->vm);
	free(r);
	
	simpledb_clear(v);
}

}

