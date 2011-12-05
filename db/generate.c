#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <virginian.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

int main(int argc, char **argv)
{
	if(argc != 3) {
		fprintf(stderr, "%s <database name> <rows>\n", argv[0]);
		exit(1);
	}

	unlink(argv[1]);
	virginian virg;
	virginian *v = &virg;

	virg_init(v);
	virg_db_create(v, argv[1]);
	virg_table_create(v, "test", VIRG_INT);
	virg_table_addcolumn(v, 0, "uniformi", VIRG_INT);
	virg_table_addcolumn(v, 0, "normali5", VIRG_INT);
	virg_table_addcolumn(v, 0, "normali20", VIRG_INT);
	virg_table_addcolumn(v, 0, "uniformf", VIRG_FLOAT);
	virg_table_addcolumn(v, 0, "normalf5", VIRG_FLOAT);
	virg_table_addcolumn(v, 0, "normalf20", VIRG_FLOAT); 

	int i;
	void *buff = malloc(4 * 6);
	int *buff_i = (int*)buff;
	float *buff_f = (float*)buff;

	const gsl_rng_type *type;
	gsl_rng *ran;

	type = gsl_rng_ranlxd2;
	ran = gsl_rng_alloc(type);
	gsl_rng_set(ran, time(0));

	for(i = 0; i < atoi(argv[2]); i++) {
		buff_i[0] = (int)gsl_ran_flat(ran, -100, 100);
		buff_i[1] = (int)gsl_ran_gaussian(ran, 5);
		buff_i[2] = (int)gsl_ran_gaussian(ran, 20);
		buff_f[3] = (float)gsl_ran_flat(ran, -100, 100);
		buff_f[4] = (float)gsl_ran_gaussian(ran, 5);
		buff_f[5] = (float)gsl_ran_gaussian(ran, 20);
		virg_table_insert(v, 0, (char*)&i, buff, NULL);

		if(i % 10000 == 0) {
			printf("%i,", i);
			fflush(stdout);
		}
	}
	printf("\n");

	free(buff);

	virg_db_close(v);
	virg_close(v);
	gsl_rng_free(ran);

	return 0;
}

