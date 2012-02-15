#ifndef VIRGINIAN
#define VIRGINIAN

/**
 * @file
 * @author Peter Bakkum <pbb7c@virginia.edu>
 *
 * @section Description
 *
 * This is the header file for the entire Virginian database. It contains nearly
 * all of the includes, macros, data structures, and function definitions for
 * the database and can be used with a static object file containing the
 * database's entire functionality.
 */

/**
 * @defgroup virginian Virginian Functions
 * @defgroup database Database File functions
 * @defgroup table Table Functions
 * @defgroup tablet Tablet Functions
 * @defgroup vm Virtual Machine Functions
 * @defgroup reader Tablet Reader Functions
 */

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
/// enables 64bit file operations
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>

/// shortcut for 2^10
#define VIRG_KB			1024
/// shortcut for 2^20
#define VIRG_MB			1048576
/// shortcut for 2^30
#define VIRG_GB			1073741824

/// acceptable error in floating point comparisons
#define VIRG_FLOAT_ERROR		0.0001

/// size of tablets, must be equal for the db file and the database
#define VIRG_TABLET_SIZE		(8 * VIRG_MB)
/// rows to allocate when a tablet is created
#define VIRG_TABLET_INITIAL_KEYS	256
/// rows to add when a tablet is full and rows still need to be added 
#define VIRG_TABLET_KEY_INCREMENT	(2048 * 128)
/// initial size of the fixed block
#define VIRG_TABLET_INITIAL_FIXED	0
/// initial size of the variable block
#define VIRG_TABLET_INITIAL_VARIABLE	0
/// size reserved for the variable block when the fixed is maxed out
#define VIRG_TABLET_MAXED_VARIABLE	(VIRG_TABLET_SIZE / 16)
/// initial area reserved for the variable size block of result tablets
#define VIRG_RESULT_INITIAL_VARIABLE	(512 * VIRG_KB)
/// initial disk slot meta informations to allocate for tablets
#define VIRG_TABLET_INFO_SIZE		16
/// meta information structs to add when all are used up and we need another
#define VIRG_TABLET_INFO_INCREMENT	32

/// maximum table columns supported
#define VIRG_MAX_COLUMNS 		16
/// maximum tables supported
#define VIRG_MAX_TABLES			16
/// maximum column name length supported
#define VIRG_MAX_COLUMN_NAME	16
/// maximum table name length supported
#define VIRG_MAX_TABLE_NAME		32

/// tablet slots to allocate in memory
#define VIRG_MEM_TABLETS		64
/// tablet slots to allocate in gpu memory
#define VIRG_GPU_TABLETS		2
/// maximum number of tables to read from supported in vm
#define VIRG_VM_TABLES			1
/// number of rows to process on the cpu in a block
#define VIRG_CPU_SIMD			64
/// buffer size to store a single row in virg_reader
#define VIRG_ROW_BUFFER			256
/// number of vm registers allocated
#define VIRG_REGS			16
/// number of vm global registers allocated
#define	VIRG_GLOBAL_REGS		16
/// maximum number of query statement opcodes allowed
#define	VIRG_OPS			32

/// cuda device selected
#define VIRG_CUDADEVICE				0
/// cuda threads per block
#define VIRG_THREADSPERBLOCK		128

/**
 * mask used for math operations dependent on the size of threads per block
 * with two's complement integers this should be the negative of the threads
 * per block variable above but I get casting warning when I don't use hex
 * that can probably be fixed
 */
#define VIRG_THREADSPERBLOCK_MASK	0xFFFFFF80
/// number of threads to use for the multicore cpu virtual machine
#define VIRG_MULTITHREADS		8

/// used to return a function failure
#define VIRG_FAIL		0
/// used to return a function success
#define VIRG_SUCCESS		1

/// convenience macro to return the min of the two inputs
#define VIRG_MIN(a, b)		(a < b ? a : b)
/// convenience macro to return the max of the two inputs
#define VIRG_MAX(a, b)		(a < b ? b : a)
/// convenience macro to verify a number is a power of 2
#define VIRG_ISPWR2(x)		((x != 0) && ((x & (~x + 1)) == x))

