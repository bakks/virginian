#include "virginian.h"
#include "test/test.h"
#include "test/queries.h"

namespace {

class TimingTest : public VirginianTest {
	protected:

	virginian *v;
	virg_reader *r;

	TimingTest()
	{
		v = (virginian*)malloc(sizeof(virginian));
		virg_init(v);
		virg_db_open(v, "../db/comparedb");
	}

	virtual ~TimingTest()
	{
		virg_close(v);
		cudaThreadExit();
		free(v);
	}

};


TEST_F(TimingTest, QuerySuite) {
	double a, b, c, d;
	double data = 0, cached = 0, results = 0, mapped = 0;
	double stats[3][4];

	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 4; j++)
			stats[i][j] = 0;

	FILE *f = fopen("perfhist.dat", "w+");

	fprintf(f, "\"Query\"\t\"Single Core\"\t\"Multi-Core\"\t\"Mapped GPU\"\t\"Cached GPU\"\n");

	for(int i = 0; i < query_size; i++) {
		virg_vm *vm = virg_vm_init();
	    virg_sql(v, queries[i], vm);

		//virg_print_stmt(vm);

		v->use_multi	= 0;
		v->use_gpu		= 0;
		v->use_mmap		= 0;
		virg_table_loadmem(v, 0);
		virg_timer_start();
		virg_vm_execute(v, vm);
		a = virg_timer_stop();
		virg_vm_freeresults(v, vm);

		v->use_multi	= 1;
		v->use_gpu		= 0;
		v->use_mmap		= 0;
		virg_table_loadmem(v, 0);
		virg_timer_start();
		virg_vm_execute(v, vm);
		b = virg_timer_stop();
		virg_vm_freeresults(v, vm);

		v->use_multi	= 0;
		v->use_gpu		= 1;
		v->use_mmap		= 0;
		virg_table_loadmem(v, 0);
		//virg_timer_start();
		virg_vm_execute(v, vm);
		//c = virg_timer_stop();
		c = vm->timing2;
		virg_vm_freeresults(v, vm);
		data += vm->timing1;
		cached += vm->timing2;
		results += vm->timing3;

		v->use_multi	= 0;
		v->use_gpu		= 1;
		v->use_mmap		= 1;
		virg_table_loadmem(v, 0);
		virg_timer_start();
		virg_vm_execute(v, vm);
		d = virg_timer_stop();
		virg_vm_freeresults(v, vm);
		mapped += d;

		fprintf(f, "%i\t%'.5f\t%'.5f\t%'.5f\t%'.5f\t\n", i, a, b, d, c);
		fprintf(stdout, "%i\t%'.5f\t%'.5f\t%'.5f\t%'.5f\t\n", i, a, b, d, c);

		int group = i % 2;
		stats[group][0] += a;
		stats[group][1] += b;
		stats[group][2] += d;
		stats[group][3] += c;
	}

	fclose(f);

	data /= query_size;
	cached /= query_size;
	results /= query_size;
	mapped /= query_size;

	f = fopen("mappedcomp.dat", "w+");
	fprintf(f, "\" \"\t\"Serial\"\t\"Mapped\"\t\"Cached\"\n");
	fprintf(f, "\"Data Transfer\"\t%'.5f\t0\t0\n", data);
	fprintf(f, "\"Kernel Execution\"\t%'.5f\t0\t%'.5f\n", cached, cached);
	fprintf(f, "\"Results Transfer\"\t%'.5f\t0\t0\n", results);
	fprintf(f, "\"Mapped Kernel\"\t0\t%'.5f\t0\n", mapped);
	fclose(f);

	f = fopen("table1.dat", "w+");

	for(int i = 0; i < 2; i++)
		for(int j = 0; j < 4; j++)
			stats[i][j] /= (query_size / 2);

	for(int i = 0; i < 4; i++)
		stats[2][i] = (stats[0][i] + stats[1][i]) / 2;

