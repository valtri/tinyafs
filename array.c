#include <stdlib.h>
#include <string.h>

#include "array.h"


void array_init(array_t *a, size_t size) {
	memset(a, 0, sizeof(array_t));
	a->size = size;
}


void array_destroy(array_t *a) {
	free(a->data);
	array_init(a, a->size);
}


static void array_enlarge(array_t *a) {
	void *tmp;

	if (a->n >= a->maxn) {
		if (a->maxn) a->maxn += (a->maxn >> 1);
		else a->maxn = 512;
		tmp = realloc(a->data, a->maxn * a->size);
		a->data = tmp;
	}
}


void *array_push(array_t *a) {
	array_enlarge(a);
	return a->data + a->n++ * a->size;
}


void *array_pop(array_t *a) {
	if (a->n) {
		a->n--;
		if (a->n) return a->data + (a->n - 1) * a->size;
		else return NULL;
	} else {
		return NULL;
	}
}