/// print out an error with the file and line
#define VIRG_ERROR(x)		fprintf(stderr, "::: Virginian error %s line %d:: " x "\n", __FILE__, __LINE__);

/// print an error and return failure if the condition is true
#define VIRG_CHECK(cond, args)												   \
	if(cond) {																   \
		VIRG_ERROR(args)													   \
		return VIRG_FAIL;													   \
	}

/// return failure and print it if there is an outstanding cuda error
#define VIRG_CUDCHK(desc) VIRG_CUDCHKCALL(cudaGetLastError(), desc)

/// return failure and print it if there in a cuda error returned from a call
#define VIRG_CUDCHKCALL(call, desc)											   \
	{ cudaError_t r;														   \
	if((r = call) != cudaSuccess) {											   \
		fprintf(stderr, "CUDA %s error: %s :: %s line %d\n", 				   \
			desc, cudaGetErrorString(r), __FILE__, __LINE__);				   \
		return VIRG_FAIL;													   \
	}}


/** VIRG_DEBUG_CHECK calls are like VIRG_CHECK calls but only get compiled in if
 * the VIRG_DEBUG macro is defined
 */
#ifdef VIRG_DEBUG
	#define VIRG_DEBUG_CHECK(cond, args) VIRG_CHECK(cond, args)
#else
	#define VIRG_DEBUG_CHECK(cond, args);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// enumeration and integer assignments of all virtual machine opcodes
typedef enum {
	OP_Table		= 0,
	OP_ResultColumn	= 1,
	OP_Parallel		= 2,
	OP_Finish		= 3,
	OP_Column		= 4,
	OP_Rowid		= 5,
	OP_Result		= 6,
	OP_Converge		= 7,
	OP_Invalid		= 8,
	OP_Cast			= 9,
	OP_Integer		= 10,
	OP_Float		= 11,
	OP_Le			= 12,
	OP_Lt			= 13,
	OP_Ge			= 14,
	OP_Gt			= 15,
	OP_Eq			= 16,
	OP_Neq			= 17,
	OP_Add			= 18,
	OP_Sub			= 19,
	OP_Mul			= 20,
	OP_Div			= 21,
	OP_And			= 22,
	OP_Or			= 23,
	OP_Not			= 24,
	OP_Nop			= 25
} virg_ops;


/// enumeration of all variable types used by the database
typedef enum {
	VIRG_INT	= 0,
	VIRG_INT64	= 1,
	VIRG_FLOAT	= 2,
	VIRG_DOUBLE	= 3,
	VIRG_CHAR	= 4,
	VIRG_STRING	= 5,
	VIRG_NULL	= 6
} virg_t;

/// used when operating on two types to get the more general type
virg_t virg_generalizetype(virg_t t1, virg_t t2);

/// size in bytes of variables types, indexed by their enumeration values
static const size_t virg_sizes[7] = {
	sizeof(int),			// 0
	sizeof(long long int),	// 1
	sizeof(float),			// 2
	sizeof(double),			// 3
	sizeof(char),			// 4
	(sizeof(char) * 4),		// 5
	0						// 6
};

/**
 * @brief Information associated with a tablet on disk
 *
 * This struct is used in a dynamically allocated array to store the locations
 * of tablets on disk. To find a tablet on disk, these will be iterated over.
 * Note that though a tablet is in disk it may also be in memory, possibly with
 * changes that have not been flushed to disk
 */
typedef struct {
	/// id of the tablet stored in this slot, valid only if used == 1
	unsigned	id;
	/// note whether or not there is a tablet stored in this disk slot
	int			used;
	/// the index of this struct in the array of tablet infos
	unsigned	disk_slot;
} virg_tablet_info;


/**
 * @brief Tablet meta information
 *
 * This stores the entire structure and all attributes of a tablet. It occupies
 * the first sizeof(virg_tablet_meta) bytes of the allocated tablet area, and is
 * used to track all associated information, including the next tablet in the
 * tablet string and whether or not this is the last tablet in the string. It
 * also has the number of columns stored and the offset beginnings of each
 * column, etc. This tablet meta struct is stored directly in constant memory
 * during GPU query execution. Note that this struct is 64-bit aligned and it
 * significantly hurts performance if this is not guaranteed.
 */
