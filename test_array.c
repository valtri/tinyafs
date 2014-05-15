#include <stdio.h>

#include "array.h"


int main() {
	array_t a;
	char **p;

	array_init(&a, sizeof(char *));

	p = (char **)array_push(&a);
	*p = "aaaa";
	p = (char **)array_push(&a);
	*p = "bbbb";
	p = (char **)array_push(&a);
	*p = "cccc";
	p = (char **)array_push(&a);
	*p = "dddd";
	p = (char **)array_push(&a);

	while ((p = array_pop(&a)) != NULL) {
		printf("popped: %s\n", *p);
	}

	array_destroy(&a);

	return 0;
}
