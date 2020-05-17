#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

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

static inline List* list_last(List *head)
{
	assert(head);
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
	bool initial;
};

typedef struct node Node;

bool sir_list_add_item(Node *n, List *l);
void sir_list_add_sir(struct sir *s, List *l);
struct sir* sir_list_del_item(Node *n, List *l);
void sir_list_dump(List *l);
void sir_list_del_rec(List *l);
size_t sir_list_len(List *l);

Node* node_new(size_t sz);
void node_connect(Node *a, Node *b);
void node_dump_adjacent_nodes(Node *n);
void node_delete(Node *n);

#endif
