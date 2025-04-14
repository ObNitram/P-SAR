#include <cstddef>
#include <cstdio>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "utils/utils.h"
#include "sigsegv_handler/sigsegv.h"
}

TEST(sigsegv, init_null) {
    EXPECT_EXIT(init_sigsegv(NULL, 0, true), testing::KilledBySignal(6), "") << "Triggering the very first assert";
}

TEST(sigsegv, init_zero) {
    EXPECT_EXIT(init_sigsegv(NULL, 0, true), testing::KilledBySignal(6), "") << "Triggering the second assert";
}

void * init(std::size_t size, bool is_owner) {
    dsm = mmap(0, size, 
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    init_sigsegv(dsm, size, is_owner);
    return dsm;
}


TEST(sigsegv, init_to_destroy_owner) {
    size_t size = PAGE_SIZE;
    void * dsm = init(size,  true);
    exit_sigsegv();
	munmap(dsm, size);
}
TEST(sigsegv, init_to_destroy_slave) {
    size_t size = PAGE_SIZE;
    void * dsm = init(size,  false);
    init_sigsegv(dsm, 1, false);
    exit_sigsegv();
	munmap(dsm, size);
}