typedef struct {
	/// number of rows stored in this tablet
	unsigned	rows;
	/// variable type of the key column
	virg_t		key_type;
	/// stride of the variable type of the key column
	size_t		key_stride;
	/// stride of the key pointer variable type
	size_t		key_pointer_stride;

	// TODO make this a 64-bit
	/// tablet id
	unsigned	id;
	/// id of the next tablet in the tablet string
	unsigned	next;
	/// boolean indicating whether this is the last tablet in the tablet string
	int			last_tablet;
	/// boolean indicating whether or not this tablet is part of a table
	int			in_table;
	/// id of the table that this tablet is a part of, if applicable
	unsigned	table_id;

	/// relative ptr to the beginning of the key column
	size_t		key_block;
	/// relative ptr to the beginning of the key pointer column
	size_t		key_pointers_block;
	/// relative ptr to the beginning of the fixed-column area
	size_t 		fixed_block;
	/// relative ptr to the beginning of the variable-sized data area
	size_t		variable_block;
	/// total size of this tablet
	size_t		size;
	/// fixed-size rows that can be in this tablet without reorganizing columns
	unsigned	possible_rows;
	/// stride of fixed-size columns, including key and key pointer
	size_t		row_stride;

	/// number of fixed-size columns
	unsigned	fixed_columns;
	// TODO pad to 16 byte align the following two arrays
	/// name of each of the fixed-size columns
	char		fixed_name	[VIRG_MAX_COLUMNS][VIRG_MAX_COLUMN_NAME];
	/// types of the fixed-size columns
	virg_t		fixed_type	[VIRG_MAX_COLUMNS];
	/// size in bytes of each of the fixed-size columns
	size_t		fixed_stride	[VIRG_MAX_COLUMNS];
	/// relative pointer from fixed_block indicating the beginning of the column
	size_t		fixed_offset	[VIRG_MAX_COLUMNS];

	/// pointer to the disk info struct associated with this tablet
	virg_tablet_info	*info;
}__attribute__((aligned(64))) virg_tablet_meta;

/**
 * @brief Union used to store any of the possible types of virg_t in a single memory
 * location, used for the 4th argument for opcodes
 */
typedef union {
	int 			i;
	float			f;
	long long int	li;
	double			d;
	char			c;
	char			*s;
}__attribute__((aligned(8))) virg_var;

/**
 * @brief Opcode, including arguments
 *
 * This struct is used to store all the attributes of an opcode, including the 3
 * integer arguments and the fourth variable-type argument, in addition to the
 * integer opcode corresponding to an op in the virg_ops enum. The 4 arguments
 * may be option, and their use is entirely dependent on the current opcode
 * being executed.
 */
typedef struct {
	int			op;
	int			p1;
	int			p2;
	int			p3;
	virg_var	p4;
} virg_op;

/**
 * @brief Linked list node used to manage result tablets
 *
 * The multi-core and GPU virtual machines need to manage their result tablets
 * in a list that is independent of the content of the meta information of the
 * tablets, thus a linked list is used.
 */
typedef struct virg_result_node_{
	/// tablet id
	unsigned 			id;
	/// pointer to the next node of the linked list
	struct virg_result_node_	*next;
} virg_result_node;

/**
 * @brief State struct of the virtual machine context
 *
 * This function is used to manage the virtual machine execution for high-level
 * CPU execution, but further context is needed for the data parallel execution
 * portions. This struct includes the opcode program and pointers to the head
 * and tail nodes of the result tablet list.
 */
