
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
 * This file contains the virg_tablet_growfixed() function.
 */
        

/**
 * @ingroup tablet
 * @brief Grows the fixed_block of a tablet by moving the variable block back
 *
 * Expands the fixed-size block of the tablet by moving the variable block back
 * farther. This is a convenience function for other tablet functions, and
 * leaves the tablet inconsistent because no virg_tablet_meta.possible_row or
 * column information is edited, so it should be followed up with these changes
 * to the tablet as well. Note that if the variable size block has a size of
 * zero, this is very easy, as we just move its location back in the meta
 * information. If not, however, we must perform a safe memory copy to transfer
 * its contents farther back in the tablet.
 *
 * @param tab	Pointer to the tablet to be changed
 * @param size	Additional size to be added to the fixed block
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_growfixed(virg_tablet_meta *tab, size_t size)
{
	// make sure you can add that much to the tablet
	VIRG_CHECK(tab->size + size > VIRG_TABLET_SIZE, "Too big to grow, the tablet must be split");

	// if the variable block has a zero size, we just move it back and return
	// otherwise we have to copy its contents
	if(tab->size == tab->variable_block) {
		tab->variable_block += size;
		tab->size += size;
		return VIRG_SUCCESS;
	}

	size_t new_variable = tab->variable_block + size;
	size_t variable_size = tab->size - tab->variable_block;

	char *dest = (char*)tab + new_variable;
	char *src = (char*)tab + tab->variable_block;

	// move variable-sized information
	memmove(dest, src, variable_size);

	tab->variable_block = new_variable;
	tab->size = new_variable + variable_size;

	return VIRG_SUCCESS;
}

