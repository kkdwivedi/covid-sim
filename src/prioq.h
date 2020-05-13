#ifndef PRIOQ_H
#define PRIOQ_H

#include <stddef.h>
#include "graph.h"
#include "vector.h"

extern size_t time;
extern List ListS;
extern List ListI;
extern List ListR;

#define TIME_MAX 300U
#define PROB_T 0.5
#define PROB_Y 0.2

enum eventtype {
	TRANSMIT = 1,
	RECOVER,
	_EVENT_TYPE_MAX,
};

typedef enum eventtype EventType;

static inline int comparator(unsigned int a, unsigned int b)
{
	return a < b ? -1 : (a == b ? 0 : 1);
}

typedef struct priorityqueue PriorityQueue;
typedef struct pqevent PQEvent;

struct pqevent {
	/* virtual timestamp of event */
	unsigned long timestamp;
	EventType type;
	/* node owning the event */
	Node *node;
	union {
		double T;
		double Y;
	};
};

struct priorityqueue {
	Vector *vec;
        PQEvent **events;
};

PriorityQueue* pq_new(void);
void pq_delete(PriorityQueue *pq);

PQEvent* pqevent_new(Node *node, EventType type);
bool pqevent_add(PriorityQueue *pq, PQEvent *ev);
PQEvent* pqevent_next(PriorityQueue *pq);
void process_trans_SIR(PriorityQueue *pq, PQEvent *ev);
void process_rec_SIR(PriorityQueue *pq, PQEvent *ev);
void pqevent_delete(PQEvent *ev);

#endif
