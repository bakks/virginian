#include "virginian.h"
#include "test/test.h"
#include "test/queries.h"
#include "gsl/gsl_rng.h"
#include "gsl/gsl_randist.h"

namespace {

#define DBNAME "growthdb"
#define INCREMENT 10000
#define CEILING 50000

class GrowthTest : public VirginianTest {
	protected:

	virginian *v;
	gsl_rng *ran;
	int rows_added;

	GrowthTest()
	{
		unlink(DBNAME);
		v = (virginian*)malloc(sizeof(virginian));

		rows_added = 0;
		virg_init(v);
		virg_db_create(v, DBNAME);
		virg_table_create(v, "test", VIRG_INT);
		virg_table_addcolumn(v, 0, "uniformi", VIRG_INT);
		virg_table_addcolumn(v, 0, "normali5", VIRG_INT);
		virg_table_addcolumn(v, 0, "normali20", VIRG_INT);
		virg_table_addcolumn(v, 0, "uniformf", VIRG_FLOAT);
		virg_table_addcolumn(v, 0, "normalf5", VIRG_FLOAT);
		virg_table_addcolumn(v, 0, "normalf20", VIRG_FLOAT);

		const gsl_rng_type *type;
		type = gsl_rng_ranlxd2;
		ran = gsl_rng_alloc(type);
		gsl_rng_set(ran, time(0));

		AddRows(INCREMENT);
	}

	void AddRows(int rows)
	{
		int i;
		void *buff = malloc(4 * 6);
		int *buff_i = (int*)buff;
		float *buff_f = (float*)buff;

		for(i = 0; i < rows; i++) {
			buff_i[0] = (int)gsl_ran_flat(ran, -100, 100);
			buff_i[1] = (int)gsl_ran_gaussian(ran, 5);
			buff_i[2] = (int)gsl_ran_gaussian(ran, 20);
			buff_f[3] = (float)gsl_ran_flat(ran, -100, 100);
			buff_f[4] = (float)gsl_ran_gaussian(ran, 5);
			buff_f[5] = (float)gsl_ran_gaussian(ran, 20);
			virg_table_insert(v, 0, (char*)&i, (char*)buff, NULL);

		}

		free(buff);
		rows_added += rows;

		printf("%i,", rows_added);
		fflush(stdout);
	}

	virtual ~GrowthTest()
	{
		virg_db_close(v);
		virg_close(v);
		cudaThreadExit();
		gsl_rng_free(ran);
		unlink(DBNAME);
	}

	double RunSuite(int cached)
	{
		double x = 0;

		for(int i = 0; i < query_size; i++) {
			virg_vm *vm = virg_vm_init();
			virg_sql(v, queries[i], vm);
			virg_table_loadmem(v, 0);
			//virg_print_stmt(vm);

			if(!cached)
				virg_timer_start();

			virg_vm_execute(v, vm);

			if(!cached)
				x += virg_timer_stop();
			else
				x += vm->timing2;

			virg_vm_freeresults(v, vm);
		}

		return x / query_size;
	}
};

TEST_F(GrowthTest, Run) {
	double b, c, d;

	FILE *f = fopen("growth.dat", "w+");
	fprintf(f, "\"Rows\"\t\"Multi\"\t\"Mapped\"\t\"Cached\"\n");

	while(rows_added <= CEILING) {
		v->use_multi	= 1;
		v->use_gpu		= 0;
		v->use_mmap		= 0;
		b = RunSuite(0);

		v->use_multi	= 0;
		v->use_gpu		= 1;
		v->use_mmap		= 1;
		c = RunSuite(0);

		v->use_multi	= 0;
		v->use_gpu		= 1;
		v->use_mmap		= 0;
		d = RunSuite(1);

		fprintf(f, "%i\t%'.5f\t%'.5f\t%'.5f\n", rows_added, b, c, d);

		AddRows(INCREMENT);
		cudaDeviceSynchronize();
	}

	fclose(f);
	printf("\n");
}

}

