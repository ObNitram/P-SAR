#include <assert.h>

#include "library.h"
#include "utils/logger.h"

#include <sys/wait.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

const char *localhost = "127.0.0.1";

const size_t page_size = 4096;

void worker_node(const size_t node_id, const int server_port,
		 const size_t worker_count, const size_t tab_size)
{
	log_info("(%ld) Worker node Start", node_id);

	char *tab = join_DSM(localhost, server_port, localhost,
			     server_port + node_id);

	const size_t segment_size = tab_size / worker_count;

	const size_t tab_offset = (node_id - 1) * segment_size;

	char *node_tab = tab + tab_offset;

	lock_write(node_tab, segment_size);
	log_info("(%ld) Sorting segment %lu to %lu, total size is %lu)",
		 node_id, tab_offset, tab_offset + segment_size, tab_size);
	
	sleep(5);

	log_info("(%ld) Sorted segment %lu to %lu, total size is %lu)", node_id,
		 tab_offset, tab_offset + segment_size, tab_size);

	unlock_write(node_tab, segment_size);

	leave_DSM();
	log_info("(%ld) Worker node End", node_id);
}

void print_tab(const int *tab, const size_t tab_size)
{
	for (size_t i = 0; i < tab_size; i++) {
		printf("%d\n", tab[i]);
	}
	printf("\n");
}

void main_node(int server_port, const size_t worker_count,
	       const size_t tab_size)
{
	log_info("Initializing DSM");
	char *tab = Init_DSM(tab_size, localhost, server_port);
	log_info("DSM initialized");

	// log_info("Filling DSM with random values");
	lock_write(tab, tab_size);
	for (char i = 0; i < tab_size; i++) {
		tab[i] = rand();
	}
	unlock_write(tab, tab_size);

	sleep(4);

	leave_last();
	//leave_DSM();
	log_info("(0) Main node End");
}

int main(int argc, char **argv)
{
	init_logger(stderr);

	if (ensure_error(argc == 4,
			 "Usage: %s <port> <number_of_nodes> <tab_size>",
			 argv[0])) {
		return 1;
	}

	const int port = atoi(argv[1]);

	if (ensure_error(port > 0, "Port must be greater than 0")) {
		return 1;
	}

	if (ensure_error(port < 65536, "Port must be less than 65536")) {
		return 1;
	}

	const size_t number_of_node = atoi(argv[2]);

	if (ensure_error(number_of_node <= 100,
			 "Number of nodes must be less than or equal to 100")) {
		return 1;
	}
	if (ensure_error(number_of_node > 0,
			 "Number of nodes must be greater than 0")) {
		return 1;
	}

	// const size_t tab_size = number_of_node * 1000;
	size_t tab_size = atoi(argv[3]);

	log_info(
		"Start Program with param: port=%d, number_of_node=%zu, tab_size=%zu",
		port, number_of_node, tab_size);

	tab_size = tab_size * page_size * number_of_node;

	int son = fork();
	if (son == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (son == 0) {
		main_node(port, number_of_node, tab_size);
		return 0;
	}

	sleep(1);

	log_info("Starting worker nodes...");

	for (size_t node_id = 1; node_id <= number_of_node; node_id++) {
		son = fork();
		if (son == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (son == 0) {
			worker_node(node_id, port, number_of_node, tab_size);
			return 0;
		}
	}

	for (size_t node_id = 0; node_id <= number_of_node; node_id++) {
		wait(NULL);
	}
}