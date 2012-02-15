#include "virginian.h"
#include "gtest/gtest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

namespace {

class BasicStateTest : public ::testing::Test {
};

TEST_F(BasicStateTest, BasicAssertions) {
	ASSERT_EQ(VIRG_KB, 1024);
	ASSERT_EQ(VIRG_MB, 1048576);
	ASSERT_EQ(VIRG_GB, 1073741824);
	ASSERT_GT(VIRG_TABLET_SIZE, 0);
	ASSERT_GT(VIRG_TABLET_KEY_INCREMENT, 0);
	ASSERT_GT(VIRG_TABLET_INFO_SIZE, 0);
	ASSERT_GT(VIRG_TABLET_INFO_INCREMENT, 0);

	ASSERT_GT(VIRG_MEM_TABLETS, 0);
	ASSERT_GT(VIRG_GPU_TABLETS, 0);
	ASSERT_GT(VIRG_VM_TABLES, 0);
	ASSERT_GT(VIRG_CPU_SIMD, 0);

	ASSERT_GE(VIRG_CUDADEVICE, 0);
	ASSERT_LT(VIRG_CUDADEVICE, 4);

	ASSERT_GT(VIRG_THREADSPERBLOCK, 0);
	EXPECT_LE(VIRG_THREADSPERBLOCK, 540);
	ASSERT_TRUE(VIRG_ISPWR2(VIRG_THREADSPERBLOCK));
	ASSERT_EQ(-(int)VIRG_THREADSPERBLOCK, (int)VIRG_THREADSPERBLOCK_MASK);
	ASSERT_EQ((int)(VIRG_THREADSPERBLOCK & VIRG_THREADSPERBLOCK_MASK), (int)VIRG_THREADSPERBLOCK);

	ASSERT_GT(VIRG_MULTITHREADS, 0);
	EXPECT_LE(VIRG_MULTITHREADS, 64);
	ASSERT_EQ(VIRG_FAIL, 0);
	ASSERT_EQ(VIRG_SUCCESS, 1);

	ASSERT_EQ(virg_sizes[VIRG_INT], sizeof(int));
	ASSERT_EQ(virg_sizes[VIRG_FLOAT], sizeof(float));
	ASSERT_EQ(virg_sizes[VIRG_INT64], sizeof(long long int));
	ASSERT_EQ(virg_sizes[VIRG_DOUBLE], sizeof(double));
	ASSERT_EQ(virg_sizes[VIRG_CHAR], sizeof(char));
}

TEST_F(BasicStateTest, VirginianCreation) {
	virginian v;

	virg_init(&v);

	for(int i = 0; i < VIRG_MEM_TABLETS; i++)
		ASSERT_TRUE(v.tablet_slots[i] != NULL);

	ASSERT_TRUE(v.gpu_slots != NULL);

	cudaThreadExit();
}

TEST_F(BasicStateTest, DataSizes) {
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

