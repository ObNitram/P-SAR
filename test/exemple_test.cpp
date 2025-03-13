#include <gtest/gtest.h>

extern "C" {
#include "library.h"
}


TEST(CountTest, Exemple1)
{
	EXPECT_EQ(1+1, 2);
}

TEST(CountTest, Exemple2)
{
	void *mem = Init_DSM(10, 5555);
	// EXPECT_FALSE(mem == NULL);
}
