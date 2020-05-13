#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "prioq.h"
#include "graph.h"
#include "log.h"

/* TODO:
 * Ensure that the SAMPLE_SIZE is no bigger than RAND_MAX
 * Organize graph.h
 * audit safety around use of raw list helpers
 */

char log_buf[LOG_BUF_SIZE];
size_t time;

// anchors for SIR lists
List ListS;
List ListI;
List ListR;

size_t max_conn;

__attribute__((noreturn)) void usage(void)
{
	log_error("This program takes no arguments.");
	exit(0);
}

static void dump_stats(Node *n)
{
	log_info("================================");
	log_info("Sample Size:        %u", SAMPLE_SIZE);
	log_info("Max edges:          %u", NR_EDGES);
	log_info("Nodes connected:    %u", max_conn);
	log_info("Initial spreaders:  %zu", sir_list_len(&ListI));
	log_info("================================");
	log_info("Susceptible: "); sir_list_dump(&ListS);
	log_info("Infected: "); sir_list_dump(&ListI);
	log_info("Recovered: "); sir_list_dump(&ListR);
	log_info("================================");
	for (size_t i = 0; i < SAMPLE_SIZE; i++)
		node_dump_adjacent_nodes(n + i);
}

static size_t gen_random_id(size_t b, size_t except)
{
	assert(b);
	b = b < RAND_MAX ? b : RAND_MAX;
	size_t r;
	while ((r = rand() % b) == except);
	return r;
}

int main(int argc, char *argv[])
{
	PriorityQueue *pq = NULL;
	Node *narr = NULL;
	int r;

	srand(0);
	if (argc > 1)
		usage();

	r = setvbuf(stderr, log_buf, _IOFBF, LOG_BUF_SIZE);
	if (r < 0)
		log_warn("Failed to set up log buffer: %m");

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
		sir_list_add_item(narr + i, &ListS);
	}

	//now connect nodes, randomly
	for (size_t i = 0; i < SAMPLE_SIZE; i++) {
		int c = 0;
		c = rand() % SAMPLE_SIZE;
		while (c--) {
		start:;
			/* XXX: may enter a loop */
			size_t j;
			while (!(j = gen_random_id(SAMPLE_SIZE, i)));
			node_connect(&narr[i], &narr[j-1]);
		}
	}
        //begin infection
/*
	size_t infect = gen_random_id(SAMPLE_SIZE, -1);
	while (infect--) {
		//
	}

	for (PQEvent *ev = pqevent_next(pq); ev; ev = pqevent_next(pq)) {
		if (ev->type == TRANSMIT) {
			if (UINT_IN_SET(ev->node->state, SIR_SUSCEPTIBLE, SIR_RECOVERED))
				process_trans_SIR(pq, ev);
		} else if (ev->type == RECOVERED) process_rec_SIR(pq, ev);
	}
*/
	dump_stats(narr);
finish:
/*	sir_list_del_rec(&ListS);
	sir_list_del_rec(&ListI);
	sir_list_del_rec(&ListR);
	if (narr)
		for (size_t i = 0; i < SAMPLE_SIZE; i++)
			sir_list_del_rec(&narr[i].neigh);
*/
	sir_list_add_item(NULL, NULL);
	free(narr);
	pq_delete(pq);
	return r;
}
