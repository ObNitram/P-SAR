#include <assert.h>

#include "library.h"
#include "utils/logger.h"

#include <stdio.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <time.h>
#include <string.h>

const char *localhost = "127.0.0.1";

const size_t page_size = 4096;

FILE *logfile;

void fprint_with_time(const char *format, ...)
{
	struct timespec ts;
	char time_buffer[64];

	// Get high precision current time
	clock_gettime(CLOCK_REALTIME, &ts);

	// Format time as [seconds.nanoseconds]
	snprintf(time_buffer, sizeof(time_buffer), "[%ld.%09ld] ", ts.tv_sec,
		 ts.tv_nsec);

	// Print the time to the file
	fputs(time_buffer, logfile);

	// Handle the variable argument list like printf
	va_list args;
	va_start(args, format);
	vfprintf(logfile, format, args);
	va_end(args);

	// Optional: flush immediately
	fflush(logfile);
}

// Fonction utilitaire pour fusionner deux segments triés
void merge(char *dest, char *left, size_t left_size, char *right,
	   size_t right_size)
{
	size_t i = 0, j = 0, k = 0;
	while (i < left_size && j < right_size) {
		if (left[i] <= right[j]) {
			dest[k++] = left[i++];
		} else {
			dest[k++] = right[j++];
		}
	}
	// Copie du reste
	while (i < left_size)
		dest[k++] = left[i++];
	while (j < right_size)
		dest[k++] = right[j++];
}

// Fonction principale : fusionne récursivement les segments triés
void merge_sort(char *tab, size_t tab_size, size_t segment_size)
{
	if (segment_size >= tab_size) {
		return; // rien à faire si tout est déjà un seul segment
	}

	// Allocation d’un buffer temporaire pour la fusion
	char *buffer = malloc(tab_size);
	if (!buffer) {
		// gestion simple de l'erreur d'allocation
		return;
	}

	size_t num_segments = tab_size / segment_size;

	// Double la taille du segment à chaque itération
	for (size_t current_size = segment_size; current_size < tab_size;
	     current_size *= 2) {
		for (size_t i = 0; i < tab_size; i += 2 * current_size) {
			size_t left_start = i;
			size_t right_start = i + current_size;
			size_t left_size = current_size;
			size_t right_size =
				(right_start + current_size <= tab_size) ?
					current_size :
					(tab_size - right_start);

			// Fusionne les deux sous-tableaux si le second existe
			if (right_start < tab_size) {
				merge(buffer + left_start, tab + left_start,
				      left_size, tab + right_start, right_size);
			} else {
				// Copie le reste s'il n'y a pas de paire
				memcpy(buffer + left_start, tab + left_start,
				       left_size);
			}
		}

		// Copie le buffer dans le tableau d'origine
		memcpy(tab, buffer, tab_size);
	}

	free(buffer);
}

// Function to swap two integers
static void swap(char *a, char *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

// Partition function for Quick Sort
static int partition(char *tab, int low, int high)
{
	// Choosing the last element as pivot
	int pivot = tab[high];
	int i = low - 1; // index of smaller element

	// Rearranging elements based on pivot
	for (int j = low; j < high; j++) {
		if (tab[j] <= pivot) {
			// If current element is less than or equal to pivot
			i++;
			swap(&tab[i], &tab[j]); // Swap elements
		}
	}
	swap(&tab[i + 1], &tab[high]); // Place pivot in the correct position
	return i + 1;
}

// Recursive Quick Sort function
static void quick_sort(char *tab, int low, int high)
{
	if (low < high) {
		// Partition the array and get the pivot index
		int pi = partition(tab, low, high);

		// Recursively sort elements before partition and after partition
		quick_sort(tab, low, pi - 1);
		quick_sort(tab, pi + 1, high);
	}
}

// Public function to sort an array of integers in ascending order using Quick Sort
void sort(char *tab, const size_t tab_size)
{
	if (tab_size == 0) {
		return; // If the array is empty, do nothing
	}
	quick_sort(tab, 0, tab_size - 1);
}

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

	sort(node_tab, segment_size);

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


bool is_sorted(const char *tab, const size_t tab_size)
{
	if (tab_size == 0) {
		return true; // If the array is empty, it's considered sorted
	}
	for (size_t i = 0; i < tab_size - 1; i++) {
		if (tab[i] > tab[i + 1]) {
			// log_error("Array is not sorted at index %zu: %d > %d",
			//           i, tab[i], tab[i + 1]);
			return false;
		}
	}
	return true;
}

void main_node(int server_port, const size_t worker_count,
	       const size_t tab_size)
{
	log_info("Initializing DSM");
	char *tab = Init_DSM(tab_size, localhost, server_port);
	log_info("DSM initialized");

	log_info("Filling DSM with random values");
	lock_write(tab, tab_size);
	for (char i = 0; i < tab_size; i++) {
		tab[i] = (char)rand();
	}
	unlock_write(tab, tab_size);

	while (true) {
		sleep(1);
		log_info("(0) Waiting for all nodes to finish sorting");

		bool tab_is_sorted = true;

		lock_read(tab, tab_size);

		for (int i = 0; i < worker_count; ++i) {
			size_t segment_size = tab_size / worker_count;
			char *node_tab = tab + i * segment_size;
			bool segment_is_sorted =
				is_sorted(node_tab, segment_size);
			if (!segment_is_sorted) {
				tab_is_sorted = false;
				log_info("(0) Segment %d is not sorted", i);
			}
		}

		unlock_read(tab, tab_size);

		if (tab_is_sorted) {
			break;
		}
		log_info("(0) Not all nodes finished sorting. Waiting...");
	}

	lock_write(tab, tab_size);
	const size_t segment_size = tab_size / worker_count;
	log_info("Merging sorted segments");
	merge_sort(tab, tab_size, worker_count);
	log_info("Sorted segments merged");
	unlock_write(tab, tab_size);

	lock_read(tab, tab_size);
	if(!is_sorted(tab, tab_size)) {
		log_error("Main node: Array is not sorted after merge");
	} else {
		log_info("Main node: Array is sorted after merge");
	}
	unlock_read(tab, tab_size);

	leave_DSM();
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

	tab_size = tab_size * page_size;

	char buf[256];
	snprintf(buf, sizeof(buf), "log_leaving_%zu_%zu.txt", number_of_node,
		 tab_size);

	logfile = fopen(buf, "a");

	if (!logfile) {
		perror("Failed to open log file");
		return 1;
	}

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