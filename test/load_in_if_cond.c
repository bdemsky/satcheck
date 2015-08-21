#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "libinterface.h"

struct node {
	struct node * next;
};


void foo()
{
	int i;
	int x = 0;

	struct node * n = malloc(sizeof(struct node));
	n->next = 0;

	i = load_32(&x);

	if ((i = load_32(&x))) {
		printf("%d\n", i);
	}

	while (load_32(&n->next)) {
		;
	}
}
