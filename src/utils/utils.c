#include "utils.h"

int node_equal(struct node_id *node1, struct node_id *node2){
	return node1->port == node2->port && strcmp(node1->host, node2->host) == 0;
}