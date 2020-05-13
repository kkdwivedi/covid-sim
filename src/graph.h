#ifndef GRAPH_H
#define GRAPH_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "log.h"
#include "config.h"

static bool ptr_in(void *n, void **a)
{
	for (; *a; a++) if (*a == n) return true;
	return false;
}

static bool uint_in(size_t n, size_t *a)
{
	for (; *a != (size_t) -1; a++)
		if (*a == n) return true;
	return false;
}

#define container_of(ptr, type, member) ((type *) ((char *) (ptr) - offsetof(type, member)))
/* conversion is implicit, since the pointer is in initializer
 * list, it's akin to doing arr[0] = ptr1; arr[1] = ptr2; ...
 * so this should be safe and generic for all pointer types
 */
#define PTR_IN_SET(ptr, ...) (ptr_in((ptr), (void **) &(void *[]) { __VA_ARGS__, NULL }))
/* don't pass SIZE_MAX here */
#define UINT_IN_SET(x, ...) (uint_in((x), (size_t *) &(size_t []) { __VA_ARGS__, -1 }))

struct list {
	struct list *next;
};

typedef struct list List;

extern List ListS;
extern List ListI;
extern List ListR;

static inline List* list_last(List *head)
{
	if (head)
		while (head->next)
			head = head->next;
	return head;
}

static inline void list_append(List *head, List *what)
{
	List *t = list_last(head);
	t->next = what;
}

static inline void list_delete(List **l, List *what)
{
	while (*l && *l != what)
		l = &(*l)->next;
	if (!*l)
		return;
	/* l points to next field we want to update */
	*l = what->next;
}

/* iterator is of the type (List *) */
#define list_for_each(i, list) for (i = (list); i; i = (list)->next)
/* iterator is of the type (struct *) */
#define list_for_each_entry(i, ptr, type, member)			\
	for (i = (ptr) ? container_of((ptr), type, member) : NULL;	\
	     i; i = i->member.next ? container_of(i->member.next, type, member) : NULL)

extern size_t max_conn;

enum status {
	SIR_SUSCEPTIBLE = 1,
	SIR_INFECTED,
	SIR_RECOVERED,
	_SIR_TYPE_MAX,
};

typedef enum status Status;

struct sir {
	struct node *item;
	struct list list;
};

struct node {
	/* node id */
	unsigned int id;
	/* node state */
	Status state;
	/* node adjacency list anchor */
	List neigh;
	List *last;
};

typedef struct node Node;

static unsigned int id_gen;

static bool sir_list_add_item(Node *n, List *l)
{
	static size_t iterator = 0;
	static struct sir *pool = NULL;
	if (!l && !n) {
		free(pool);
		pool = NULL;
		iterator = 0;
		return true;
	}
	if (!pool) {
		pool = calloc(SAMPLE_SIZE*2, sizeof(*pool));
		if (!pool) return false;
	}
	struct sir *s = NULL;
	if (iterator < SAMPLE_SIZE*2)
		s = pool + iterator++;
	if (!s) return false;
	list_append(l, &s->list);
	s->item = n;
	return true;
}

static void sir_list_del_item(Node *n, List *l)
{
	/* l is assumed to be anchor, not an object embedded in entry */
	struct sir *i;
	list_for_each_entry(i, l->next, struct sir, list) {
		if (i->item == n) {
			struct sir *f = container_of(&i->list, struct sir, list);
			/* updates l->next */
			list_delete(&l->next, &i->list);
			free(f);
			break;
		}
	}
}

static void sir_list_dump(List *l)
{
	struct sir *i;
	list_for_each_entry(i, l->next, struct sir, list)
		fprintf(stderr, "%u ", i->item->id);
	log_info("");
}

static void sir_list_del_rec(List *l)
{
	if (!l->next) return;
	struct sir *i;
	/* we cannot use list_for_each_entry here, since we need to
	 * advance the iterator and only then free the memory
	 */
	for (i = container_of(l->next, struct sir, list); i;) {
		struct sir *f = i;
		i = i->list.next ? container_of(i->list.next, struct sir, list) : NULL;
		//sir_list_dump(&f->list, true);
		free(f);
	}
	l->next = NULL;
}

static size_t sir_list_len(List *l)
{
	/* begin counting from next, as l is anchor */
	size_t count = 0;
	while (l->next) {
		l = l->next;
		count++;
	}
	return count;
}

static inline Node* node_new(size_t sz)
{
	if (id_gen == UINT_MAX) return NULL;
	Node *n = calloc(sz, sizeof(*n));
	if (!n) return NULL;
	for (size_t i = 0; i < sz; i++) {
		n[i].id = ++id_gen;
		n[i].state = SIR_SUSCEPTIBLE;
	}
	return n;
}

static inline void node_connect(Node *a, Node *b)
{
	assert(a);
	assert(b);
	struct sir *i;
	if (a == b)
		return;
	list_for_each_entry(i, a->neigh.next, struct sir, list)
		if (i->item == b) return;
/*	list_for_each_entry(i, b->neigh.next, struct sir, list)
		if (i->item == a) return;
*/
	sir_list_add_item(a, &b->neigh);
	sir_list_add_item(b, &a->neigh);
	max_conn++;
}

static inline void node_dump_adjacent_nodes(Node *n)
{
	fprintf(stderr, "Node %u: ", n->id);
	sir_list_dump(&n->neigh);
}

static inline void node_delete(Node *n)
{
	sir_list_del_rec(&n->neigh);
}

#endif
