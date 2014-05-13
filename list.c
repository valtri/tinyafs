#include <stdlib.h>
#include <string.h>

#include "list.h"


void list_init(list_t *list) {
	memset(list, 0, sizeof(list_t));
}


void list_push(list_t *list, list_node_t *new_node) {
	new_node->next = list->first;
	list->first = new_node;
}


list_node_t *list_pop(list_t *list) {
	list_node_t *node;

	node = list->first;
	if (node)
		list->first = node->next;

	return node;
}
