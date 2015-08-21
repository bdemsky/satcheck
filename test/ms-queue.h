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

#define MAKE_POINTER(ptr, count)	(struct node *) (((count) << 48) | (uintptr_t)(ptr))
#define PTR_MASK 0xffffffffffffLL
#define COUNT_MASK (0xffffLL << 48)
#define GET_PTR(x) ((struct node *)(((uintptr_t) x) & PTR_MASK))
#define GET_COUNT(x) (((uintptr_t) x) >> 48)
