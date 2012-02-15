#include "virginian.h"

/**
 * @brief Generalize two data types
 *
 * When an operation is performed between disparate data types the result
 * should be cast to the more general of the types. For instance, when an int
 * is multiplied by a float, the result should be a float.
 *
 * @param type1 The first type
 * @param type2 The second type
 * @return The generalized type
 */
virg_t virg_generalizetype(virg_t t1, virg_t t2)
{
	// if the types are equal it doesnt matter
	if(t1 == t2)
		return t1;

	// if we are dealing with numerical data types
	if(t1 <= VIRG_DOUBLE && t2 <= VIRG_DOUBLE)
		return VIRG_MAX(t1, t2);

	// else undefined, die
	assert(0);
}

/**
 * @brief Get the size of a type given its enumerated value
 *
 * Variable types are stored in Virginian using one of the enumerated values of
 * virg_t. To get the size in bytes of one of these types, this function is used
 * to access the static virg_sizes[] array.
 *
 * @param type Integer value of the enumerated variable type
 * @return Size in bytes as a size_t
 */
size_t virg_sizeof(virg_t type)
{
	return virg_sizes[type];
}

struct timeval virg_starttime;
struct timeval virg_endtime;

/**
 * @brief Start the global timer
 *
 * Uses the gettimeofday() system call to set the value of virg_starttime.
 */
void virg_timer_start()
{
        gettimeofday(&virg_starttime, NULL);
}

/**
 * @brief Returns the time since the timer's start
 *
 * Subtracts the current value returned by gettimeofday() to that set in the
 * virg_timer_start() function. Note that this doesn't actually stop the timer,
 * as it does not change the value of virg_starttime.
 *
 * @return The time in seconds since virg_timer_start() was called
 */
double virg_timer_stop()
{
        gettimeofday(&virg_endtime, NULL);
        double start = virg_starttime.tv_sec +
                (double)virg_starttime.tv_usec * .000001;
        double end = virg_endtime.tv_sec +
                (double)virg_endtime.tv_usec * .000001;
        return (end - start);
}

/**
 * @brief Like virg_timer_stop() but prints the time
 *
 * This function is intended to make it easy to measure parts of program
 * execution by printing out labels and times. It calls virg_timer_stop() and
 * prints out the label and the value returned.
 *
 * @param label The character string to label the result with
 * @return The time in seconds since virg_timer_start() was called
 */
double virg_timer_end(const char* label)
{
        double time = virg_timer_stop();
        printf("%s time: %f seconds\n", label, time);
        return time;
}

/**
 * @brief Print out a string of tablets to disk
 *
 * @param v Virginian state struct
 * @param m A pointer to the tablet
 * @param filename Name of the output file
 */
