extern "C" {
#include "virginian.h"
}

#include "test/test.h"

namespace {

class TableTest : public VirginianTest {
	protected:
	
	void CheckTabletIntegrity(virg_tablet_meta *t)
	{
		ASSERT_EQ(t->possible_rows * t->key_stride + sizeof(virg_tablet_meta), t->key_pointers_block);
		ASSERT_EQ(t->key_pointers_block + t->key_pointer_stride * t->possible_rows, t->fixed_block);
		ASSERT_EQ(t->fixed_block + t->possible_rows * (t->row_stride - t->key_stride - t->key_pointer_stride), t->variable_block);

		if(t->fixed_columns != 0) {
			ASSERT_EQ(t->fixed_block + t->fixed_offset[t->fixed_columns-1] + t->possible_rows * t->fixed_stride[t->fixed_columns-1], t->variable_block);
		}
		else {
			ASSERT_EQ(t->fixed_block, t->variable_block);
		}
	}

	void CheckTableIntegrity(virginian *v, unsigned table_id)
	{
		// check tablet slot statuses
		// this should really be in a memory management test class
		int empty = 0;
		for(int j = 0; j < VIRG_MEM_TABLETS; j++)
			if(v->tablet_slot_status[j] == 0)
				empty++;

		ASSERT_TRUE(VIRG_MEM_TABLETS == v->tablet_slots_taken + empty);
		ASSERT_TRUE(v->tablet_slot_counter < VIRG_MEM_TABLETS);

		// iterate through tablets, checking integrity
		virg_tablet_meta *tab;
		unsigned tab_id = v->db.first_tablet[table_id];

		virg_db_load(v, tab_id, &tab);
		CheckTabletIntegrity(tab);

		while(!tab->last_tablet) {
			virg_db_loadnext(v, &tab);
			CheckTabletIntegrity(tab);
		}

		virg_tablet_unlock(v, tab->id);
	}
};


TEST_F(TableTest, TableManipulation) {
	virginian *v = simpledb_create();
	CheckTableIntegrity(v, 0);

	simpledb_addrows(v, 1);
	CheckTableIntegrity(v, 0);

	simpledb_addrows(v, 10);
	CheckTableIntegrity(v, 0);

	simpledb_addrows(v, 100000);
	CheckTableIntegrity(v, 0);

	simpledb_addrows(v, 100000);
	CheckTableIntegrity(v, 0);

	simpledb_addrows(v, 100000);
	CheckTableIntegrity(v, 0);

	simpledb_clear(v);
}

}