typedef struct {
	/// high-level program counter
	unsigned		pc;
	/// opcode program
	virg_op			stmt		[VIRG_OPS];
	/// number of opcodes in the opcode program
	unsigned		num_ops;
	/// high-level registers
	virg_var		global_reg	[VIRG_GLOBAL_REGS];
	/// types currently stored in the high-level registers
	virg_t			type		[VIRG_GLOBAL_REGS];
	/// table handles used for the query
	unsigned		table		[VIRG_VM_TABLES];
	/// number of table handles used
	unsigned		num_tables;
	/// pointer to the head node of the result tablet list
	virg_result_node	*head_result;
	/// pointer to the tail node of the result tablet list
	virg_result_node	*tail_result;
	/// used to return timing data
	float			timing1, timing2, timing3;
} virg_vm;

/**
 * @brief Lower-level GPU virtual machine context
 *
 * This is used to store the registers associated with virtual machine execution
 * on the GPU. Note that each thread manages its program counter independently,
 * and thus it is not included in this context.
 */
typedef struct {
	/// registers
	virg_var		reg			[VIRG_REGS];
	/// variable type currently stored in registers
	virg_t			type		[VIRG_REGS];
	/// size in bytes of the variable stored in registers
	size_t			stride		[VIRG_REGS];
}virg_vm_context;

/**
 * @brief Lower-level data parallel CPU virtual machine context
 *
 * This is the cache-efficient data structure used to store intermediate
 * information for CPU virtual machine execution. The VIRG_CPU_SIMD macro
 * specifies the size of the row blocks that are processed in one step on the
 * CPU. Note that the registers are blocked for data-parallel execution, so
 * the data used by different rows in the same register location is adjacent.
 * This is accomplished with the array of unions. Also note that the type and
 * stride of each register only needs to be stored once per register, because
 * everything is SIMD in a row block.
 */
typedef struct {
	/// global program counter
	unsigned		pc;
	/// program counter for individual rows in the block
	unsigned		row_pc [VIRG_CPU_SIMD];
	/// block-organized registers
	union {
		int			i	[VIRG_CPU_SIMD];
		long long int	li	[VIRG_CPU_SIMD];
		float		f	[VIRG_CPU_SIMD];
		double		d	[VIRG_CPU_SIMD];
		char		c	[VIRG_CPU_SIMD];
		char		*s	[VIRG_CPU_SIMD];
	} reg[VIRG_REGS];
	/// type stored in each register
	virg_t			type	[VIRG_REGS];
	/// size in bytes of the variable stored in each registers
	size_t			stride	[VIRG_REGS];
} virg_vm_simdcontext;

/**
 * @brief State of the currently open database
 *
 * This structure is used to manage the database file that is currently open in
 * the database. It is the head of the database file, is loaded into memory when
 * the database is opened, and is accessed in memory until the database is
 * closed, when it is written to the start of the table. This has a fixed size,
 * but the list of tablets in the database managed with virg_tablet_info structs
 * necessarily has a variable size, and is stored just after this on disk.
 *
 * Note that the fd (file descriptor) variable is meaningless when this data
 * is written to disk.
 */
typedef struct {
	/// number of tablets on disk
	unsigned		num_tablets;
	/// number of virg_tablet_info structs that have been allocated
	unsigned		alloced_tablets;
	/// used to assign unique ids to each data and result tablet
	unsigned		tablet_id_counter;
	/// size of this struct plus all the virg_tablet_info structs
	size_t			block_size;
	/// maps table id to name
	char			tables[VIRG_MAX_TABLES][VIRG_MAX_TABLE_NAME];
	/// maps table id to the id of its first tablet
	unsigned		first_tablet[VIRG_MAX_TABLES];
	/// maps table id to the id of its last tablet
	unsigned		last_tablet[VIRG_MAX_TABLES];
	/// tablet id of the current write location for adding rows to a table
	unsigned		write_cursor[VIRG_MAX_TABLES];
	/// number of tablets for each table
	unsigned		table_tablets[VIRG_MAX_TABLES];
	/// note which table slots have been used
	int				table_status[VIRG_MAX_TABLES];
	/// pointer to the block allocated to store virg_tablet_info structs
	virg_tablet_info	*tablet_info;
} virg_db;

/**
 * @brief State struct of the whole database
 *
 * This is used to manage the overall state of Virginian. It is used in almost
 * all function calls and must be initialized and closed with the appropriate
 * functions. This holds the tablet slot allocations, virg_db struct, and
 * virtual machine execution options.
 */
