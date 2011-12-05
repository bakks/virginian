
#include "virginian.h"

/**
 * @file
 * @author Peter Bakkum <pbb7c@virginia.edu>
 *
 * @section License
 *
 * This software is provided "as is" and any expressed or implied warranties,
 * including, but not limited to, the implied warranties of merchantibility and
 * fitness for a particular purpose are disclaimed. In no event shall the
 * regents or contributors be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *      
 * @section Description 
 *
 * This file contains the virg_reader_init() function.
 */
    

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

