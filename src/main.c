#include "library.h"
#include "utils/logger.h"
#include <sys/wait.h>

#include <stddef.h>  // for size_t

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

void worker_node(size_t node_id, int server_port, size_t number_of_node)
{
	log_info("Worker node %ld", node_id);

	// const size_t size = page_size * number_of_node;
	// int *tab = join_DSM("localhost", server_port, server_port + node_id);
	//
	// lock_read(tab, size);
	// for (int i = 0; i < 24; i++) {
	// 	printf("%d\n", tab[i]);
	// }
	// printf("\n");
	// unlock_read(tab, size);
	//
	// int *node_tab = tab + node_id * page_size;
	//
	// lock_write(node_tab, page_size);
	// sort(node_tab, page_size);
	// unlock_write(node_tab, page_size);
	//
	// free_DSM();
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

	size_t page_size = sysconf(_SC_PAGESIZE);
	log_debug("Page size: %ld", page_size);


	// Initialize the DSM with the size of the page multiplied by the number of nodes
	const size_t size = page_size * 100;

	// Allocate and initialize the DSM
	log_info("Initializing DSM");
	int *tab = Init_DSM(size, port);
	log_info("DSM initialized");


	log_info("Filling DSM with random values");
	lock_write(tab, size);
	for (int i = 0; i < size; i++) {
		tab[i] = rand();
	}
	unlock_write(tab, size);

	log_info("Printing initial values");
	lock_read(tab, size);
	for (int i = 0; i < 24; i++) {
		printf("%d\n", tab[i]);
	}
	printf("\n");
	unlock_read(tab, size);

	log_info("Starting worker nodes...");

	for (size_t node_id = 1; node_id < number_of_node; node_id++) {
		const int sun = fork();
		if (sun == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (sun == 0) {
			worker_node(node_id, port, number_of_node);
			return 0;
		}
	}

	for (size_t node_id = 1; node_id < number_of_node; node_id++) {
		wait(NULL);
	}


	lock_read(tab, size);
	for (int i = 0; i < 24; i++) {
		printf("%d\n", tab[i]);
	}
	printf("\n");
	unlock_read(tab, size);

	free_DSM();
}