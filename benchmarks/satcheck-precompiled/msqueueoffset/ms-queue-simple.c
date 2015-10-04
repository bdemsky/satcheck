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
bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal, MCID * retval);

queue_t queue;

void init_queue(MCID _mq, queue_t *q, MCID _mnum_threads, int num_threads) {
	MCID _fn0; struct node *newnode=(struct node *)malloc(sizeof (struct node));
	_fn0 = MC2_function_id(0, 0, sizeof (newnode), (uint64_t)newnode); 
	
	MC2_nextOpStoreOffset(_fn0, MC2_OFFSET(struct node *, value), MCID_NODEP);
	store_32(&newnode->value, 0);

	
	MC2_nextOpStoreOffset(_fn0, MC2_OFFSET(struct node *, next), MCID_NODEP);
	store_64(&newnode->next, (uintptr_t) NULL);
		/* initialize queue */
	
	MC2_nextOpStoreOffset(_mq, MC2_OFFSET(queue_t *, head), _fn0);
	store_64(&q->head, (uintptr_t) newnode);
	
	
	MC2_nextOpStoreOffset(_mq, MC2_OFFSET(queue_t *, tail), _fn0);
	store_64(&q->tail, (uintptr_t) newnode);
}

void enqueue(MCID _mq, queue_t *q, MCID _mval, unsigned int val) {
	MCID _mtail; struct node *tail;
	MCID _fn1; struct node * node_ptr; node_ptr = (struct node *)malloc(sizeof(struct node));

	_fn1 = MC2_function_id(0, 0, sizeof (node_ptr), (uint64_t)node_ptr); 	
	MC2_nextOpStoreOffset(_fn1, MC2_OFFSET(struct node *, value), _mval);
	store_32(&node_ptr->value, val);
	
	
	MC2_nextOpStoreOffset(_fn1, MC2_OFFSET(struct node *, next), MCID_NODEP);
	store_64(&node_ptr->next, (uintptr_t) NULL);
			
	MC2_enterLoop();
	while (true) {

		_mtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, tail)), tail = (struct node *) load_64(&q->tail);

		MCID _mnext; _mnext=MC2_nextOpLoadOffset(_mtail, MC2_OFFSET(struct node *, next)); struct node * next = (struct node *) load_64(&tail->next);
		
		MCID _mqtail; _mqtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, tail)); struct node * qtail = (struct node *) load_64(&q->tail);

		MCID _br0;
		
		MCID _cond0_m;
		
		int _cond0 = MC2_equals(_mtail, (uint64_t)tail, _mqtail, (uint64_t)qtail, &_cond0_m);
		if (_cond0) {
			MCID _br1;
			_br0 = MC2_branchUsesID(_cond0_m, 1, 2, true);
			
			int _cond1 = next;
			MCID _cond1_m = MC2_function_id(1, 1, sizeof(_cond1), _cond1, _mnext);
			if (_cond1) {
				_br1 = MC2_branchUsesID(_mnext, 1, 2, true);
				MC2_yield();
				MC2_merge(_br1);
			} else {
				_br1 = MC2_branchUsesID(_mnext, 0, 2, true);
				MCID _rmw0 = MC2_nextRMWOffset(_mtail, MC2_OFFSET(struct node *, next), _mnext, _fn1);
				struct node * valread = (struct node *) rmw_64(CAS, &tail->next, (uintptr_t) next, (uintptr_t) node_ptr);
				
				MCID _br2;
				
				MCID _cond2_m;
				
				int _cond2 = MC2_equals(_mnext, (uint64_t)next, _rmw0, (uint64_t)valread, &_cond2_m);
				if (_cond2) {
					_br2 = MC2_branchUsesID(_cond2_m, 1, 2, true);
					break;
				} else {_br2 = MC2_branchUsesID(_cond2_m, 0, 2, true);
				
					MC2_merge(_br2);
				}
				MC2_merge(_br1);
			}
			MCID _mnew_tailptr; _mnew_tailptr=MC2_nextOpLoadOffset(_mtail, MC2_OFFSET(struct node *, next)); struct node * new_tailptr = (struct node *)load_64(&tail->next);
			MCID _rmw1 = MC2_nextRMWOffset(_mq, MC2_OFFSET(queue_t *, tail), _mtail, _mnew_tailptr);
			rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) new_tailptr);
			MC2_yield();
			MC2_merge(_br0);
		} else { 
			_br0 = MC2_branchUsesID(_cond0_m, 0, 2, true);
			MC2_yield();
			MC2_merge(_br0);
		}
	}
