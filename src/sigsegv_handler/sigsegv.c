#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sigsegv.h"

// The global variable are only used in the signal handler
// to see if the SIGSEGV happened somewhere within our juridiction
static size_t memsize;
static void * mem;

void lock_memory(void * addr, size_t size) {
    assert(addr != NULL);
    assert(size != 0);
    if (mprotect(addr, size, PROT_NONE) == -1) {
        perror("mprotect lock");
        exit(EXIT_FAILURE);
    }
}

void unlock_memory(void * addr, size_t size) {
    assert(addr != NULL);
    assert(size != 0);
    if (mprotect(addr, size, PROT_WRITE) == -1) {
        perror("mprotect unlock");
        exit(EXIT_FAILURE);
    }
}

static void sigsev_handler(int sig, siginfo_t *info, void *ucontext) {
    // Check if the SIGSEGV is happening within our juridiction
    assert((info->si_addr >= mem) && (info->si_addr < (mem + memsize)));

    long int page_size = sysconf(_SC_PAGESIZE); 

    // Thanks to `info` we can know at which memory adress the SIGSEGV happened
    // That is, at within which page it happen
    // But not the size of the data to read/write.
    // Which shouldn't be a problem I think?
    // In part due to alignment
    void * page_addr = info->si_addr - ((size_t)info->si_addr % page_size);

    // somehow get the page.
    // ...
    // We got the page!

    // If an user write in a readlocked memory, it's not my problem
    // (I'll see later how to do it, if possible at all)
    unlock_memory(page_addr, page_size);
    
    // If, for some reason (like another signal?) the handler exit without unlocking the memory
    // No problem! The read/write will try again, which will trigger SIGSEGV again
    // And the handler will be run once again... The circle of life. Beautiful.
}

void * init_sigsegv(size_t size) {
    assert(size != 0);
    memsize = size;
    long int page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }

    size_t nb_page = size / page_size;
    if (size % page_size != 0) {
        nb_page++;
    }

    struct sigaction sigact;
    sigact.sa_sigaction = sigsev_handler;
    sigact.sa_flags = SA_SIGINFO;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
        perror("sigaction set");
        exit(EXIT_FAILURE);
    }

    // POSIX require that mprotect should be applied to memory obtained through mmap
    // Linux doesn't care tho lmao (as long it's not memory used by the kernel obviously)
    // Small problem tho, valgrind can't track mmap'd memory
    // Why PROT_EXEC? To give the maximum amount of freedom to the memory.
    // It will be locked down by mprotect later anyway.
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    lock_memory(mem, nb_page * page_size);
    return mem;
}

void exit_sigsegv(void * addr, size_t size) {
    assert(addr != NULL);
    assert(size != 0);
    // Dev only assert, check that we're removing the right data
    assert(mem == addr);
    assert(memsize == size);

    int exit_status = EXIT_SUCCESS;

    struct sigaction sigact;
    sigact.sa_handler = SIG_DFL;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
        perror("sigaction unset");
        exit_status = EXIT_FAILURE;
    }

    // no need to 'munprotect' or something like that with munmap
    if (munmap(mem, size) == -1) {
        perror("munmap");
        exit_status = EXIT_FAILURE;
    }


    assert(exit_status == EXIT_SUCCESS);
}
