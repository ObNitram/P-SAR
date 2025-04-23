#include "library.h"
#include "utils/logger.h"

#include <sys/wait.h>
#include <stdbool.h>

#include <stddef.h>  // for size_t
#include <stdlib.h>


const char *localhost = "127.0.0.1";


// Function to swap two integers
static void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

// Partition function for Quick Sort
static int partition(int *tab, int low, int high)
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
static void quick_sort(int *tab, int low, int high)
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
void sort(int *tab, const size_t tab_size)
{
	if (tab_size == 0) {
		return; // If the array is empty, do nothing
	}
	quick_sort(tab, 0, tab_size - 1);
}


bool is_sorted(const int *tab, const size_t tab_size)
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
                 const size_t number_of_node,
                 const size_t tab_size)
{
	log_info("Worker node %ld", node_id);
	const size_t raw_tab_size = tab_size * sizeof(int);

	int *tab = join_DSM(localhost, server_port,
	                    localhost, server_port + node_id);

	const size_t raw_segment_size = raw_tab_size / number_of_node;
	ensure_warning(raw_tab_size % number_of_node == 0,
	               "The size of the array is not divisible by the number of nodes");
	const size_t segment_size = tab_size / number_of_node;
	ensure_warning(tab_size % number_of_node == 0,
	               "The size of the array is not divisible by the number of nodes");

	const size_t tab_offset = (node_id - 1) * segment_size;
	int *node_tab = tab + tab_offset;
	log_info("(%ld) Sorting segment %lu to %lu, total size is %lu",
	         node_id, tab_offset, tab_offset + segment_size, tab_size);

	lock_write(node_tab, raw_segment_size);
	sort(node_tab, segment_size);
	unlock_write(node_tab, raw_segment_size);

	leave_DSM();
}

void main_node(int server_port, size_t tab_size)
{
	const size_t row_tab_size = tab_size * sizeof(int);

	// Allocate and initialize the DSM
	log_info("Initializing DSM");
	int *tab = Init_DSM(row_tab_size, localhost, server_port);
	log_info("DSM initialized");

	log_info("Filling DSM with random values");
	lock_write(tab, row_tab_size);
	for (int i = 0; i < tab_size; i++) {
		tab[i] = rand();
	}
	unlock_write(tab, row_tab_size);

	log_info("Printing initial values");
	lock_read(tab, row_tab_size);
	for (int i = 0; i < 24; i++) {
		printf("%d\n", tab[i]);
	}
	printf("\n");
	unlock_read(tab, row_tab_size);

	while (true) {
		sleep(1);

		lock_read(tab, row_tab_size);
		bool is_sorted_ = is_sorted(tab, tab_size);
		unlock_read(tab, row_tab_size);

		if (is_sorted_) {
			break;
		}
	}

	log_info("The array is sorted");

	leave_DSM();
}


int main(int argc, char **argv)
{
	init_logger(stderr);

	if (ensure_error(argc == 3, "Usage: %s <port> <number_of_nodes>",
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

	if (ensure_error(number_of_node<= 10,
	                 "Number of nodes must be less than or equal to 10")) {
		return 1;
	}
	if (ensure_error(number_of_node > 0,
	                 "Number of nodes must be greater than 0")) {
		return 1;
	}

	const size_t tab_size = number_of_node * 1000;

	int sun = fork();
	if (sun == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (sun == 0) {
		main_node(port, tab_size);
		return 0;
	}

	sleep(1);

	log_info("Starting worker nodes...");

	for (size_t node_id = 1; node_id <= number_of_node; node_id++) {
		sun = fork();
		if (sun == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (sun == 0) {
			worker_node(node_id, port, number_of_node, tab_size);
			return 0;
		}
	}

	for (size_t node_id = 0; node_id < number_of_node; node_id++) {
		wait(NULL);
	}
}