void virg_print_tablet(virginian *v, virg_tablet_meta *m, const char *filename)
{
	FILE * f = fopen(filename, "w+");

	for(unsigned i = 0; i < m->fixed_columns; i++)
		fprintf(f, "%s,", m->fixed_name[i]);
	fprintf(f, "\n");

	virg_tablet_lock(v, m->id);

	while(1) {
		for(unsigned i = 0; i < m->rows; i++) {
			switch(m->key_type) {
				case VIRG_INT :
					fprintf(f, "%*i", 12, ((int*)((char*)m + m->key_block +
						m->key_stride * i))[0]);
					break;
				case VIRG_FLOAT :
					fprintf(f, "%*f", 12, ((float*)((char*)m + m->key_block +
						m->key_stride * i))[0]);
					break;
				case VIRG_DOUBLE:
					fprintf(f, "%*f", 12, ((double*)((char*)m + m->key_block +
						m->key_stride * i))[0]);
					break;
				case VIRG_INT64:
					fprintf(f, "%*lld", 12, ((long long int*)((char*)m + m->key_block +
						m->key_stride * i))[0]);
					break;
				case VIRG_CHAR:
					fprintf(f, "%*c", 12, ((char*)((char*)m + m->key_block +
						m->key_stride * i))[0]);
					break;
				case VIRG_STRING:
					fprintf(f, "%*s", 12, (char*)((char*)m + m->key_block +
						m->key_stride * i));
					break;
				case VIRG_NULL:
					break;
			}

			for(unsigned j = 0; j < m->fixed_columns; j++)
				switch(m->fixed_type[j]) {
					case VIRG_INT :
						fprintf(f, "%*i", 12, ((int*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
						break;
					case VIRG_FLOAT :
						fprintf(f, "%*f", 12, ((float*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
						break;
					case VIRG_DOUBLE:
						fprintf(f, "%*f", 12, ((double*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
						break;
					case VIRG_INT64:
						fprintf(f, "%*lld", 12, ((long long int*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
						break;
					case VIRG_CHAR:
						fprintf(f, "%*c", 12, ((char*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
						break;
					case VIRG_STRING:
						fprintf(f, "%*s", 12, (char*)((char*)m + m->fixed_block +
							m->fixed_offset[j] + m->fixed_stride[j] * i));
						break;
					case VIRG_NULL:
						break;
				}
			fprintf(f, "\n");
		}

		if(!m->last_tablet)
			virg_db_loadnext(v, &m);
		else
			break;
	}

	virg_tablet_unlock(v, m->id);

	fclose(f);
}

/**
 * @brief Print out the meta information of a tablet
 *
 * This is indended to help debug code by printing out the meta block of a
 * tablet and a few of its fixed-size rows.
 *
 * @param m A pointer to the tablet
 */
void virg_print_tablet_meta(virg_tablet_meta *m)
{
	printf("== virg_tablet_meta == %p ============\n", (void*)m);

	printf(" rows:\t\t\t%u\n", m->rows);
	printf(" key_type:\t\t%i\n", m->key_type);
	printf(" key_stride:\t\t%u\n", (unsigned)m->key_stride);
	printf(" id:\t\t\t%u\n", m->id);
	printf(" next:\t\t\t%u\n", m->next);
	printf(" last_tablet:\t\t%i\n", m->last_tablet);
	printf(" key_block:\t\t%u\n", (unsigned)m->key_block);
	printf(" key_pointers_block:\t%u\n", (unsigned)m->key_pointers_block);
	printf(" fixed_block:\t\t%u\n", (unsigned)m->fixed_block);
	printf(" variable_block:\t%u\n", (unsigned)m->variable_block);
	printf(" size:\t\t\t%u\n", (unsigned)m->size);
	printf(" possible_rows:\t\t%u\n", m->possible_rows);
	printf(" fixed_columns:\t\t%u\n", m->fixed_columns);

	unsigned i, j;

	printf(" fixed_name\n");
	for(i = 0; i < m->fixed_columns; i++)
		printf("%s,", m->fixed_name[i]);
	printf("\n");

	printf(" fixed_type\n");
	for(i = 0; i < m->fixed_columns; i++)
		printf("%u,", m->fixed_type[i]);
	printf("\n");

	printf(" fixed_stride\n");
	for(i = 0; i < m->fixed_columns; i++)
		printf("%u,", (unsigned)m->fixed_stride[i]);
	printf("\n");

	printf(" fixed_offset\n");
	for(i = 0; i < m->fixed_columns; i++)
		printf("%u,", (unsigned)m->fixed_offset[i]);
	printf("\n    ===========      \n");

	for(i = 0; i < m->fixed_columns; i++)
		printf("%*s", 12, m->fixed_name[i]);
	printf("\n");

	for(i = 0; i < 20; i++) {
		for(j = 0; j < m->fixed_columns; j++)
			switch(m->fixed_type[j]) {
				case VIRG_INT :
					printf("%*i", 12, ((int*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
					break;
				case VIRG_FLOAT :
					printf("%*f", 12, ((float*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
					break;
				case VIRG_DOUBLE:
					printf("%*f", 12, ((double*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
					break;
				case VIRG_INT64:
					printf("%*lld", 12, ((long long int*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
					break;
				case VIRG_CHAR:
					printf("%*c", 12, ((char*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i))[0]);
					break;
				case VIRG_STRING:
					printf("%*s", 12, (char*)((char*)m + m->fixed_block + m->fixed_offset[j] + m->fixed_stride[j] * i));
					break;
				default:
					break;
			}
		printf("\n");
	}

	printf("------------------------------------------------\n");
}

/**
 * @brief Print the opcode program of a virtual machine context
 *
 * This function loops through all the opcodes of an opcode query plan in a
 * virtual machine context and prints the opcode as a string, the 3 integer
 * arguments of the opcode, and the integer value of its 4th argument.
 *
 * @param vm The virtual machine context to use
 */
void virg_print_stmt(const virg_vm *vm)
{
	unsigned i;
	char buff[16];

	fprintf(stderr, "== stmt ==========================================\n");
	for(i = 0; i < vm->num_ops; i++) {
		virg_op op = vm->stmt[i];

		switch(op.op) {
			case OP_ResultColumn :
				sprintf(&buff[0], "%*s", 15, op.p4.s);
				break;

			case OP_Float :
				sprintf(&buff[0], "%*f", 15, op.p4.f);
				break;

			default :
				sprintf(&buff[0], "%*i", 15, op.p4.i);
		}

		fprintf(stderr, " %*i: %-*s%*i%*i%*i%s\n", 3, i, 14, virg_opstring(op.op),
			5, op.p1, 5, op.p2, 5, op.p3, &buff[0]);
	}
	fprintf(stderr, "--------------------------------------------------\n");
}

/**
 * @brief Print out the status of the main-memory tablet slots
 *
 * This function is intended to help debug tablet loading and locking/unlocking.
 * It loops through each of the main memory tablet slots and prints its status
 * to stdout. A 0 indicates the slot is unused, and a 1 or above indicates that
 * a tablet is currently loaded into the slot. As read-locks are added to the
 * tablet its status is incremented. It is decremented as locks are released.
 *
 * @param v The state struct of the Virginian database containing the tablet
 * slots
 */
void virg_print_slots(virginian *v)
{
        fprintf(stderr, "== slots ======================================\n");
        fprintf(stderr, " used   ");
        unsigned i;
        for(i = 0; i < VIRG_MEM_TABLETS; i++)
                fprintf(stderr, "%i,", v->tablet_slot_status[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, " id     ");
        for(i = 0; i < VIRG_MEM_TABLETS; i++)
		if(v->tablet_slot_status[i] == 0)
			fprintf(stderr, ",");
		else
			fprintf(stderr, "%u,", v->tablet_slot_ids[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, "-----------------------------------------------\n");
}

/**
 * @brief Print out the status of the on-disk tablet slots
 *
 * This function is intended to help debug the loading and writing of tablets
 * from and to disk. It prints out the on-disk tablet slots and the ids of
 * tablets that are using them to stdout.
 *
 * @param v The state struct of the Virginian database containing the tablet
 * slots
 */
void virg_print_tablet_info(virginian *v)
{
	virg_db *db = &v->db;

	unsigned i;
        fprintf(stderr, "== tablet info ================================\n");
	fprintf(stderr, " ");

	for(i = 0; i < db->alloced_tablets; i++)
		if(db->tablet_info[i].used)
			fprintf(stderr, "%u,", db->tablet_info[i].id);
		else
			fprintf(stderr, "-,");

	fprintf(stderr, "\n");
        fprintf(stderr, "-----------------------------------------------\n");
}

/**
 * @brief Returns the total number of read-locks on main-memory tablet slots
 *
 * @param v The state struct of the Virginian database containing the tablet slots
 * @return The total number of locks
 */
int virg_lock_sum(virginian *v)
{
	int x = 0, i;

	for(i = 0; i < VIRG_MEM_TABLETS; i++)
		if(v->tablet_slot_status[i] > 1)
			x += v->tablet_slot_status[i] - 1;

	return x;
}