typedef struct {
	/// database file state
	virg_db			db;
	/// id of the tablet in each slot, valid only if status is above 0
	unsigned		tablet_slot_ids		[VIRG_MEM_TABLETS];
	/// use status of the tablet slot, 0 for unused, 1 for used, >1 for each lock
	int				tablet_slot_status	[VIRG_MEM_TABLETS];
	/// number of tablet slots which are unused
	unsigned		tablet_slots_taken;
	/// round-robin counter to kick out tablets
	unsigned		tablet_slot_counter;
	/// pointer to each main-memory tablet slot
	virg_tablet_meta	*tablet_slots		[VIRG_MEM_TABLETS];
	/// pointer to the beginning of the allocated gpu tablet slots
	void			*gpu_slots;
	/// mutex for multi-core manipulation of the tablet slots
	pthread_mutex_t		slot_lock;
	/// file descriptor for the open database
	int			dbfd;
	/// threads per block for gpu execution
	unsigned	threads_per_block;
	/// number of threads to use for multi-core cpu execution
	unsigned	multi_threads;
	/// enables multicore
	int			use_multi;
	/// enables gpu execution
	int			use_gpu;
	/// enables stream execution
	int			use_stream;
	/// enables mapped execution, only used if stream is false
	int			use_mmap;
} virginian;

/**
 * @brief Holds the arguments for the multicore GPU virtual machine
 *
 * The virtual machine is run directly from pthread_create(), which can only
 * pass a single argument to the function, so this struct is used to package
 * arguments together.
 */
typedef struct {
	virginian		*v;
	virg_vm			*vm;
	virg_tablet_meta	*tab;
	virg_tablet_meta	*res;
	unsigned		row;
	unsigned		num_rows;
	unsigned		num_tablets;
	unsigned		tablets_proced;
	pthread_mutex_t		tab_lock;
	pthread_mutex_t		res_lock;
} virg_vm_arg;

/**
 * @brief State of the results reader object
 *
 * This stores the current cursor location of a result reader and holds the
 * buffer through which row information is returned
 */
typedef struct {
	/// virtual machine context from which the results are taken
	virg_vm			*vm;
	/// pointer to the current results object
	virg_tablet_meta	*res;
	/// current result row
	unsigned		row;
	/// buffer to hold the contents of returned rows
	char			buffer	[VIRG_ROW_BUFFER];
} virg_reader;

/**
 * @brief Array of data structure sizes used for testing
 *
 * There are certain instances where data structures compile with different
 * sizes on gcc/icc versus nvcc because of different optimization criteria. For
 * example, structs will get rounded up to a power of 2 on one platform but not
 * the other. This creates bugs which are extremely difficult to diagnose. This
 * array is used by the testing code to ensure that all data types are identical
 * between cpu and gpu code.
 */
static const size_t virg_testsizes[15] = {
	sizeof(int),
	sizeof(float),
	sizeof(long long int),
	sizeof(double),
	sizeof(char),
	sizeof(virg_tablet_meta),
	sizeof(virg_tablet_info),
	sizeof(virg_var),
	sizeof(virg_vm),
	sizeof(virg_op),
	sizeof(virg_result_node),
	sizeof(virg_db),
	sizeof(virg_reader),
	sizeof(virg_vm_arg),
	sizeof(virginian)
};

int virg_db_alloc(virginian *v, virg_tablet_meta **meta, int id);
int virg_db_open(virginian *v, const char *file);
int virg_db_create(virginian *v, const char *file);
int virg_db_close(virginian *v);
int virg_db_clear(virginian *v, unsigned slot);
int virg_db_write(virginian *v, unsigned slot);
int virg_db_load(virginian *v, unsigned tablet_id, virg_tablet_meta **tab);
int virg_db_loadnext(virginian *v, virg_tablet_meta **tab);
int virg_db_findslot(virginian *v, unsigned *slot_);

int virg_table_addcolumn(virginian *v,
	unsigned table_id, const char *name, virg_t type);
