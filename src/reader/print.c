
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
 * This file contains the virg_reader_print() function.
 */
        

/**
 * @ingroup reader
 * @brief Print the contents of the reader row buffer to stdout
 *
 * Print the reader row buffer to stdout after a call to virg_reader_row(). This
 * function does not advance the reader to the next row or read the current row,
 * and should only be called after virg_reader_row().
 *
 * @param r     Pointer to the reader state
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_print(virg_reader *r)
{
	unsigned i;
	static int colwidth = 10;
	char *offset = &r->buffer[0];

	// for each column in the result row
	for(i = 0; i < r->res->fixed_columns; i++) {
		// handle each column type
		switch(r->res->fixed_type[i]) {
			case VIRG_INT:
				printf("%*i", colwidth, ((int*)offset)[0]);
				break;
			case VIRG_FLOAT:
				printf("%*.2f", colwidth, ((float*)offset)[0]);
				break;
			case VIRG_INT64:
				printf("%*lli", colwidth, ((long long int*)offset)[0]);
				break;
			case VIRG_DOUBLE:
				printf("%*.2f", colwidth, ((double*)offset)[0]);
				break;
			case VIRG_CHAR:
				printf("%*c", colwidth, offset[0]);
				break;
			default:
				VIRG_CHECK(1, "Can't print that type")
		}
		// move to the next area
		offset += r->res->fixed_stride[i];
	}

	printf("\n");
	return VIRG_SUCCESS;
}

