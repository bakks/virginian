#include "virginian.h"

/**
 * @ingroup tablet
 * @brief Ensure that the tablet is internally consistent and conforms to expectation
 *
 * This is a debugging function used to check that the tablet has not been
 * corrupted, returning VIRG_SUCCESS if all tests are passed. This checks that
 * the key block, key pointers block, columns, and variable block are all where
 * they are expected to be and aligned given the meta information about columns
 * and key and column widths.
 *
 * @param t Pointer to the tablet to be checked
 * @return VIRG_SUCCESS or VIRG_FAIL depending on errors during the function
 * call
 */
int virg_tablet_check(virg_tablet_meta *t)
{
	unsigned pr = t->possible_rows;
	unsigned i;

	// key block starts at the end of the meta block
	assert(sizeof(virg_tablet_meta) == t->key_block);

	// key pointers block starts at the end of the key block
	assert(pr * t->key_stride + t->key_block == t->key_pointers_block);

	// fixed block starts at the end of the key pointers block
	assert(t->fixed_block + pr * (t->row_stride - t->key_stride - t->key_pointer_stride) == t->variable_block);

	// if there are columns in the tablet
	if(t->fixed_columns > 0) {
		// the offset of the first column is always 0
		assert(t->fixed_offset[0] == 0);

		// the offset of each column should be just after the last column's
		// block
		for(i = 1; i < t->fixed_columns; i++)
			assert(t->fixed_offset[i] == t->fixed_offset[i - 1] +
				pr * t->fixed_stride[i - 1]);

		// the variable block should be just after the last column's block
		assert(t->fixed_block + t->fixed_offset[t->fixed_columns-1] +
			pr * t->fixed_stride[t->fixed_columns-1] == t->variable_block);
	}
	// otherwise the fixed block has a zero size
	else
		assert(t->fixed_block == t->variable_block);

	// meta is 64-bit aligned
	assert((t->key_block & 0xFFFFFFC0) == t->key_block);

	// possible rows is a multiple of 16
	assert((t->possible_rows & 0xFFFFFFF0) == t->possible_rows);

	return VIRG_SUCCESS;
}

