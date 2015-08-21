typedef struct node {
	atomic_int value;
	atomic_address next;
} node_t;

typedef struct {
	atomic_address head;
	atomic_address tail;
} queue_t;

//void init_queue(queue_t *q, int num_threads);
//void enqueue(queue_t *q, unsigned int val);
//bool dequeue(queue_t *q, unsigned int *retVal);
int get_thread_num();

