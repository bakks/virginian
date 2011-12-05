
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
 * This file contains the virg_close() function.
 */


/**
 * @ingroup virginian
 * @brief Closes the Virginian database by freeing allocations
 *
 * Frees all of the allocations made in virg_init(). This must be called after
 * you are finished using the database to prevent memory leaks. If a database
 * file is still open when this is called, then virg_db_close() is also called.
 *
 * @param v Pointer to the state struct of the database system
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_close(virginian *v)
{
	int i;
	cudaError_t r;

	virg_db_close(v);

	// free each tablet slot, use cuda free depending on if its pinned
	for(i = 0; i < VIRG_MEM_TABLETS; i++) {
#ifndef VIRG_NOPINNED
		r = cudaFreeHost(v->tablet_slots[i]);
		VIRG_CHECK(r != cudaSuccess, "Problem freeing slot")
#else
		free(v->tablet_slots[i]);
#endif
	}

	// free gpu slots
	r = cudaFree(v->gpu_slots);
	VIRG_CHECK(r != cudaSuccess, "Problem freeing slot")

	VIRG_CHECK(pthread_mutex_destroy(&v->slot_lock), "Could not destroy mutex")

	return VIRG_SUCCESS;
}

