#define _GNU_SOURCE
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
#include "../memory/memory.h"
// #define DISABLE_LOG
#include "../utils/logger.h"

static int sync_page_chan = 0;
static int lock_status_chan = 0;
static int send_invalidation_chan = 0;

static bool is_valid_address(void *addr)
{
	return addr >= dsm && addr < dsm + PAGE_SIZE * nb_pages;
}

static void sigsev_handler(int sig, siginfo_t *info, void *ucontext)
{
	int err = ((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_ERR];
	bool curr_reading = false;
	bool curr_writing = false;
	if (err & 0x2) {
		curr_writing = true;
	} else if (err & 0x4) {
		curr_reading = true;
	}

	if (!is_valid_address(info->si_addr)) {
		log_error(
			"SIGSEGV triggered on invalid adress %p, watch your program\n");
		char *reading = (curr_reading) ? "true" : "false";
		char *writing = (curr_writing) ? "true" : "false";
		printf("Node %s:%d accessed adresse %p with access read = %s and write = %s\n",
		       me.host, me.port, info->si_addr, reading, writing);
		exit(0);
	}

	size_t page_index = get_page_index(info->si_addr);

	int prot = PROT_EXEC | PROT_READ;

	write(lock_status_chan, &page_index, sizeof(size_t));
	write(lock_status_chan, &curr_writing, sizeof(bool));
	int prot_write = 0;
	read(lock_status_chan, &prot_write, sizeof(int));
	assert(prot_write != -1);
	prot |= prot_write;

	char eof = 0;
	log_info("write for sync page\n");
	write(sync_page_chan, &page_index, sizeof(size_t));
	log_info("read res\n");
	read(sync_page_chan, &eof, sizeof(char));
	//sync_page(page_index);

	memory_protect(page_index, prot);

	// If, for some reason (like another signal?) the handler exit without unlocking the memory
	// No problem! The read/write will try again, which will trigger SIGSEGV again
	// And the handler will be run once again... The circle of life. Beautiful.

	if (curr_writing && prot_write) {
		write(send_invalidation_chan, &page_index, sizeof(size_t));
	}
	log_info("FINISHED HIM\n");
}

void *init_sigsegv(void *dsm, size_t nb_page, bool is_owner, int chans[3])
{
	assert(dsm != NULL);
	assert(nb_page != 0);
	sync_page_chan = chans[0];
	lock_status_chan = chans[1];
	send_invalidation_chan = chans[2];

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

	for (size_t i = 0; i < nb_page; i++) {
		memory_protect(i, prot);
	}
	return dsm;
}

void exit_sigsegv()
{
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
