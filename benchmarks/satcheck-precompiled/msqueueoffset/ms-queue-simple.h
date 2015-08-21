typedef struct node {
	unsigned int value;
	struct node * next;
} node_t;

typedef struct {
	struct node * head;
	struct node * tail;
} queue_t;

//void init_queue(queue_t *q, int num_threads);
//void enqueue(queue_t *q, unsigned int val);
//bool dequeue(queue_t *q, unsigned int *retVal);
int get_thread_num();

