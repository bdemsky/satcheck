#include "lsl_protos.h"

/* ---- data types  ---- */

typedef int value_t;

typedef struct node {
  struct node *next;
  value_t value;
} node_t;

typedef struct queue {
  node_t *head;
  node_t *tail;
} queue_t;

/* ---- operations  ---- */

void init_queue(queue_t *queue) 
{
  node_t *node = lsl_malloc_noreuse(sizeof(node_t));
  node->next = 0;
  queue->head = queue->tail = node;
}

void enqueue(queue_t *queue, value_t value)
{
  node_t *node;
  node_t *tail;
  node_t *next;

  node = lsl_malloc_noreuse(sizeof(node_t));
  node->value = value;
  node->next = 0;
  while (true) {
    tail = queue->tail;
    next = tail->next;
    if (tail == queue->tail)
      if (next == 0) {
        if (lsl_cas_64(&tail->next, next, node))
          break;
      } else
        lsl_cas_ptr(&queue->tail, tail, next);
  }
  lsl_cas_ptr(&queue->tail, tail, node);
}

boolean_t dequeue(queue_t *queue, value_t *pvalue)
{
  node_t *head;
  node_t *tail;
  node_t *next;

  while (true) {
    head = queue->head;
    tail = queue->tail;
    next = head->next;
    if (head == queue->head) {
      if (head == tail) {
        if (next == 0)
          return false;
        lsl_cas_ptr(&queue->tail, tail, next);
      } else {
        *pvalue = next->value;
        if (lsl_cas_ptr(&queue->head, head, next))
          break;
      } 
    }
  }
  lsl_free_noreuse(head);
  return true;
}