	fprintf(f, "\t\tSingle\tMulti\tMapped\tCached\n");
	fprintf(f, "Integer\t\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[0][0], stats[0][1], stats[0][2], stats[0][3]);
	fprintf(f, "Floating Pt.\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[1][0], stats[1][1], stats[1][2], stats[1][3]);
	fprintf(f, "All\t\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[2][0], stats[2][1], stats[2][2], stats[2][3]);

	fclose(f);

	f = fopen("table2.dat", "w+");
	fprintf(f, "\t\tOver Single\tOver Multi\n");
	fprintf(f, "\t\tMapped\tCached\tMapped\tCached\n");
	fprintf(f, "Integer\t\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[0][0] / stats[0][2], stats[0][0] / stats[0][3],
		stats[0][1] / stats[0][2], stats[0][1] / stats[0][3]);
	fprintf(f, "Floating Pt.\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[1][0] / stats[1][2], stats[1][0] / stats[1][3],
		stats[1][1] / stats[1][2], stats[1][1] / stats[1][3]);
	fprintf(f, "All\t\t%'.3f\t%'.3f\t%'.3f\t%'.3f\t\n",
		stats[2][0] / stats[2][2], stats[2][0] / stats[2][3],
		stats[2][1] / stats[2][2], stats[2][1] / stats[2][3]);
	fclose(f);
}

TEST_F(TimingTest, VerifySuite) {

	virg_tablet_meta *tab;

	virg_db_load(v, v->db.first_tablet[0], &tab);
	//virg_print_tablet(v, tab, "test.csv");
	virg_tablet_unlock(v, tab->id);

	for(int i = 0; i < 10; i++) {
		for(int j = 0; j < 4; j++) {
			switch(j) {
				case 0: 
					v->use_multi 	= 0;
					v->use_gpu 		= 0;
					v->use_mmap 	= 0;
					break;

				case 1: 
					v->use_multi 	= 1;
					v->use_gpu 		= 0;
					v->use_mmap 	= 0;
					break;

				case 2: 
					v->use_multi 	= 0;
					v->use_gpu 		= 1;
					v->use_mmap 	= 0;
					break;

				case 3: 
					v->use_multi 	= 0;
					v->use_gpu 		= 1;
					v->use_mmap 	= 1;
					break;
			}

			virg_reader *r;
			virg_query(v, &r, queries[i]);

			//virg_print_stmt(r->vm);

			char buffer[32];
			sprintf(&buffer[0], "output.%i.%i.csv", i, j);

			//fprintf(stderr, "output.%i.%i.csv", i, j);
			//virg_print_tablet(v, r->res, &buffer[0]);
			//virg_print_tablet_meta(r->res);

			int count = 0;

			while(virg_reader_row(v, r) != VIRG_FAIL) {
				count++;
				unsigned *ru = (unsigned*)r->buffer;
				int *ri = (int*)r->buffer;
				float *rf = (float*)r->buffer;
				EXPECT_TRUE(ru[0] < 50000000);
				if(ru[0] >= 50000000)
					printf(":::::%u   %i   %i\n", ru[0], count, i);

				switch(i) {
					case 0:
						EXPECT_TRUE(ri[1] > 60);
						EXPECT_TRUE(ri[2] < 0);

						if(ri[1] <= 60 || ri[2] >= 0) {
							printf("::%i:: %i %u %i %i    %u\n",
								j, count, ru[0], ri[1], ri[2], r->row);
							virg_print_tablet_meta(r->res);
							exit(1);
						}
						break;

					case 1:
						EXPECT_TRUE(rf[1] > 60.0);
						EXPECT_TRUE(rf[2] < 0.0);

						if(rf[1] <= 60 || rf[2] >= 0) {
							printf("::%i:: %i %u %f %f    %u\n",
								j, count, ru[0], rf[1], rf[2], r->row);
							virg_print_tablet_meta(r->res);
						}
						break;

					case 2:
						EXPECT_TRUE(ri[1] > -60);
						EXPECT_TRUE(ri[2] < 5);
						break;

					case 3:
						EXPECT_TRUE(rf[1] > -60.0);
						EXPECT_TRUE(rf[2] < 5.0);
						break;

					case 4:
						EXPECT_TRUE(ri[2] + 40 > ri[1] - 10);
						break;

					case 5:
						EXPECT_TRUE(rf[2] + 40.0 > rf[1] - 10.0);
						break;

					case 6:
						EXPECT_TRUE(ri[1] * ri[2] >= -5);
						EXPECT_TRUE(ri[1] * ri[2] <= 5);
						break;

					case 7:
						EXPECT_TRUE(rf[1] * rf[2] >= -5.00001);
						EXPECT_TRUE(rf[1] * rf[2] <= 5.00001);
						break;

					case 8:
						EXPECT_TRUE((ri[1] >= -1 && ri[1] <= 1)
							|| (ri[2] >= -1 && ri[2] <= 1) 
							|| (ri[3] >= -1 && ri[3] <= 1));
						break;

					case 9:
						if(!((rf[1] >= -1.0 && rf[1] < 1.0)
									|| (rf[2] > -1.0 && rf[2] < 1.0)
									|| (rf[3] > -1.0 && rf[3] < 1.0)))
						printf("%i  %f  %f  %f\n", ri[0], rf[1], rf[2], rf[3]);

						EXPECT_TRUE(   (rf[1] > -1.0 && rf[1] < 1.0)
									|| (rf[2] > -1.0 && rf[2] < 1.0)
									|| (rf[3] > -1.0 && rf[3] < 1.0));
						break;
				}
			}

			virg_release(v, r);
		}
	}
}

} // namespace

