#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "graph.h"

bool sir_list_add_item(Node *n, List *l)
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
		/* switch to NR_EDGES */
		pool = malloc(SAMPLE_SIZE*2 * sizeof(*pool));
		if (!pool) return false;
	}
	struct sir *s = NULL;
	if (iterator < SAMPLE_SIZE*2)
		s = pool + iterator++;
	if (!s) return false;
	s->list.next = NULL;
	list_append(l, &s->list);
	s->item = n;
	return true;
}

void sir_list_add_sir(struct sir *s, List *l)
{
	assert(s);
	list_append(l, &s->list);
}

struct sir* sir_list_del_item(Node *n, List *l)
{
	/* l is assumed to be anchor, not an object embedded in entry */
	struct sir *i;
	list_for_each_entry(i, l->next, struct sir, list) {
		if (i->item == n) {
			struct sir *f = container_of(&i->list, struct sir, list);
			/* updates l->next */
			list_delete(&l->next, &i->list);
			/* freeing individual elements is not possible
			   in pool based implementation */
			// free(f);
			f->list.next = NULL;
			return f;
		}
	}
	return NULL;
}

void sir_list_dump(List *l)
{
	struct sir *i;
	list_for_each_entry(i, l->next, struct sir, list)
		fprintf(stderr, "%u ", i->item->id);
	log_info("");
}

void sir_list_del_rec(List *l)
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

size_t sir_list_len(List *l)
{
	/* begin counting from next, as l is anchor */
	size_t count = 0;
	while (l->next) {
		l = l->next;
		count++;
	}
	return count;
}

Node* node_new(size_t sz)
{
	Node *n = malloc(sz * sizeof(*n));
	if (!n) return NULL;
	for (size_t i = 0; i < sz; i++) {
		n[i].id = i + 1;
		n[i].state = SIR_SUSCEPTIBLE;
		n[i].neigh.next = NULL;
		n[i].last = NULL;
		n[i].initial = false;
	}
	return n;
}

void node_connect(Node *a, Node *b)
{
	assert(a);
	assert(b);
	struct sir *i;
	if (a == b) return;
	list_for_each_entry(i, a->neigh.next, struct sir, list)
		if (i->item == b) return;
	sir_list_add_item(a, &b->neigh);
	sir_list_add_item(b, &a->neigh);
	max_conn++;
}

void node_dump_adjacent_nodes(Node *n)
{
	fprintf(stderr, "Node %u: ", n->id);
	sir_list_dump(&n->neigh);
}

void node_delete(Node *n)
{
	sir_list_del_rec(&n->neigh);
}
