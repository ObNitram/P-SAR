#define  _GNU_SOURCE
#include <assert.h>
#include <errno.h>
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
#include "../core/core.h"
#include "../core/data_transfer.h"

// perm: PROT_NONE, PROT_EXEC, PROT_READ, PROT_WRITE
static void memory_protect(size_t index, int perm) {
    int ret = mprotect(dsm + (index * PAGE_SIZE), PAGE_SIZE, perm);
    if (ret == -1) {
        int error = errno;
        if (error == EACCES) {
            fprintf(stderr, "memory_protect: EACCES\n");
        } else if (error == EINVAL) {
            fprintf(stderr, "memory_protect: EINVAL\n");
        } else if (error == ENOMEM) {
            fprintf(stderr, "memory_protect: ENOMEM\n");
        }
        perror("mprotect lock");
        exit(EXIT_FAILURE);
    }
}

void memory_lock(size_t index) {
    memory_protect(index, PROT_NONE);
}

void memory_unlock_read(size_t index) {
    memory_protect(index, PROT_READ);
}

void memory_unlock_write(size_t index) {
    memory_protect(index, PROT_READ | PROT_WRITE);
}

void memory_lock_reset(size_t index) {
    enum lock_status lock_status = get_lock_status(index);
    int prot;
    switch (lock_status) {
        case NONE:
            prot = PROT_NONE;
            break;
        case READING:
        case WRITING:
            prot = PROT_READ;
            break;
    }
    memory_protect(index, prot);
}

static void send_invalidation(size_t page_index) {
    size_t msg_size = sizeof(struct message) + sizeof(size_t);
    struct message * msg = malloc(msg_size);
    msg->message_type = INVALIDATION;
    size_t * page_id = (size_t *)(msg + 1);
    *page_id = page_index;
    pthread_mutex_lock(&umtx);
    broadcast_message(msg, msg_size);
    pthread_mutex_unlock(&umtx);
    free_message(msg);
}

static void sigsev_handler(int sig, siginfo_t * info, void * ucontext) {
    // Thanks to `info` we can know at which memory adress the SIGSEGV happened
    // That is, at within which page it happen
    // But not the size of the data to read/write.
    // Which shouldn't be a problem I think?
    // In part due to alignment
    void * page_addr = info->si_addr - ((size_t)info->si_addr % PAGE_SIZE);
    int err = ((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_ERR];
    bool curr_reading = false;
    bool curr_writing = false;
    if (err & 0x2) {
        curr_writing = true;
    } else if (err & 0x4) {
        curr_reading = true;
    }
    // } else if (err & 16) {
    //     printf("EXEC \n");
    // }

    size_t page_index = get_page_index(info->si_addr);


    int prot = PROT_EXEC | PROT_READ;

    enum lock_status lock_status = get_lock_status(page_index);
    // printf("%i sigsev_handler: page_index: %lu, action: %s, lock_status: %s\n", getpid(), page_index, (curr_writing) ? "write" : "read", (lock_status == WRITING) ? "WRITING" : (lock_status == READING) ? "READING" : "NONE");
    assert(lock_status != NONE); // You're not allowed to do that you criminal, how dare you
    if (lock_status == READING) {
        assert(curr_writing == false); // You don't have the right do to this my dude
    } else if (curr_writing && lock_status == WRITING) {
        // Do not put the write permission too soon.
        // We want to know if the user will write and only then send the invalidation
        // And if we put the write permission when the first read happen, we'll just never know if a read happen
        // It's a mystery~
        prot |= PROT_WRITE;
    }
    sync_page(page_index);
    memory_protect(page_index, prot);

    // If, for some reason (like another signal?) the handler exit without unlocking the memory
    // No problem! The read/write will try again, which will trigger SIGSEGV again
    // And the handler will be run once again... The circle of life. Beautiful.

    if (curr_writing && lock_status == WRITING) {
        set_new_owner(page_index, &me);
        send_invalidation(page_index);
    }
}

static void INVALIDATION_handler(struct message *msg) {
    size_t *page_id = (size_t *) (msg + 1);
    memory_lock(*page_id);
    set_new_owner(*page_id, &msg->sender);
}

void *init_sigsegv(void * dsm, size_t nb_page, bool is_owner) {
    assert(dsm != NULL);
    assert(nb_page != 0);

    struct sigaction sigact;
    sigact.sa_sigaction = sigsev_handler;
    sigact.sa_flags = SA_SIGINFO;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
        perror("sigaction set");
        exit(EXIT_FAILURE);
    }

    int prot;
    if (is_owner) {
        prot = PROT_READ;
    } else {
        prot = PROT_NONE;
    }

    for (size_t i = 0; i < nb_page ; i++) {
        memory_protect(i, prot);
    }
    addHandler(INVALIDATION, NULL, INVALIDATION_handler);
    return dsm;
}

void exit_sigsegv() {
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
