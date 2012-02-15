#include "virginian.h"

/**
 * @ingroup reader
 * @brief Get the next result row
 *
 * Get the next result row from the reader and advance the reader to the next
 * row. This function places the content of the row in r->buffer and it is up to
 * the user to extract individual columns from the buffer. This function handles
 * moving to the next tablet as rows are read, and can be operated in a loop to
 * retrieve every single row until it returns VIRG_FAIL, indicating that the end
 * of the results have been reached. This function should only be called after
 * virg_reader_init().
 *
 * @param v     Pointer to the state struct of the database system
 * @param r     Pointer to the reader state
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_reader_row(virginian *v, virg_reader *r)
{
	if(r->res == NULL)
		return VIRG_FAIL;

	virg_tablet_meta *res = r->res;
	char *dest = &r->buffer[0];
	unsigned i = 0;
	char *fixed = (char*)res + res->fixed_block;

	// get columns one by one and place entire row in reader buffer
	for(i = 0; i < res->fixed_columns; i++) {
		size_t stride = res->fixed_stride[i];
		void *src = fixed + res->fixed_offset[i] + stride * r->row;
		memcpy(dest, src, stride);
		((int*)dest)[0] = ((int*)dest)[0];
		dest += stride;
	}

	r->row++;

	// check if we've reached the end of the tablet
	if(r->row >= res->rows) {
		// if last tablet do nothing
		if(res->last_tablet) {
			virg_tablet_unlock(v, res->id);
			r->res = NULL;
			return VIRG_FAIL;
		}
		// otherwise load the next one
		else
			virg_db_loadnext(v, &r->res);
		r->row = 0;
	}

	return VIRG_SUCCESS;
}

