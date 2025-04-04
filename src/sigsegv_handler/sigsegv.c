#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sigsegv.h"
#include "../utils/utils.h"
#include "../core/data_transfer.h"
#include "../core/core.h"

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
    long int page_size = sysconf(_SC_PAGESIZE); 

    // Thanks to `info` we can know at which memory adress the SIGSEGV happened
    // That is, at within which page it happen
    // But not the size of the data to read/write.
    // Which shouldn't be a problem I think?
    // In part due to alignment
    void * page_addr = info->si_addr - ((size_t)info->si_addr % page_size);

    size_t page_index = get_page_index(info->si_addr);
    sync_page(page_index);

    // If an user write in a readlocked memory, it's not my problem
    // (I'll see later how to do it, if possible at all)
    unlock_memory(page_addr, page_size);
    
    // If, for some reason (like another signal?) the handler exit without unlocking the memory
    // No problem! The read/write will try again, which will trigger SIGSEGV again
    // And the handler will be run once again... The circle of life. Beautiful.
}

void * init_sigsegv(void * dsm, size_t size) {
    assert(dsm != NULL);
    assert(size != 0);

    struct sigaction sigact;
    sigact.sa_sigaction = sigsev_handler;
    sigact.sa_flags = SA_SIGINFO;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
        perror("sigaction set");
        exit(EXIT_FAILURE);
    }

    lock_memory(dsm, size);
    return dsm;
}

void exit_sigsegv(void * addr, size_t size) {
    assert(addr != NULL);
    assert(size != 0);

    int exit_status = EXIT_SUCCESS;

    struct sigaction sigact;
    sigact.sa_handler = SIG_DFL;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
        perror("sigaction unset");
        exit_status = EXIT_FAILURE;
    }

    assert(exit_status == EXIT_SUCCESS);
}
