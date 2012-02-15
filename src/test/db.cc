#include "virginian.h"
#include "test/test.h"

namespace {

class DBTest : public VirginianTest {
	protected:
	
	void CheckDBIntegrity(virg_db *db) {
		ASSERT_EQ(db->block_size, sizeof(virg_db) + sizeof(virg_tablet_info) * db->alloced_tablets);

		for(unsigned i = 0; i < db->alloced_tablets; i++) {
			ASSERT_TRUE(db->tablet_info[i].used == 0 || db->tablet_info[i].used == 1);
			if(db->tablet_info[i].used == 1)
				EXPECT_EQ(db->tablet_info[i].disk_slot, i);
		}
	}
};


TEST_F(DBTest, DBManipulation) {
	virginian *v = simpledb_create();
	CheckDBIntegrity(&v->db);

	simpledb_addrows(v, 1);
	CheckDBIntegrity(&v->db);

	simpledb_addrows(v, 10);
	CheckDBIntegrity(&v->db);

	simpledb_addrows(v, 100000);
	CheckDBIntegrity(&v->db);

	simpledb_addrows(v, 100000);
	CheckDBIntegrity(&v->db);

	simpledb_addrows(v, 100000);
	CheckDBIntegrity(&v->db);

	simpledb_clear(v);
}

}

