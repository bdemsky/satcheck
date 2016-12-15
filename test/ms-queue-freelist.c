#include "threads.h"
#include <stdlib.h>
// #include "librace.h"
// #include "model-assert.h"

#include "libinterface.h"
#include "ms-queue-freelist.h"


#define MAX_FREELIST 4 /* Each thread can own up to MAX_FREELIST free nodes */
#define INITIAL_FREE 2 /* Each thread starts with INITIAL_FREE free nodes */

#define POISON_IDX 0x666

static unsigned int (*free_lists)[MAX_FREELIST];

/* Search this thread's free list for a "new" node */
static unsigned int new_node()
{
	int i;
	int t = get_thread_num();
	for (i = 0; i < MAX_FREELIST; i++) {
		unsigned int node = load_32(&free_lists[t][i]);
		if (node) {
			store_32(&free_lists[t][i], 0);
			return node;
		}
	}
	/* free_list is empty? */
	// MODEL_ASSERT(0);
	return 0;
}

/* Place this node index back on this thread's free list */
static void reclaim(unsigned int node)
{
	int i;
	int t = get_thread_num();

	/* Don't reclaim NULL node */
	// MODEL_ASSERT(node);

	for (i = 0; i < MAX_FREELIST; i++) {
		/* Should never race with our own thread here */
		unsigned int idx = load_32(&free_lists[t][i]);

		/* Found empty spot in free list */
		if (idx == 0) {
			store_32(&free_lists[t][i], node);
			return;
		}
	}
	/* free list is full? */
	// MODEL_ASSERT(0);
}

void init_queue(queue_t *q, int num_threads)
{
	int i, j;

	/* Initialize each thread's free list with INITIAL_FREE pointers */
	/* The actual nodes are initialized with poison indexes */
	free_lists = malloc(num_threads * sizeof(*free_lists));
	for (i = 0; i < num_threads; i++) {
		for (j = 0; j < INITIAL_FREE; j++) {
			free_lists[i][j] = 2 + i * MAX_FREELIST + j;
			store_32(&q->nodes[free_lists[i][j]].next, MAKE_POINTER(POISON_IDX, 0));
		}
	}

	/* initialize queue */
	store_32(&q->head, MAKE_POINTER(1, 0));
	store_32(&q->tail, MAKE_POINTER(1, 0));
	store_32(&q->nodes[1].next, MAKE_POINTER(0, 0));
}

void enqueue(queue_t *q, unsigned int val)
{
	int success = 0;
	unsigned int node;
	pointer tail;
	pointer next;
	pointer tmp;

	node = new_node();
	store_32(&q->nodes[node].value, val);
	tmp = load_32(&q->nodes[node].next);
	// manually inlined macro: set_ptr(&tmp, 0); // NULL
	tmp = (tmp & ~PTR_MASK);
	store_32(&q->nodes[node].next, tmp);

	while (!success) {
		pointer_t * qt = &q->tail;
		tail = load_32(qt);
		pointer_t * qn = &q->nodes[tail & PTR_MASK].next;
		next = load_32(qn);
		if (tail == load_32(&q->tail)) {

			/* Check for uninitialized 'next' */
			// MODEL_ASSERT(get_ptr(next) != POISON_IDX);

			if ((next & PTR_MASK) == 0) { // == NULL
				pointer value = MAKE_POINTER(node, ((next & COUNT_MASK) >> 32) + 1);
				success = rmw_32(CAS, &q->nodes[tail & PTR_MASK].next,
						(uint32_t)&next, value);
			}
			if (!success) {
				unsigned int ptr = (load_32(&q->nodes[tail & PTR_MASK].next) & PTR_MASK);
				pointer value = MAKE_POINTER(ptr,
                                             ((tail & COUNT_MASK) >> 32) + 1);
				rmw_32(CAS, &q->tail,
						(uint32_t)&tail, value);
				thrd_yield();
			}
		}
	}
	rmw_32(CAS, &q->tail,
		   (uint32_t)&tail,
		   MAKE_POINTER(node, ((tail & COUNT_MASK) >> 32) + 1));
}

bool dequeue(queue_t *q, unsigned int *retVal)
{
	int success = 0;
	pointer head;
	pointer tail;
	pointer next;

	while (!success) {
		head = load_32(&q->head);
		tail = load_32(&q->tail);
		next = load_32(&q->nodes[(head & PTR_MASK)].next);
		if (load_32(&q->head) == head) {
			if ((head & PTR_MASK) == (tail & PTR_MASK)) {

				/* Check for uninitialized 'next' */
				// MODEL_ASSERT(get_ptr(next) != POISON_IDX);

				if (get_ptr(next) == 0) { // NULL
					return false; // NULL
				}
				rmw_32(CAS, &q->tail,
					   (uint32_t)&tail,
					   MAKE_POINTER((next & PTR_MASK), ((tail & COUNT_MASK) >> 32) + 1));
				thrd_yield();
			} else {
				*retVal = load_32(&q->nodes[(next & PTR_MASK)].value);
				success = rmw_32(CAS, &q->head,
								(uint32_t)&head,
					    MAKE_POINTER((next & PTR_MASK), ((head & COUNT_MASK) >> 32) + 1));
				if (!success)
					thrd_yield();
			}
		}
	}
	reclaim((head & PTR_MASK));
	return true;
}

/* ms-queue-main */
static int procs = 2;
static queue_t *queue;
static thrd_t *threads;
static unsigned int *input;
static unsigned int *output;
static int num_threads;

int get_thread_num()
{
	thrd_t curr = thrd_current();
	int i;
	for (i = 0; i < num_threads; i++)
		if (curr.priv == threads[i].priv)
			return i;
	/* MODEL_ASSERT(0); */
	return -1;
}

bool succ1, succ2;

static void main_task(void *param)
{
	unsigned int val;
	int pid = *((int *)param);
	if (!pid) {
		input[0] = 17;
		enqueue(queue, input[0]);
		succ1 = dequeue(queue, &output[0]);
		//printf("Dequeue: %d\n", output[0]);
	} else {
		input[1] = 37;
		enqueue(queue, input[1]);
		succ2 = dequeue(queue, &output[1]);
	}
}

int user_main(int argc, char **argv)
{
	int i;
	int *param;
	unsigned int in_sum = 0, out_sum = 0;

	queue = calloc(1, sizeof(*queue));
	// MODEL_ASSERT(queue);

	num_threads = procs;
	threads = malloc(num_threads * sizeof(thrd_t));
	param = malloc(num_threads * sizeof(*param));
	input = calloc(num_threads, sizeof(*input));
	output = calloc(num_threads, sizeof(*output));

	init_queue(queue, num_threads);
	for (i = 0; i < num_threads; i++) {
		param[i] = i;
		thrd_create(&threads[i], main_task, &param[i]);
	}
	for (i = 0; i < num_threads; i++)
		thrd_join(threads[i]);

	for (i = 0; i < num_threads; i++) {
		in_sum += input[i];
		out_sum += output[i];
	}
	for (i = 0; i < num_threads; i++)
		printf("input[%d] = %u\n", i, input[i]);
	for (i = 0; i < num_threads; i++)
		printf("output[%d] = %u\n", i, output[i]);
	/* if (succ1 && succ2) */
	/* 	MODEL_ASSERT(in_sum == out_sum); */
	/* else */
	/* 	MODEL_ASSERT (false); */

	free(param);
	free(threads);
	free(queue);

	return 0;
}
