typedef struct node {
	std::atomic<int> value;
	std::atomic<struct node *> next;
} node_t;

typedef struct {
	std::atomic<struct node *> head;
	std::atomic<struct node *> tail;
} queue_t;

//void init_queue(queue_t *q, int num_threads);
//void enqueue(queue_t *q, unsigned int val);
//bool dequeue(queue_t *q, unsigned int *retVal);
int get_thread_num();

