#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#define true 1
#define false 0
#define bool int

#include "ms-queue.h"
#include "libinterface.h"

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads);
void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val);
bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal);

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads) {
	int i, j;
	MCID _fn11; struct node *newnode=malloc(sizeof (struct node));
	_fn11 = MC2_function(0, sizeof (newnode), newnode); 
	
	void * _p0 = &newnode->value;
	MCID _fn0 = MC2_function(1, MC2_PTR_LENGTH, _p0, _fn11); MC2_nextOpStore(_fn0, MCID_NODEP);
	store_32(_p0, 0);

	
	void * _p1 = &newnode->next;
	MCID _fn1 = MC2_function(1, MC2_PTR_LENGTH, _p1, _fn11); MC2_nextOpStore(_fn1, MCID_NODEP);
	store_64(_p1, (uintptr_t) MAKE_POINTER(NULL, 0LL));
	/* initialize queue */
	
	void * _p2 = &q->head;
	MCID _fn2 = MC2_function(1, MC2_PTR_LENGTH, _p2, _mq); MC2_nextOpStore(_fn2, _fn11);
	store_64(_p2, (uintptr_t) MAKE_POINTER(newnode, 0LL));

	
	void * _p3 = &q->tail;
	MCID _fn3 = MC2_function(1, MC2_PTR_LENGTH, _p3, _mq); MC2_nextOpStore(_fn3, _fn11);
	store_64(_p3, (uintptr_t) MAKE_POINTER(newnode, 0LL));
}

