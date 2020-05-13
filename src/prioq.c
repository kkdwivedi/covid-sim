#include <assert.h>
#include <stdlib.h>

#include "graph.h"
#include "prioq.h"
#include "vector.h"
#include "log.h"

PriorityQueue* pq_new(void)
{
	PriorityQueue *pq = calloc(1, sizeof(*pq));
	if (!pq) return NULL;
	pq->vec = vector_new(sizeof(PQEvent*));
	if (!pq->vec) return NULL;
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

bool pqevent_add(PriorityQueue *pq, PQEvent *ev)
{
	size_t i = pq->vec->length;
	vector_push_back(pq->vec, ev);
	i--;
	return true;
}

static void pq_pop_front(PriorityQueue *pq)
{
	return;
}

PQEvent* pqevent_next(PriorityQueue *pq)
{
	PQEvent *r = *pq->events;
	pq_pop_front(pq);
	return r;
}

static bool get_heads(double bias)
{
	return true;
}

static size_t toss_coin(size_t time, double bias)
{
	assert(bias <= 1.0);
	size_t t = 0;
	while (++t <= TIME_MAX - time) {
		if (get_heads(bias))
			break;
	}
	/* days elapsed */
	return time <= (TIME_MAX - t) ? time + t : TIME_MAX;
}

void process_trans_SIR(PriorityQueue *pq, PQEvent *ev)
{
	/* XXX: check that node state is IN_SET(SUSCEPTIBLE,RECOVERED) */
	assert(ev);
	Node *n = ev->node;
	/* delete from susceptible list */
	sir_list_del_item(ev->node, &ListS);
	/* delete from recovered list */
	sir_list_del_item(ev->node, &ListR);
	/* add to infected list */
	if (!sir_list_add_item(ev->node, &ListI)) log_oom();
	/* for each neighbour */
	list_for_each_entry(n, n->neigh.next, Node, neigh) {
		/* add transmit event */
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
		r = pqevent_new(n, RECOVER);
		if (!r) {
			log_error("Failed to create RECOVER event for Node %u", n->id);
			log_oom();
			goto finish;
		}
		/* can only recover after being detected as infected */
		r->timestamp = toss_coin(t->timestamp, ev->T);
		if (!pqevent_add(pq, r)) {
			log_error("Failed to add RECOVER event for Node %u", n->id);
			goto finish;
		}
	finish:
		pqevent_delete(t);
		pqevent_delete(r);
	}
}

void process_rec_SIR(PriorityQueue *pq, PQEvent *ev)
{
	/* delete from infected list */
	sir_list_del_item(ev->node, &ListI);
	/* add to recovered list */
	if (!sir_list_add_item(ev->node, &ListR)) log_oom();
}

void pqevent_delete(PQEvent *ev)
{
	free(ev);
}
