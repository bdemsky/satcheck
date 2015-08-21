#include <threads.h>
#include "libinterface.h"

struct node {
	// atomic<struct node*> next;
	struct node * next;
	int data;
        
	node(int d = 0) : data(d) {
        store_64(&next, 0);
	}
};


class spsc_queue {
public:
	spsc_queue()
	{
		node* n = new node ();
		head = n;
		tail = n;
	}

	~spsc_queue()
	{
	}

	void enqueue(int data)
	{
		node* n = new node(data);
		store_64(&head->next, (uint64_t)n);
		head = n;
	}

	int dequeue() {
		int data=0;
		while (0 == data) {
			data = try_dequeue();
		}
		return data;
	}
        
private:

	struct node* head;
	struct node* tail;

	int try_dequeue() {
		node* t = tail;
		node* n = (node *)load_64(&t->next);
		if (0 == n) {
			return 0;
		}
		int data = load_64(&n->data);
		tail = n;
		return data;
	}
};


spsc_queue *q;

void thread(unsigned thread_index)
{
	for (int i = 0; i < 40; i++) {
		if (0 == thread_index)
			{
				q->enqueue(11);
			}
		else
			{
				int d = q->dequeue();
				// RL_ASSERT(11 == d);
			}
	}
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
