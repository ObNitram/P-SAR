#include <cstddef>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
#include "sigsegv_handler/sigsegv.h"
}

TEST(sigsegv, basic) {
    EXPECT_EQ(4, 2+2) << "simple as that";
}

TEST(sigsegv, init_null) {
    EXPECT_EXIT(init_sigsegv(NULL, 0, true), testing::KilledBySignal(6), "") << "Triggering the very first assert";
}

TEST(sigsegv, init_to_destroy) {
    std::size_t size = 5000;
	void * dsm = mmap(0, size, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // void * mem = init_sigsegv(dsm, size, false);
    // exit_sigsegv(mem, size);
	munmap(dsm, size);
}
