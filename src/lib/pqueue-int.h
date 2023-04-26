#ifndef PQUEUE_INT_H
#define PQUEUE_INT_H

#include "pqueue.h"

typedef struct pqueue_node_t {
	pqueue_pri_t pri;
	int	     val;
	size_t	     pos;
} pqueue_node_t;

static int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr)
{
	return (next > curr);
}

static pqueue_pri_t get_pri(void *a)
{
	return ((pqueue_node_t *)a)->pri;
}

static void set_pri(void *a, pqueue_pri_t pri)
{
	((pqueue_node_t *)a)->pri = pri;
}

static size_t get_pos(void *a)
{
	return ((pqueue_node_t *)a)->pos;
}

static void set_pos(void *a, size_t pos)
{
	((pqueue_node_t *)a)->pos = pos;
}

static pqueue_t *pqueue_int_init(size_t size)
{
	return pqueue_init(size, cmp_pri, get_pri, set_pri, get_pos, set_pos);
}

#endif /* PQUEUE_INT_H */
