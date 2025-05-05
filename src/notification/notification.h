#pragma once

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "comm/comm.h"
#include "utils/logger.h"

int create_chan(void (*cb)(int))
{
	// create struct
	int fds[2] = { 0 };
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) != 0) {
		log_error("fail create pipe: %s", strerror(errno));
		return -1;
	}
	// add handler
	add_handler(cb, fds[0]);

	// return fd read
	return fds[1];
}

int destroy_chan(void (*cb)(int), int fd)
{
	//delete handler
	int ret = 0;
	if (delete_handlers(cb) != 0) {
		log_error("cant delete handler");
		ret = -1;
	}

	//close fd
	if (close(fd) != 0) {
		log_error("fail close pipe: %s", strerror(errno));
		ret = -1;
	}
	return ret;
}

