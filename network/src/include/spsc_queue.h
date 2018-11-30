#ifndef _SPSC_QUEUE_H
#define _SPSC_QUEUE_H
#include <arch.h>

struct spsc_node {
	struct spsc_node *next;
};

struct spsc_queue {
	struct spsc_node *head ____cacheline_aligned;
	struct spsc_node *tail ____cacheline_aligned;
};

static inline
void spsc_q_put(struct spsc_queue *q, struct spsc_node *node)
{
	struct spsc_node *old_tail;

	ACCESS_ONCE(node->next) = NULL;
	smp_wmb(); /* full wmb here not wmb_tso to flush out caller changes */
	old_tail = smp_swap(&q->tail, node);
	if (old_tail == NULL)
		ACCESS_ONCE(q->head) = node;
	else
		ACCESS_ONCE(old_tail->next) = node;
	smp_wmb();
}

static inline
struct spsc_node *spsc_q_peek(struct spsc_queue *q)
{
	struct spsc_node *head;

	smp_rmb();
	head = ACCESS_ONCE(q->head);
	return head;
}

static inline
struct spsc_node *spsc_q_get(struct spsc_queue *q)
{
	struct spsc_node *head, *next;

	smp_rmb();
	head = spsc_q_peek(q);
	if (head == NULL)
		return NULL;

	next = ACCESS_ONCE(head->next);
	if (next == NULL) {
		smp_rmb();
		if (head != ACCESS_ONCE(q->tail)) {
			while ((next = ACCESS_ONCE(head->next)) == NULL)
				smp_rmb();
		}
		else
			smp_cas(&q->tail, head, NULL);
	}

	ACCESS_ONCE(q->head) = next;
	return head;
}

#endif /* _SPSC_QUEUE_H */
