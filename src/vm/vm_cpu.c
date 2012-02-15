#include "virginian.h"

/// Used in testing to return size test array from gpu-compiled code
const size_t *virg_cpu_getsizes()
{
	return &virg_testsizes[0];
}

/**
 * @ingroup vm
 * @brief Chooses and prepares for execution on one or multiple CPU cores
 *
 * This function is intended to make the virg_vm_execute() function by making
 * the choice between executing with a single or multiple CPU cores transparent
 * based on the virginian.use_multi. If this value is false, we loop and call
 * virginia_single for every tablet to be processes. Otherwise, we create
 * virginian.multi_threads threads which greedily process as many data tablets
 * as they can and wait for them to finish before returning. If num_tablets is
 * 0, then there is no restriction on how many data tablets will be processed in
 * this function.
 *
 * @param v		Pointer to the state struct of the database system
 * @param vm	Pointer to the context struct of the virtual machine
 * @param tab	Pointer to the pointer to the current data tablet to process
 * @param res	Pointer to the pointer to the current result tablet
 * @param num_tablets The number of tablets to process during this call, 0 for
 * as many as possible
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_vm_cpu(virginian *v, virg_vm *vm, virg_tablet_meta **tab, virg_tablet_meta **res, unsigned num_tablets)
{
	unsigned proced = 0;

	// if the virginian struct is set to execute using only a single core
	if(!v->use_multi) {
		// infinite loop
		while(1) {
			// add an extra lock to the current data and result tablets that
			// will be released at a lower level within virginia_single()
			virg_tablet_lock(v, tab[0]->id);
			virg_tablet_lock(v, res[0]->id);

			// single core execution function
			virginia_single(v, vm, tab[0], res, 0, tab[0]->rows);

			// we've processed another tablet
			proced++;

			// break out if we've processed enough tablets
			if((num_tablets != 0 && proced >= num_tablets) || tab[0]->last_tablet)
				break;

			// load the next data tablet
			virg_db_loadnext(v, tab);
		}
	}
	else {
		pthread_t thread[v->multi_threads];

		// copy values into the argument structure that must be used with
		// pthread_create()
		virg_vm_arg arg;
		arg.v = v;
		arg.vm = vm;
		arg.tab = tab[0];
		arg.res = res[0];
		arg.row = 0;
		arg.num_rows = 0;
		arg.num_tablets = num_tablets;
		arg.tablets_proced = 0;

		// mutexes for dealing with the current last tablet and result tablets
		VIRG_CHECK(pthread_mutex_init(&arg.tab_lock, NULL), "Could not init mutex")
		VIRG_CHECK(pthread_mutex_init(&arg.res_lock, NULL), "Could not init mutex")

		// create tablet processing threads
		unsigned i;
		for(i = 0; i < v->multi_threads; i++)
			pthread_create(&thread[i], NULL, virginia_multi, (void*) &arg);

		// wait for all threads to run out of data to process and finish
		for(i = 0; i < v->multi_threads; i++)
			pthread_join(thread[i], NULL);

		tab[0] = arg.tab;
		res[0] = arg.res;

		VIRG_CHECK(pthread_mutex_destroy(&arg.res_lock), "Could not destroy mutex")
		VIRG_CHECK(pthread_mutex_destroy(&arg.tab_lock), "Could not destroy mutex")
	}
	
	return VIRG_SUCCESS;
}

