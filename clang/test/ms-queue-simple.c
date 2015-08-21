#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#define true 1
#define false 0
#define bool int

#include "ms-queue-simple.h"
#include "libinterface.h"

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads);
void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val);
bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal);

queue_t queue;

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads) {
	MCID _fn13; struct node *newnode=malloc(sizeof (struct node));
	_fn13 = MC2_function(0, sizeof (newnode), newnode); 
	
	void * _p0 = &newnode->value;
	MCID _fn0 = MC2_function(1, MC2_PTR_LENGTH, _p0, _fn13); MC2_nextOpStore(_fn0, MCID_NODEP);
	store_32(_p0, 0);

	
	void * _p1 = &newnode->next;
	MCID _fn1 = MC2_function(1, MC2_PTR_LENGTH, _p1, _fn13); MC2_nextOpStore(_fn1, MCID_NODEP);
	store_64(_p1, (uintptr_t) NULL);
		/* initialize queue */
	
	void * _p2 = &q->head;
	MCID _fn2 = MC2_function(1, MC2_PTR_LENGTH, _p2, _mq); MC2_nextOpStore(_fn2, _fn13);
	store_64(_p2, (uintptr_t) newnode);
	
	
	void * _p3 = &q->tail;
	MCID _fn3 = MC2_function(1, MC2_PTR_LENGTH, _p3, _mq); MC2_nextOpStore(_fn3, _fn13);
	store_64(_p3, (uintptr_t) newnode);
}

void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val) {
	MCID _fn15; struct node *tail;
	MCID _fn14; struct node * node_ptr = malloc(sizeof(struct node));
	_fn14 = MC2_function(0, sizeof (node_ptr), node_ptr); 
	
	void * _p4 = &node_ptr->value;
	MCID _fn4 = MC2_function(1, MC2_PTR_LENGTH, _p4, _fn14); MC2_nextOpStore(_fn4, _mval);
	store_32(_p4, val);
	
	
	void * _p5 = &node_ptr->next;
	MCID _fn5 = MC2_function(1, MC2_PTR_LENGTH, _p5, _fn14); MC2_nextOpStore(_fn5, MCID_NODEP);
	store_64(_p5, (uintptr_t) NULL);
			
	MC2_enterLoop();
	while (true) {
		
		void * _p6 = &q->tail;
		MCID _fn6 = MC2_function(1, MC2_PTR_LENGTH, _p6, _mq); _fn15=MC2_nextOpLoad(_fn6), tail = (struct node *) load_64(_p6);
		_fn15 = MC2_function(1, sizeof (tail), tail, _fn15); 
		MCID _fn16; struct node ** tail_ptr_next= & tail->next;
		_fn16 = MC2_function(1, sizeof (tail_ptr_next), tail_ptr_next, _fn15); 

		MCID _mnext; _mnext=MC2_nextOpLoad(_fn16); struct node * next = (struct node *) load_64(tail_ptr_next);
		
		MCID _mqtail; 
		void * _p7 = &q->tail;
		MCID _fn7 = MC2_function(1, MC2_PTR_LENGTH, _p7, _mq); _mqtail=MC2_nextOpLoad(_fn7); struct node * qtail = (struct node *) load_64(_p7);
		MCID _fn17; int tqt = (tail == qtail);
		_fn17 = MC2_function(2, sizeof (tqt), tqt, _fn15, _mqtail); 
		MCID _br0;
		if (tqt) {
			
			MCID _br1;
			_br0 = MC2_branchUsesID(_fn17, 1, 2, true);
			if (next) {
				_br1 = MC2_branchUsesID(_mnext, 1, 2, true);
				MC2_yield();
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_mnext, 0, 2, true);
				MCID _rmw0 = MC2_nextRMW(_fn16, _mnext, _fn14);
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) node_ptr);
				MCID _fn18; int vn = (valread == next);
				_fn18 = MC2_function(2, sizeof (vn), vn, _mnext, _rmw0); 
				MCID _br2;
				if (vn) {
					_br2 = MC2_branchUsesID(_fn18, 1, 2, true);
					break;
				} else {
					_br2 = MC2_branchUsesID(_fn18, 0, 2, true);
					MC2_yield();
					MC2_merge(_br2);
				}
				MC2_merge(_br1);
			}
			MCID _mnew_tailptr; _mnew_tailptr=MC2_nextOpLoad(_fn16); struct node * new_tailptr = (struct node *)load_64( tail_ptr_next);
			MCID _rmw1 = MC2_nextRMW(MCID_NODEP, _fn15, _mnew_tailptr);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) new_tailptr);
			MC2_yield();
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_fn17, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
MC2_exitLoop();

	MCID _rmw2 = MC2_nextRMW(MCID_NODEP, _fn15, _fn14);
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) node_ptr);
}

bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal) {
	MC2_enterLoop();
	while (true) {
		MCID _mhead; 
		void * _p8 = &q->head;
		MCID _fn8 = MC2_function(1, MC2_PTR_LENGTH, _p8, _mq); _mhead=MC2_nextOpLoad(_fn8); struct node * head = (struct node *) load_64(_p8);
		MCID _mtail; 
		void * _p9 = &q->tail;
		MCID _fn9 = MC2_function(1, MC2_PTR_LENGTH, _p9, _mq); _mtail=MC2_nextOpLoad(_fn9); struct node * tail = (struct node *) load_64(_p9);
		MCID _mnext; 
		void * _p10 = &head->next;
		MCID _fn10 = MC2_function(1, MC2_PTR_LENGTH, _p10, _mhead); _mnext=MC2_nextOpLoad(_fn10); struct node * next = (struct node *) load_64(_p10);
		
		MCID _mt; 
		void * _p11 = &q->head;
		MCID _fn11 = MC2_function(1, MC2_PTR_LENGTH, _p11, _mq); _mt=MC2_nextOpLoad(_fn11); int t = ((struct node *) load_64(_p11)) == head;
		MCID _br3;
		if (t) {
			MCID _fn19; _br3 = MC2_branchUsesID(_mt, 1, 2, true);
			int ht = (head == tail);
			_fn19 = MC2_function(2, sizeof (ht), ht, _mhead, _mtail); 
			MCID _br4;
			if (ht) {

				MCID _br5;
				_br4 = MC2_branchUsesID(_fn19, 1, 2, true);
				if (next) {
					_br5 = MC2_branchUsesID(_mnext, 1, 2, true);
					MC2_yield();
					MC2_merge(_br5);
				} else {
					_br5 = MC2_branchUsesID(_mnext, 0, 2, true);
					MC2_exitLoop();
					return false; // NULL
				}
				MCID _rmw3 = MC2_nextRMW(MCID_NODEP, _mtail, _mnext);
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) next);
				MC2_yield();
				MC2_merge(_br4);
			} else {
				MCID _mnv; _br4 = MC2_branchUsesID(_fn19, 0, 2, true);
				
				void * _p12 = &next->value;
				MCID _fn12 = MC2_function(1, MC2_PTR_LENGTH, _p12, _mnext); _mnv=MC2_nextOpLoad(_fn12); int nv = load_32(_p12);
				MC2_nextOpStore(_mretVal, _mnv);
				store_32(retVal, nv);
				MCID _rmw4 = MC2_nextRMW(MCID_NODEP, _mhead, _mnext);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) next);
				MCID _fn20; int ohh = (old_head == head);
				_fn20 = MC2_function(2, sizeof (ohh), ohh, _mhead, _rmw4); 
				MCID _br6;
				if (ohh) {
					_br6 = MC2_branchUsesID(_fn20, 1, 2, true);
					MC2_exitLoop();
					return true;
				} else {
					_br6 = MC2_branchUsesID(_fn20, 0, 2, true);
					MC2_yield();
					MC2_merge(_br6);
				}
				MC2_merge(_br4);
			}
			MC2_merge(_br3);
		} else {
			_br3 = MC2_branchUsesID(_mt, 0, 2, true);
			MC2_yield();
			MC2_merge(_br3);
		}
	}
MC2_exitLoop();

}

/* ms-queue-main */
bool succ1, succ2;

static void e(void *p) {
	int i;
	MC2_enterLoop();
	for(i=0;i<2;i++)
        enqueue(MCID_NODEP, &queue, MCID_NODEP, 1);
MC2_exitLoop();

}

static void d(void *p) {
	unsigned int val;
	int i;
	MC2_enterLoop();
	for(i=0;i<2;i++)
		dequeue(MCID_NODEP, &queue, MCID_NODEP, &val);
MC2_exitLoop();

}

int user_main(int argc, char **argv)
{
	init_queue(MCID_NODEP, &queue, MCID_NODEP, 2);

	thrd_t t1,t2;
	thrd_create(&t1, e, NULL);
	thrd_create(&t2, d, NULL);

	thrd_join(t1);
	thrd_join(t2);

	return 0;
}