int virg_table_create(virginian *v, const char *name, virg_t key_type);
int virg_table_insert(virginian *v, unsigned table_id, char *key,
	char *data, char *blob);
int virg_table_loadmem(virginian *v, unsigned table_id);
int virg_table_getid(virginian *v, const char* name, unsigned *id);
int virg_table_getcolumn(virginian *v, unsigned tid, const char* name,
	unsigned *id);
int virg_table_getcolumntype(virginian *v, unsigned tid, unsigned cid,
	virg_t *type);
int virg_table_getkeytype(virginian *v, unsigned tid, virg_t *type);
int virg_table_numrows(virginian *v, unsigned id, unsigned *rows);

int virg_tablet_addcolumn(virg_tablet_meta *tab, const char *name, virg_t type);
int virg_tablet_addrows(virginian *v, virg_tablet_meta *tab, unsigned rows);
int virg_tablet_addmaxrows(virginian *v, virg_tablet_meta *tab);
int virg_tablet_create(virginian *v, int *id, virg_t key_type,
	unsigned table_id);
int virg_tablet_check(virg_tablet_meta *t);
int virg_tablet_growfixed(virg_tablet_meta *tab, size_t size);
int virg_tablet_lock(virginian *v, unsigned tablet_id);
int virg_tablet_unlock(virginian *v, unsigned tablet_id);
int virg_tablet_addtail(virginian *v, virg_tablet_meta *head,
	virg_tablet_meta **tail, unsigned possible_rows);
int virg_tablet_remove(virginian *v, unsigned id);

int virg_vm_addop(virg_vm *vm, int op, int p1, int p2, int p3, virg_var p4);
int virg_vm_allocresult(virginian *v, virg_vm *vm,
	virg_tablet_meta **meta, virg_tablet_meta *template_);
int virg_vm_execute(virginian *v, virg_vm *vm);
int virg_vm_freeresults(virginian *v, virg_vm *vm);
virg_vm *virg_vm_init();
void virg_vm_cleanup(virginian *v, virg_vm *vm);
void virginia_single(virginian *v, virg_vm *vm, virg_tablet_meta *tab,
	virg_tablet_meta **res_, unsigned row, unsigned num_rows);
void *virginia_multi(void *arg_);
int virg_vm_cpu(virginian *v, virg_vm *vm, virg_tablet_meta **tab_,
	virg_tablet_meta **res, unsigned num_tablets);
int virg_vm_gpu(virginian *v, virg_vm *vm, virg_tablet_meta **tab_,
	virg_tablet_meta **res, unsigned num_tablets);
const size_t *virg_gpu_getsizes();
const size_t *virg_cpu_getsizes();

int virg_reader_init(virginian *v, virg_reader *r, virg_vm *vm);
int virg_reader_free(virginian *v, virg_reader *r);
int virg_reader_row(virginian *v, virg_reader *r);
int virg_reader_print(virg_reader *r);
int virg_reader_nexttablet(virginian *v, virg_reader *r);
int virg_reader_getrows(virginian *v, virg_reader *r, unsigned *rows_);

int virg_sql(virginian *v, const char *querystr, virg_vm *vm);

int virg_init(virginian *v);
int virg_close(virginian *v);
int virg_query(virginian *v, virg_reader **reader, const char *query);
void virg_release(virginian *v, virg_reader *reader);

size_t virg_sizeof(virg_t type);
const char* virg_opstring(int op);
void virg_print_tablet(virginian *v, virg_tablet_meta *m, const char* filename);
void virg_print_tablet_meta(virg_tablet_meta *m);
void virg_print_stmt(const virg_vm *vm);
void virg_print_slots(virginian *v);
void virg_print_tablet_info(virginian *v);
int virg_lock_sum(virginian *v);

/// global variable used to store the timer value after virg_timer_start()
extern struct timeval virg_starttime;
/// global variable used to store the timer value after virg_timer_stop()
extern struct timeval virg_endtime;

void virg_timer_start();
double virg_timer_stop();
double virg_timer_end(const char* label);

#ifdef __cplusplus
}
#endif
#endif

