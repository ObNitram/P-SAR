#include <assert.h>

#include "library.h"
#include "utils/logger.h"

#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

const char *localhost = "127.0.0.1";

const size_t page_size = 4096;

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

void merge_segments(char *tab, const size_t tab_size, const size_t segment_size)
{
	// Verify that tab_size is a multiple of segment_size
	ensure_error(
		tab_size % segment_size == 0,
		"The size of the array is not divisible by the number of segments");

	size_t num_segments = tab_size / segment_size;

	// Allocate temporary buffer to hold merged output
	char *tmp = (char *)malloc(tab_size);
	ensure_error(tmp != NULL,
		     "Memory allocation failed for temporary buffer");

	// Allocate array of indices, one per segment, all initialized to 0
	size_t *indices = (size_t *)calloc(num_segments, sizeof(size_t));
	ensure_error(indices != NULL,
		     "Memory allocation failed for indices array");

	// Merge loop: for each output position
	for (size_t out = 0; out < tab_size; ++out) {
		char min_val = 0;
		size_t min_seg = (size_t)-1;

		// Find the smallest available element among the segments
		for (size_t s = 0; s < num_segments; ++s) {
			if (indices[s] < segment_size) {
				char v = tab[s * segment_size + indices[s]];
				// If first candidate or v is smaller than current min
				if (min_seg == (size_t)-1 || v < min_val) {
					min_val = v;
					min_seg = s;
				}
			}
		}

		// Sanity check: there must be at least one element left
		ensure_error(min_seg != (size_t)-1,
			     "No more elements to merge");

		// Write the chosen value into the tmp buffer
		tmp[out] = min_val;
		// Advance the index in that segment
		indices[min_seg]++;
	}

	// Copy merged result back into the original array
	memcpy(tab, tmp, tab_size);

	// Clean up
	free(tmp);
	free(indices);
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

	while (true) {
		sleep(1);

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
	log_info("All segments are sorted. Merging segments...");
	merge_segments(tab, tab_size, tab_size / worker_count);
	log_info("All segments merged");
	unlock_write(tab, tab_size);

	lock_read(tab, tab_size);
	if (is_sorted(tab, tab_size)) {
		log_info("The array is sorted");
	} else {
		log_error("The array is not sorted");
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