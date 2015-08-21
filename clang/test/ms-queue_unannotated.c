#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#define true 1
#define false 0
#define bool int

#include "ms-queue.h"
#include "libinterface.h"

void init_queue(queue_t *q, int num_threads);
void enqueue(queue_t *q, unsigned int val);
bool dequeue(queue_t *q, unsigned int *retVal);

void init_queue(queue_t *q, int num_threads) {
	int i, j;
	struct node *newnode=malloc(sizeof (struct node));
	store_32(&newnode->value, 0);

	store_64(&newnode->next, (uintptr_t) MAKE_POINTER(NULL, 0LL));
	/* initialize queue */
	store_64(&q->head, (uintptr_t) MAKE_POINTER(newnode, 0LL));

	store_64(&q->tail, (uintptr_t) MAKE_POINTER(newnode, 0LL));
}

void enqueue(queue_t *q, unsigned int val) {
	struct node *tail;
	struct node * node_ptr = malloc(sizeof(struct node));
	store_32(&node_ptr->value, val);
	
	store_64(&node_ptr->next, (uintptr_t) MAKE_POINTER(NULL, GET_COUNT(1)));
		
	while (true) {
		tail = (struct node *) load_64(&q->tail);
		struct node ** tail_ptr_next= & GET_PTR(tail)->next;

		struct node * next = (struct node *) load_64(tail_ptr_next);
		
		struct node * qtail = (struct node *) load_64(&q->tail);
		int tqt = (tail == qtail);
		if (tqt) {
			struct node *next_ptr=GET_PTR(next);
			
			int npn = (next_ptr == NULL);
			if (npn) {
				struct node * new_node = MAKE_POINTER(node_ptr, GET_COUNT(next) +1);
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) new_node);
				int vn = (valread == next);
				if (vn) {
					break;
				} else {
					MC2_yield();
				}
			} else {
				MC2_yield();
			}
			struct node * new_tailptr = GET_PTR(load_64( tail_ptr_next));
			struct node * newtail = MAKE_POINTER(new_tailptr, GET_COUNT(tail) + 1);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) newtail);
			MC2_yield();
		} else {
			MC2_yield();
		}
	}
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t)MAKE_POINTER(node_ptr, GET_COUNT(tail) + 1));
}

bool dequeue(queue_t *q, unsigned int *retVal) {
	while (true) {
		struct node * head = (struct node *) load_64(&q->head);
		struct node * tail = (struct node *) load_64(&q->tail);
		struct node * next = (struct node *) load_64(&(GET_PTR(head)->next));
		
		int t = ((struct node *) load_64(&q->head)) == head;
		if (t) {
			if (GET_PTR(head) == GET_PTR(tail)) {

				if (GET_PTR(next) == NULL) {
					return false; // NULL
				} else {
					MC2_yield();
				}
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) MAKE_POINTER(GET_PTR(next), GET_COUNT(tail) + 1));
				MC2_yield();
			} else {
				int nv = load_32(&GET_PTR(next)->value);
				store_32(retVal, nv);
				struct node * nh = MAKE_POINTER(GET_PTR(next), GET_COUNT(head)+1);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) nh);
				if (old_head == head) {
					return true;
				} else {
					MC2_yield();
				}
			}
		}
	}
}

/* ms-queue-main */
static queue_t queue;
bool succ1, succ2;

static void e(void *p) {
	int i;
	for(i=0;i<2;i++)
        enqueue(&queue, 1);
}

static void d(void *p) {
	unsigned int val;
	int i;
	for(i=0;i<2;i++)
		dequeue(&queue, &val);
}

int user_main(int argc, char **argv)
{
	init_queue(&queue, 2);

	thrd_t t1,t2;
	thrd_create(&t1, e, NULL);
	thrd_create(&t2, d, NULL);

	thrd_join(t1);
	thrd_join(t2);

	return 0;
}
