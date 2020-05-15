#include <assert.h>
#include <stdlib.h>

#include "config.h"
#include "graph.h"
#include "prioq.h"
#include "vector.h"
#include "log.h"

PriorityQueue* pq_new(void)
{
	PriorityQueue *pq = malloc(sizeof *pq);
	if (!pq) return NULL;
	pq->vec = vector_new(sizeof(PQEvent*));
	/* caught by cppcheck */
	if (!pq->vec) { free(pq); return NULL; }
	pq->events = (PQEvent **) pq->vec->p;
	return pq;
}

void pq_delete(PriorityQueue *pq)
{
	/* free! free! free! */
	free(pq->vec->p);
	free(pq->vec);
	free(pq);
}

PQEvent* pqevent_new(Node *node, EventType type)
{
	assert(node);
	assert(type < _EVENT_TYPE_MAX);
	PQEvent *ev = malloc(sizeof *ev);
	if (!ev) return NULL;
	ev->type = type;
	ev->node = node;
	if (type == TRANSMIT)
		ev->T = PROB_T;
	else if (type == RECOVER)
		ev->Y = PROB_Y;
	else /* not reached */
		assert(false);
	return ev;
}

static void swap(PQEvent **a, PQEvent **b)
{
	PQEvent *temp = *a;
	*a = *b;
	*b = temp;
}

static void pq_heapify(PriorityQueue *pq, size_t i, bool down)
{
	/* min heap */
	size_t lim = pq->vec->length;
	if (i >= lim) return;
	if (down) {
		size_t left = i * 2 + 1;
		size_t right = i * 2 + 2;
		size_t smaller = i;
		if (left < lim && pq->events[smaller]->timestamp > pq->events[left]->timestamp)
			smaller = left;
		if (right < lim && pq->events[smaller]->timestamp > pq->events[right]->timestamp)
			smaller = right;
		if (smaller != i) {
			swap(&pq->events[i], &pq->events[smaller]);
			pq_heapify(pq, smaller, true);
		}
	} else {
		/* if i % 2 is true then left child */
		if (i == 0) return;
		size_t parent = (i % 2) ? (i - 1)/2 : (i - 2)/2;
		if (pq->events[i]->timestamp < pq->events[parent]->timestamp) {
			swap(&pq->events[i], &pq->events[parent]);
			pq_heapify(pq, parent, false);
		}
	}
}

bool pqevent_add(PriorityQueue *pq, PQEvent *ev)
{
	assert(pq);
	assert(ev);
	int r = vector_push_back(pq->vec, &ev);
	if (r < 0) {
		log_error("Failed to grow vector.");
		return false;
	}
	/* caught by ASAN: possibly resized/reinitialized, so replace
	   with new address. This can also happen during normal growth */
	pq->events = (PQEvent **) pq->vec->p;
	pq_heapify(pq, pq->vec->length - 1, false);
	return true;
}

static bool pq_pop_front(PriorityQueue *pq)
{
	static int i = 0;
	if (!pq->vec->length) return false;
	pq->events[0] = pq->events[pq->vec->length - 1];
	vector_pop_back(pq->vec);
	pq->events = (PQEvent **) pq->vec->p;
	pq_heapify(pq, 0, true);
	i++;
	/* XXX: add vec_is_empty primitive, vector might have been reset */
	if (i > 7 && pq->vec->length) {
		vector_shrink_to_fit(pq->vec);
		pq->events = (PQEvent **) pq->vec->p;
		i = 0;
	}
	return true;
}

PQEvent* pqevent_next(PriorityQueue *pq)
{
	assert(pq);
	PQEvent *r = pq->events ? pq->events[0] : NULL;
	if (!pq_pop_front(pq)) return NULL;
	return r;
}

size_t gen_random_id(size_t b, size_t except)
{
	assert(b);
	assert(except < b || except == (size_t) -1);
	size_t r;
	if (b == 1) return 0;
	r = rand() % b;
	return r == except ? (r < 1 ? r + 1 : r - 1) : r;
}

static bool get_heads(double bias)
{
	assert(bias <= 1.0);
	int i = bias * 1000;
	if (gen_random_id(1001, i) < i) return true;
	return false;
}

size_t toss_coin(size_t ts, double bias)
{
	assert(bias <= 1.0);
	size_t t = 0;
	while (++t <= TIME_MAX - ts) {
		if (get_heads(bias))
			break;
	}
	return ts + t + 1 < TIME_MAX ? ts + t + 1 : TIME_MAX;
}

void process_trans_SIR(PriorityQueue *pq, PQEvent *ev)
{
	assert(pq);
	assert(ev);
	Node *n = ev->node;
	struct sir *s = NULL;
	/* If node is already infected, don't process this TRANSMIT
	   event for it */
	if (ev->node->state != SIR_INFECTED) {
		if (ev->node->state == SIR_SUSCEPTIBLE)
			s = sir_list_del_item(ev->node, &ListS);
		else if (ev->node->state == SIR_RECOVERED)
			s = sir_list_del_item(ev->node, &ListR);
		/* add to infected list, if not in there already */
		sir_list_add_sir(s, &ListI);
		s->item->state = SIR_INFECTED;
	} else return;
	/* for each neighbour */
	list_for_each_entry(s, n->neigh.next, struct sir, list) {
		/* add transmit event */
		Node *n = s->item;
		if (n->state == SIR_INFECTED) continue;
		PQEvent *r = NULL, *t = pqevent_new(n, TRANSMIT);
		if (!t) {
			log_error("Failed to create TRANSMIT event for Node %u", n->id);
			log_oom();
			continue;
		}
		t->timestamp = toss_coin(ev->timestamp, ev->T);
		ev->timestamp += t->timestamp - ev->timestamp;
		if (!pqevent_add(pq, t)) {
			log_error("Failed to add TRANSMIT event for Node %u", n->id);
			goto finish;
		}
		log_info("Added TRANSMIT event for Node %u with time %lu", n->id, t->timestamp);

		r = pqevent_new(n, RECOVER);
		if (!r) {
			log_error("Failed to create RECOVER event for Node %u", n->id);
			log_oom();
			goto finish;
		}
		/* can only recover after being detected as infected */
		r->timestamp = toss_coin(ev->timestamp, ev->Y) + 12;
		if (!pqevent_add(pq, r)) {
			log_error("Failed to add RECOVER event for Node %u", n->id);
			goto finish;
		}
		log_info("Added RECOVER event for Node %u with time %lu", n->id, r->timestamp);
		continue;
	finish:
		pqevent_delete(t);
		pqevent_delete(r);
	}
}

void process_rec_SIR(PriorityQueue *pq, PQEvent *ev)
{
	assert(ev);
	struct sir *s = NULL;
	if (ev->node->state != SIR_RECOVERED) {
		if (ev->node->state == SIR_SUSCEPTIBLE)
			s = sir_list_del_item(ev->node, &ListS);
		else if (ev->node->state == SIR_INFECTED)
			s = sir_list_del_item(ev->node, &ListI);
		sir_list_add_sir(s, &ListR);
		s->item->state = SIR_RECOVERED;
	}
}

void pqevent_delete(PQEvent *ev)
{
	free(ev);
}
