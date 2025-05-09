#pragma once

#include <stddef.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "lock/lock.h"
#include "utils/logger.h"
#include "memory/memory.h"
#include "notification/notification.h"
#include "core/data_transfer.h"

static void memory_lock_status_notification(int fd)
{
	size_t index = 0;
	log_info("damn we've been called, we'll see\n");
	read(fd, &index, sizeof(size_t));
	log_info("read thy indexoo\n");
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
	log_info("write res\n");
	write(fd, &prot, sizeof(int));
	log_info("res written\n");
}
static int memory_fd1 = 0;

static void sigsegv_sync_page_notification(int fd)
{
	size_t page_index = 0;
	read(fd, &page_index, sizeof(size_t));
	sync_page(page_index);
	char eof = 0;
	write(fd, &eof, sizeof(char));
}
static int sigsegv_fd1 = 0;

static void sigsegv_lock_status_notification(int fd)
{
	int res = 0;
	size_t page_index = 0;
	bool curr_writing = 0;
	read(fd, &page_index, sizeof(size_t));
	read(fd, &curr_writing, sizeof(bool));
	enum lock_status lock_status = get_lock_status(page_index);
	// You're not allowed to do that you criminal, how dare you
	if (lock_status == NONE)
		res = -1;
	// You don't have the right do to this my dude
	if (lock_status == READING) {
		if (curr_writing != false)
			res = -1;
	} else if (curr_writing && lock_status == WRITING) {
		// Do not put the write permission too soon.
		// We want to know if the user will write and only then send the invalidation
		// And if we put the write permission when the first read happen, we'll just never know if a read happen
		// It's a mystery~
		res = PROT_WRITE;
	}
	write(fd, &res, sizeof(int));
}
static int sigsegv_fd2 = 0;

static void sigsegv_send_invalidation_notification(int fd)
{
	size_t page_index = 0;
	read(fd, &page_index, sizeof(size_t));
	send_invalidation(page_index);
}
static int sigsegv_fd3 = 0;

void create_all_chans(void)
{
	memory_fd1 = create_chan(memory_lock_status_notification);
	sigsegv_fd1 = create_chan(sigsegv_sync_page_notification);
	sigsegv_fd2 = create_chan(sigsegv_lock_status_notification);
	sigsegv_fd3 = create_chan(sigsegv_send_invalidation_notification);
}

void destroy_all_chans(void)
{
	destroy_chan(memory_lock_status_notification, memory_fd1);
	destroy_chan(sigsegv_sync_page_notification, sigsegv_fd1);
	destroy_chan(sigsegv_lock_status_notification, sigsegv_fd2);
	destroy_chan(sigsegv_send_invalidation_notification, sigsegv_fd3);
}