MC2_exitLoop();

	
	MCID _rmw2 = MC2_nextRMWOffset(_mq, MC2_OFFSET(queue_t *, tail), _mtail, _fn1);
	rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) node_ptr);
}

bool dequeue(MCID _mq, queue_t *q, MCID _mretVal, unsigned int *retVal, MCID * retval) {
	MC2_enterLoop();
	while (true) {
		MCID _mhead; _mhead=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, head)); struct node * head = (struct node *) load_64(&q->head);
		MCID _mtail; _mtail=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, tail)); struct node * tail = (struct node *) load_64(&q->tail);
		MCID _mnext; _mnext=MC2_nextOpLoadOffset(_mhead, MC2_OFFSET(struct node *, next)); struct node * next = (struct node *) load_64(&head->next);

		MCID _mheadr; _mheadr=MC2_nextOpLoadOffset(_mq, MC2_OFFSET(queue_t *, head)); struct node * headr = (struct node *) load_64(&q->head);

		MCID _br3;
		
		MCID _cond3_m;
		
		int _cond3 = MC2_equals(_mhead, (uint64_t)head, _mheadr, (uint64_t)headr, &_cond3_m);
		if (_cond3) {
			MCID _br4;
			_br3 = MC2_branchUsesID(_cond3_m, 1, 2, true);
			
			MCID _cond4_m;
			
			int _cond4 = MC2_equals(_mhead, (uint64_t)head, _mtail, (uint64_t)tail, &_cond4_m);
			if (_cond4) {
				MCID _br5;
				_br4 = MC2_branchUsesID(_cond4_m, 1, 2, true);
				
				int _cond5 = next;
				MCID _cond5_m = MC2_function_id(2, 1, sizeof(_cond5), _cond5, _mnext);
				if (_cond5) {
					_br5 = MC2_branchUsesID(_mnext, 1, 2, true);
					MC2_yield();
					MC2_merge(_br5);
				} else {
					*retval = MCID_NODEP;
					_br5 = MC2_branchUsesID(_mnext, 0, 2, true);
					MC2_exitLoop();
					return false; // NULL
				}
				MCID _rmw3 = MC2_nextRMWOffset(_mq, MC2_OFFSET(queue_t *, tail), _mtail, _mnext);
				rmw_64(CAS, &q->tail, (uintptr_t) tail, (uintptr_t) next);
				MC2_yield();
				MC2_merge(_br4);
			} else {
				_br4 = MC2_branchUsesID(_cond4_m, 0, 2, true);
				_mretVal=MC2_nextOpLoadOffset(_mnext, MC2_OFFSET(struct node *, value)), *retVal = load_32(&next->value);
				MCID _rmw4 = MC2_nextRMWOffset(_mq, MC2_OFFSET(queue_t *, head), _mhead, _mnext);
				struct node *old_head = (struct node *) rmw_64(CAS, &q->head,
																											 (uintptr_t) head, 
																											 (uintptr_t) next);

				MCID _br6;
				
				MCID _cond6_m;
				
				int _cond6 = MC2_equals(_mhead, (uint64_t)head, _rmw4, (uint64_t)old_head, &_cond6_m);
				if (_cond6) {
					*retval = MCID_NODEP;
					_br6 = MC2_branchUsesID(_cond6_m, 1, 2, true);
					MC2_exitLoop();
					return true;
				} else {
					_br6 = MC2_branchUsesID(_cond6_m, 0, 2, true);
					MC2_yield();
					MC2_merge(_br6);
				}
				MC2_merge(_br4);
			}
			MC2_merge(_br3);
		} else {
			_br3 = MC2_branchUsesID(_cond3_m, 0, 2, true);
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
	for(i=0;i<PROBLEMSIZE;i++) {
		enqueue(MCID_NODEP, &queue, MCID_NODEP, 1);
	}
MC2_exitLoop();

}

static void d(void *p) {
	unsigned int val;
	int i;
	MC2_enterLoop();
	for(i=0;i<PROBLEMSIZE;i++) {
		int r;
		MCID _rv0;
		r = dequeue(MCID_NODEP, &queue, MCID_NODEP, &val, &_rv0);
	}
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
