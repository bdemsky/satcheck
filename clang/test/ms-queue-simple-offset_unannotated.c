#define PROBLEMSIZE 10
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#define true 1
#define false 0
#define bool int

#include "ms-queue-simple.h"
#include "libinterface.h"

void init_queue(queue_t *q, int num_threads);
void enqueue(queue_t *q, unsigned int val);
bool dequeue(queue_t *q, unsigned int *retVal);

queue_t queue;

void init_queue(queue_t *q, int num_threads) {
	struct node *newnode=malloc(sizeof (struct node));
	store_32(&newnode->value, 0);
	
	store_64(&newnode->next, (uintptr_t) NULL);
		/* initialize queue */
	store_64(&q->head, (uintptr_t) newnode);
	
	store_64(&q->tail, (uintptr_t) newnode);
}

void enqueue(queue_t *q, unsigned int val) {
	struct node *tail;
	struct node * node_ptr = malloc(sizeof(struct node));
	
	store_32(&node_ptr->value, val);
	
	
	store_64(&node_ptr->next, (uintptr_t) NULL);

	while (true) {

		tail = (struct node *) load_64(&q->tail);
		struct node ** tail_ptr_next= & tail->next;

		struct node * next = (struct node *) load_64(tail_ptr_next);
		
		struct node * qtail = (struct node *) load_64(&q->tail);

		if (tail == qtail) {
			if (next) {
				MC2_yield();
			} else {
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) node_ptr);
				if (next == valread) {
					break;
				}
			}
			struct node * new_tailptr = (struct node *)load_64( tail_ptr_next);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) new_tailptr);
			MC2_yield();
		} else { 
			MC2_yield();
		}
	}
	
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) node_ptr);
}

bool dequeue(queue_t *q, unsigned int *retVal) {
	while (true) {
		struct node * head = (struct node *) load_64(&q->head);
		struct node * tail = (struct node *) load_64(&q->tail);
		struct node * next = (struct node *) load_64(&head->next);

		struct node * headr = (struct node *) load_64(&q->head);

		if (head == headr) {

			if (head == tail) {

				if (next) {
					MC2_yield();
				} else {
					return false; // NULL
				}
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) next);
				MC2_yield();
			} else {
				void * nextval= &(next->value);
				*retVal = load_32(&next->value);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) next);
				if (head == old_head) {
					MC2_exitLoop();
					return true;
				} else {
					MC2_yield();
				}
			}
		} else {
			MC2_yield();
		}
	}
}

/* ms-queue-main */
bool succ1, succ2;

static void e(void *p) {
	int i;
	for(i=0;i<4;i++)
		enqueue(&queue, 1);
}

static void d(void *p) {
	unsigned int val;
	int i;
	for(i=0;i<PROBLEMSIZE;i++)
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