void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val) {
	MCID _fn13; struct node *tail;
	MCID _fn12; struct node * node_ptr = malloc(sizeof(struct node));
	_fn12 = MC2_function(0, sizeof (node_ptr), node_ptr); 
	
	void * _p4 = &node_ptr->value;
	MCID _fn4 = MC2_function(1, MC2_PTR_LENGTH, _p4, _fn12); MC2_nextOpStore(_fn4, _mval);
	store_32(_p4, val);
	
	
	void * _p5 = &node_ptr->next;
	MCID _fn5 = MC2_function(1, MC2_PTR_LENGTH, _p5, _fn12); MC2_nextOpStore(_fn5, MCID_NODEP);
	store_64(_p5, (uintptr_t) MAKE_POINTER(NULL, GET_COUNT(1)));
		
	MC2_enterLoop();
	while (true) {
		
		void * _p6 = &q->tail;
		MCID _fn6 = MC2_function(1, MC2_PTR_LENGTH, _p6, _mq); _fn13=MC2_nextOpLoad(_fn6), tail = (struct node *) load_64(_p6);
		_fn13 = MC2_function(1, sizeof (tail), tail, _fn13); 
		MCID _fn14; struct node ** tail_ptr_next= & GET_PTR(tail)->next;
		_fn14 = MC2_function(1, sizeof (tail_ptr_next), tail_ptr_next, _fn13); 

		MCID _mnext; _mnext=MC2_nextOpLoad(_fn14); struct node * next = (struct node *) load_64(tail_ptr_next);
		
		MCID _mqtail; 
		void * _p7 = &q->tail;
		MCID _fn7 = MC2_function(1, MC2_PTR_LENGTH, _p7, _mq); _mqtail=MC2_nextOpLoad(_fn7); struct node * qtail = (struct node *) load_64(_p7);
		MCID _fn15; int tqt = (tail == qtail);
		_fn15 = MC2_function(2, sizeof (tqt), tqt, _fn13, _mqtail); 
		MCID _br0;
		if (tqt) {
			MCID _fn16; _br0 = MC2_branchUsesID(_fn15, 1, 2, true);
			struct node *next_ptr=GET_PTR(next);
			_fn16 = MC2_function(1, sizeof (next_ptr), next_ptr, _mnext); 
			
			MCID _fn17; int npn = (next_ptr == NULL);
			_fn17 = MC2_function(1, sizeof (npn), npn, _fn16); 
			MCID _br1;
			if (npn) {
				MCID _fn18; _br1 = MC2_branchUsesID(_fn17, 1, 2, true);
				struct node * new_node = MAKE_POINTER(node_ptr, GET_COUNT(next) +1);
				_fn18 = MC2_function(2, sizeof (new_node), new_node, _fn12, _mnext); 
				MCID _rmw0 = MC2_nextRMW(_fn14, _mnext, _fn18);
				struct node * valread = (struct node *) rmw_64(CAS, tail_ptr_next, (uintptr_t) next, (uintptr_t) new_node);
				MCID _fn19; int vn = (valread == next);
				_fn19 = MC2_function(2, sizeof (vn), vn, _mnext, _rmw0); 
				MCID _br2;
				if (vn) {
					_br2 = MC2_branchUsesID(_fn19, 1, 2, true);
					break;
				} else {
					_br2 = MC2_branchUsesID(_fn19, 0, 2, true);
					MC2_yield();
					MC2_merge(_br2);
				}
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_fn17, 0, 2, true);
				MC2_yield();
				MC2_merge(_br1);
			}
			MCID _mnew_tailptr; _mnew_tailptr=MC2_nextOpLoad(_fn14); struct node * new_tailptr = GET_PTR(load_64( tail_ptr_next));
			MCID _fn20; struct node * newtail = MAKE_POINTER(new_tailptr, GET_COUNT(tail) + 1);
			_fn20 = MC2_function(2, sizeof (newtail), newtail, _fn13, _mnew_tailptr); 
			MCID _rmw1 = MC2_nextRMW(MCID_NODEP, _fn13, _fn20);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) newtail);
			MC2_yield();
			MC2_merge(_br0);
		} else {
			_br0 = MC2_branchUsesID(_fn15, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
MC2_exitLoop();

	MCID _rmw2 = MC2_nextRMW(MCID_NODEP, _fn13, _fn13);
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t)MAKE_POINTER(node_ptr, GET_COUNT(tail) + 1));
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
		MCID _mnext; _mnext=MC2_nextOpLoad(_mhead); struct node * next = (struct node *) load_64(&(GET_PTR(head)->next));
		
		MCID _mt; 
		void * _p10 = &q->head;
		MCID _fn10 = MC2_function(1, MC2_PTR_LENGTH, _p10, _mq); _mt=MC2_nextOpLoad(_fn10); int t = ((struct node *) load_64(_p10)) == head;
		MCID _br3;
		if (t) {
			_br3 = MC2_branchUsesID(_mt, 1, 2, true);
			if (GET_PTR(head) == GET_PTR(tail)) {

				MCID _br4;
				if (GET_PTR(next) == NULL) {
					_br4 = MC2_branchUsesID(_mnext, 1, 2, true);
					MC2_exitLoop();
					return false; // NULL
				} else {
					_br4 = MC2_branchUsesID(_mnext, 0, 2, true);
					MC2_yield();
					MC2_merge(_br4);
				}
				MCID _rmw3 = MC2_nextRMW(MCID_NODEP, _mtail, _mtail);
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) MAKE_POINTER(GET_PTR(next), GET_COUNT(tail) + 1));
				MC2_yield();
			} else {
				MCID _mnv; _mnv=MC2_nextOpLoad(_mnext); int nv = load_32(&GET_PTR(next)->value);
				MC2_nextOpStore(_mretVal, _mnv);
				store_32(retVal, nv);
				MCID _fn21; struct node * nh = MAKE_POINTER(GET_PTR(next), GET_COUNT(head)+1);
				_fn21 = MC2_function(2, sizeof (nh), nh, _mhead, _mnext); 
				MCID _rmw4 = MC2_nextRMW(MCID_NODEP, _mhead, _fn21);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) nh);
				if (old_head == head) {
					MC2_exitLoop();
					return true;
				} else {
					MC2_yield();
				}
			}
			MC2_merge(_br3);
		} else { _br3 = MC2_branchUsesID(_mt, 0, 2, true);	MC2_merge(_br3);
		 }
	}
MC2_exitLoop();

}

/* ms-queue-main */
static queue_t queue;
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
