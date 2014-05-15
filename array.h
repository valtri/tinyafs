#ifndef ARRAY_H
#define ARRAY_H

typedef struct {
	size_t maxn, n, size;
	char *data;
} array_t;

void array_init(array_t *a, size_t size);
void array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_pop(array_t *a);

#endif
