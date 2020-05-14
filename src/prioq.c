#include <assert.h>
#include <stdlib.h>

#include "config.h"
#include "graph.h"
#include "prioq.h"
#include "vector.h"
#include "log.h"

PriorityQueue* pq_new(void)
{
	PriorityQueue *pq = calloc(1, sizeof(*pq));
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
	PQEvent *ev = calloc(1, sizeof(*ev));
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

static void pq_heapify(PriorityQueue *pq, size_t i)
{
	/* min heap */
	if (i >= pq->vec->length) return;
	size_t lim = pq->vec->length;
	size_t left = i * 2 + 1;
	size_t right = i * 2 + 2;
	size_t smaller = i;
	if (left < lim && pq->events[smaller]->timestamp > pq->events[left]->timestamp)
		smaller = left;
	if (right < lim && pq->events[smaller]->timestamp > pq->events[right]->timestamp)
		smaller = right;
	if (smaller != i) {
		swap(&pq->events[i], &pq->events[smaller]);
		pq_heapify(pq, smaller);
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
	pq_heapify(pq, pq->vec->length - 1);
	return true;
}

static bool pq_pop_front(PriorityQueue *pq)
{
	static int i = 0;
	if (!pq->vec->length) return false;
	pq->events[0] = pq->events[pq->vec->length - 1];
	vector_pop_back(pq->vec);
	pq->events = (PQEvent **) pq->vec->p;
	pq_heapify(pq, 0);
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

static bool get_heads(double bias)
{
	assert(bias >= 0.0 && bias <= 1.0);
	int i = bias * 1000;
	if (lrand48() % 1000 < i) return true;
	return false;
}

static size_t toss_coin(size_t time, double bias)
{
	assert(bias <= 1.0);
	size_t t = 0;
	while (++t <= TIME_MAX - time) {
		if (get_heads(bias))
			break;
	}
	/* speed up things */
	time += 4;
	/* days elapsed */
	return time <= (TIME_MAX - t) ? time + t : TIME_MAX;
}

void process_trans_SIR(PriorityQueue *pq, PQEvent *ev)
{
	assert(pq);
	assert(ev);
	Node *n = ev->node;
	struct sir *s;
	if (ev->node->state != SIR_INFECTED) {
		if (ev->node->state == SIR_SUSCEPTIBLE)
			s = sir_list_del_item(ev->node, &ListS);
		else if (ev->node->state == SIR_RECOVERED)
			s = sir_list_del_item(ev->node, &ListR);
		/* add to infected list, if not in there already */
		sir_list_add_sir(s, &ListI);
		s->item->state = SIR_INFECTED;
	}
	/* for each neighbour */
	list_for_each_entry(s, n->neigh.next, struct sir, list) {
		/* add transmit event */
		Node *n = s->item;
		PQEvent *r = NULL, *t = pqevent_new(n, TRANSMIT);
		if (!t) {
			log_error("Failed to create TRANSMIT event for Node %u", n->id);
			log_oom();
			continue;
		}
		t->timestamp = toss_coin(ev->timestamp, ev->T);
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
		r->timestamp = toss_coin(t->timestamp, ev->Y);
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
	struct sir *s;
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
