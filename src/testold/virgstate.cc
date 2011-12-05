
extern "C" {
#include "virginian.h"
}

#include "gtest/gtest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

namespace {

class VirgStateTest : public ::testing::Test {
};


TEST_F(VirgStateTest, VirginianCreation) {
	virginian v;

	virg_init(&v);

	for(int i = 0; i < VIRG_MEM_TABLETS; i++)
		ASSERT_TRUE(v.tablet_slots[i] != NULL);

	ASSERT_TRUE(v.gpu_slots != NULL);

	cudaThreadExit();
}

TEST_F(VirgStateTest, DataSizes) {
	EXPECT_TRUE(sizeof(int) == 4);
	EXPECT_TRUE(sizeof(float) == 4);
	EXPECT_TRUE(sizeof(char) == 1);
	EXPECT_TRUE(sizeof(double) == 8);
	EXPECT_TRUE(sizeof(long long int) == 8);

	const size_t *local = &virg_testsizes[0];
	const size_t *gpu = virg_gpu_getsizes();
	const size_t *cpu = virg_cpu_getsizes();
	int n = sizeof(virg_testsizes) / sizeof(size_t);

	for(int i = 0; i < n; i++) {
		ASSERT_EQ(local[i], cpu[i]);
		ASSERT_EQ(local[i], gpu[i]);
	}
}


} // namespace

