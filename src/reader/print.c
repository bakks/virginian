#include "virginian.h"

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

