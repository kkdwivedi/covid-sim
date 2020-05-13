#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_SZ (SIZE_MAX >> 1)

struct vector {
	/* number of elements */
	size_t length;
	/* base unit in bytes */
	size_t unit;
	/* number of pools */
	size_t nr_pool;
	/* pool size in bytes */
	size_t pool;
	/* pointer to buffer */
	unsigned char *p;
};

typedef struct vector Vector;

int vector_grow(Vector *v);
int vector_shrink_to_fit(Vector *v);
int vector_insert(Vector *v, size_t pos, const void *p);
int vector_insert_many(Vector *v, size_t pos, const void *p, size_t n);
int vector_push_back(Vector *v, const void *p);
bool vector_pop_back(Vector *v);
Vector* vector_new(size_t unit);
void vector_reset(Vector *v);

#endif
