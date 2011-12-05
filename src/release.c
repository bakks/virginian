
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
 * This file contains the virg_release() function.
 */


/**
 * @ingroup virginian
 * @brief Cleans up after a SQL query
 *
 * Call this after you are done with the results of a SQL query to unlock and
 * purge the result tablets and release the reader. See the documentation of
 * virg_query() for an example.
 *
 * @param v Pointer to the state struct of the database system
 * @param reader Pointer to a pointer to a reader of results
 */
void virg_release(virginian *v, virg_reader *reader)
{
	virg_reader_free(v, reader);
	virg_vm_cleanup(v, reader->vm);
	free(reader);
}

