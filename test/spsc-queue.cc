#include <threads.h>
#include "libinterface.h"

struct node {
	// atomic<struct node*> next;
	struct node * next;
	int data;
        
	node(MCID _md, int d = 0) : data(d) {
        MC2_nextOpStore(MCID_NODEP, MCID_NODEP);
        store_64(&next, 0);
	}
};


class spsc_queue {
public:
	spsc_queue()
	{
		MCID _mn; node* n = new node (MCID_NODEP); _mn = MC2_function(0, MC2_PTR_LENGTH, (uint64_t) n); 
		head = n;
		tail = n;
	}

	~spsc_queue()
	{
	}

	void enqueue(MCID _mdata, int data)
	{
		MCID _mn; node* n = new node(MCID_NODEP, data); _mn = MC2_function(0, MC2_PTR_LENGTH, (uint64_t) n); 
		MC2_nextOpStore(MCID_NODEP, _mn);
		store_64(&head->next->data, (uint64_t)n);
		head = n;
	}

	int dequeue() {
		int data=0;
		MC2_enterLoop();
		while (0 == data) {
			data = try_dequeue();
		}
MC2_exitLoop();

		return data;
	}
        
private:

	struct node* head;
	struct node* tail;

	int try_dequeue() {
		node* t = tail;
		MCID _mn; 
		void * _p0 = &t->next;
		MCID _fn0 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p0, MCID_NODEP); _mn=MC2_nextOpLoad(_fn0); node* n = (node *)load_64(_p0);
		MCID _br0;
		if (0 == n) {
			_br0 = MC2_branchUsesID(_mn, 1, 2, true);
			return 0;
		} else { _br0 = MC2_branchUsesID(_mn, 0, 2, true);	MC2_merge(_br0);
		 }
		MCID _mdata; 
		void * _p1 = &n->data;
		MCID _fn1 = MC2_function(1, MC2_PTR_LENGTH, (uint64_t)_p1, _mn); _mdata=MC2_nextOpLoad(_fn1); int data = load_64(_p1);
		tail = n;
		return data;
	}
};


spsc_queue *q;

void thread(unsigned thread_index)
{
	MC2_enterLoop();
	for (int i = 0; i < 40; i++) {
		MCID _br1;
		if (0 == thread_index)
			{
				_br1 = MC2_branchUsesID(MCID_NODEP, 1, 2, true);
				q->enqueue(MCID_NODEP, 11);
				MC2_merge(_br1);
			}
		else
			{
				_br1 = MC2_branchUsesID(MCID_NODEP, 0, 2, true);
				int d = q->dequeue();
				// RL_ASSERT(11 == d);
				MC2_merge(_br1);
			}
	}
MC2_exitLoop();

}

int user_main(int argc, char **argv)
{
	thrd_t A; thrd_t B;
        
	q = new spsc_queue();
        
	thrd_create(&A, (thrd_start_t)&thread, (void *)0);
	thrd_create(&B, (thrd_start_t)&thread, (void *)1);
	thrd_join(A);
	thrd_join(B);


	return 0;
}
