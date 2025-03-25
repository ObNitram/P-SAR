#include <cstddef>
#include <gtest/gtest.h>

extern "C" {
#include "sigsegv_handler/sigsegv.h"
}

TEST(sigsegv, basic) {
    EXPECT_EQ(4, 2+2) << "simple as that";
}

TEST(sigsegv, init_zero) {
    EXPECT_EXIT(init_sigsegv(0), testing::KilledBySignal(6), "") << "Triggering the very first assert";
}

TEST(sigsegv, init_to_destroy) {
    std::size_t size = 5000;
    void * mem = init_sigsegv(size);
    exit_sigsegv(mem, size);
}
