#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "prioq.h"
#include "graph.h"
#include "log.h"

/* TODO:
 * Ensure that the SAMPLE_SIZE is no bigger than RAND_MAX
 * Find some comprehensive fix to UAFs from vector_reset
 */

char log_buf[LOG_BUF_SIZE];

// anchors for SIR lists
List ListS;
List ListI;
List ListR;

size_t time;
size_t max_conn;

#define DUMP_NUM  0x00000001
#define DUMP_NODE 0x00000002
#define DUMP_SIR  0x00000004

__attribute__((noreturn)) void usage(void)
{
	log_error("This program takes no arguments.");
	exit(0);
}

static void dump_stats(Node *n, unsigned mask)
{
	if (mask & DUMP_NUM) {
		log_info("Sample Size:        %u", SAMPLE_SIZE);
		log_info("Max edges:          %u", NR_EDGES);
		log_info("Connections made:   %zu", max_conn);
		log_info("Initial spreaders:  %zu", sir_list_len(&ListI));
	}
	if (mask & DUMP_SIR) {
		log_info("Susceptible: "); sir_list_dump(&ListS);
		log_info("Infected: "); sir_list_dump(&ListI);
		log_info("Recovered: "); sir_list_dump(&ListR);
	}
	if (mask & DUMP_NODE) {
		assert(n);
		for (size_t i = 0; i < SAMPLE_SIZE; i++)
			node_dump_adjacent_nodes(n + i);
	}
	log_info("================================");
}

static size_t gen_random_id(size_t b, size_t except)
{
	assert(b);
	// b = b < RAND_MAX + 1ULL ? b : RAND_MAX + 1ULL;
	assert(except < b);
	size_t r;
	if (b == 1) return 0;
	while ((r = lrand48() % b) == except);
	return r;
}

int main(int argc, char *argv[])
{
	PriorityQueue *pq = NULL;
	Node *narr = NULL;
	int r;

	srand48(0xfaceb00c);
	if (argc > 1)
		usage();

	r = setvbuf(stderr, log_buf, _IOFBF, LOG_BUF_SIZE);
	if (r < 0)
		log_warn("Failed to set up log buffer.");

	pq = pq_new();
	if (!pq) {
		log_error("Failed to setup priority queue, fatal.");
		log_oom();
		goto finish;
	}

	narr = node_new(SAMPLE_SIZE);
	if (!narr) {
		log_error("Failed to allocate nodes, fatal.");
		log_oom();
		goto finish;
	}

	for (size_t i = 0; i < SAMPLE_SIZE; i++) {
		if (!sir_list_add_item(narr + i, &ListS)) {
			log_error("Failed to add nodes to Susceptible list, fatal.");
			log_oom();
			goto finish;
		}
	}

	log_info("Initial lists: ");
	dump_stats(NULL, DUMP_SIR);

	// now connect nodes, randomly
	for (size_t i = lrand48() % SAMPLE_SIZE; i < SAMPLE_SIZE; i += 2) {
		int c = 0;
		c = lrand48() % NR_EDGES/2;
		while (c--) {
			size_t j = gen_random_id(SAMPLE_SIZE, i);
			node_connect(&narr[i], &narr[j]);
		}
	}
	log_info("Node connections: ");
	dump_stats(narr, DUMP_NODE);

	size_t infect = gen_random_id(SAMPLE_SIZE, 0);
	while (infect--) {
		size_t r = gen_random_id(SAMPLE_SIZE, 0);
		/*
		struct sir *s = sir_list_del_item(narr + r, &ListS);
		if (!s) {
			log_warn("Node %u not found in Susceptible list.", narr[r].id);
			continue;
		}
		else sir_list_add_sir(s, &ListI);
		*/
		PQEvent *ev = pqevent_new(narr + r, TRANSMIT);
		if (!ev) {
			log_warn("Failed to allocate TRANSMIT event for spreader %u.", narr[r].id);
			log_oom();
			continue;
		}
		ev->timestamp = time++;
		if (!pqevent_add(pq, ev)) {
			log_warn("Failed to add TRANSMIT event for spreader %u", narr[r].id);
			pqevent_delete(ev);
			continue;
		}
	}
	log_info("Lists after infection: ");
	dump_stats(NULL, DUMP_SIR);

	// begin simulation
	PQEvent *ev;
	for (ev = pqevent_next(pq); ev && ev->timestamp < TIME_MAX; ev = pqevent_next(pq)) {
		if (ev->type == TRANSMIT) {
			log_info("Processing event TRANSMIT at time %lu for Node %u", ev->timestamp, ev->node->id);
			if (UINT_IN_SET(ev->node->state, SIR_SUSCEPTIBLE, SIR_RECOVERED))
				process_trans_SIR(pq, ev);
		} else if (ev->type == RECOVER) {
			log_info("Processing event RECOVER at time %lu for Node %u", ev->timestamp, ev->node->id);
			process_rec_SIR(pq, ev);
		} else assert(false);
		pqevent_delete(ev);
	}
	if (ev) {
		/* min-heap was not empty */
		/* caught by LSAN: this is a min heap, so a zero
		initialized PQEvent is not really a good end marker,
		since it would swap its way up to the top, and cut
		short our loop below, leaking memory, hence make the
		timestamp infinite */
		pqevent_add(pq, &(PQEvent){ .timestamp = -1 });
		do {
			if (ev->type == RECOVER)
				process_rec_SIR(pq, ev);
			/* delete events of either type */
			pqevent_delete(ev);
		} while (ev = pqevent_next(pq), ev && ev->node);
	}
	dump_stats(narr, DUMP_SIR|DUMP_NUM|DUMP_NODE);
finish:
// Not useful with pool based SIR nodes
/*	sir_list_del_rec(&ListS);
	sir_list_del_rec(&ListI);
	sir_list_del_rec(&ListR);
	if (narr)
		for (size_t i = 0; i < SAMPLE_SIZE; i++)
			sir_list_del_rec(&narr[i].neigh);
*/
	log_info("Destructing objects...");
	// resets pool
	sir_list_add_item(NULL, NULL);
	free(narr);
	pq_delete(pq);
	return r;
}
