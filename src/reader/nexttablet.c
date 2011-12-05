
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
 * This file contains the virg_reader_nexttablet() function.
 */
        

/**
 * @ingroup reader
 * @brief Move the reader to the next tablet
 *
 * Move the reader state onto the next result tablet, resetting the row mark to
 * the beginning of that next tablet. Returns VIRG_FAIL if the reader is already
 * at the last tablet.
 *
 * @param v     Pointer to the state struct of the database system
 * @param r     Pointer to the reader state
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_nexttablet(virginian *v, virg_reader *r)
{
	VIRG_CHECK(r->res->last_tablet, "Reached the last tablet");
	r->row = 0;

	return virg_db_loadnext(v, &r->res);
}

