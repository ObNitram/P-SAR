#define  _GNU_SOURCE
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "sigsegv.h"
#include "../utils/utils.h"
#include "../core/data_transfer.h"

// perm: PROT_NONE, PROT_EXEC, PROT_READ, PROT_WRITE
void memory_protect(size_t index, int perm) {
    if (mprotect(dsm + (index * PAGE_SIZE), PAGE_SIZE, perm) == -1) {
        perror("mprotect lock");
        exit(EXIT_FAILURE);
    }
}


// TODO Check if we got the correct perm for the action that was attempted
static void sigsev_handler(int sig, siginfo_t * info, void * ucontext) {
    // Thanks to `info` we can know at which memory adress the SIGSEGV happened
    // That is, at within which page it happen
    // But not the size of the data to read/write.
    // Which shouldn't be a problem I think?
    // In part due to alignment
    void * page_addr = info->si_addr - ((size_t)info->si_addr % PAGE_SIZE);
    // int err = ((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_ERR];
    // if (err & 0x2) {
    //     printf("WRITE \n");
    // } else if (err & 16) {
    //     printf("EXEC \n");
    // } else {
    //     printf("READ \n");
    // }

    size_t page_index = get_page_index(info->si_addr);
    sync_page(page_index);

    // If an user write in a readlocked memory, it's not my problem
    // (I'll see later how to do it, if possible at all)
    memory_protect(page_index, PROT_EXEC | PROT_WRITE | PROT_READ);

    // If, for some reason (like another signal?) the handler exit without unlocking the memory
    // No problem! The read/write will try again, which will trigger SIGSEGV again
    // And the handler will be run once again... The circle of life. Beautiful.


    size_t msg_size = sizeof(struct message) + sizeof(size_t);
    struct message * msg = malloc(msg_size);
    msg->message_type = INVALIDATION;
    size_t * page_id = (size_t *)(msg + 1);
    *page_id = page_index;

    broadcast_message(msg, msg_size);
    free_message(msg);
}


static void INVALIDATION_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    memory_protect(*page_id, PROT_NONE);
}

void * init_sigsegv(void * dsm, size_t size, bool is_owner) {
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

    if (!is_owner) {
        size_t nb_page = size / PAGE_SIZE;
        if (size % PAGE_SIZE != 0) {
            nb_page++;
        }
        for (size_t i = 0; i < nb_page ; i++) {
            memory_protect(i, PROT_NONE);
        }
    }
    addHandler(INVALIDATION, NULL, INVALIDATION_handler);
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
