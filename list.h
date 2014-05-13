#ifndef LIST_H
#define LIST_H


typedef struct list_node_s list_node_t;
struct list_node_s {
	list_node_t *next;
};

typedef struct {
	list_node_t *first;
} list_t;


void list_init(list_t *list);
void list_push(list_t *list, list_node_t *new_node);
list_node_t *list_pop(list_t *list);

#endif
