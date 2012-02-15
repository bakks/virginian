#pragma once

#include "virginian.h"
#include "gtest/gtest.h"



class VirginianTest : public ::testing::Test {
	protected:
	
	virginian *simpledb_create();
	void simpledb_addrows(virginian *v, int numrows);
	void simpledb_clear(virginian *v);
};

