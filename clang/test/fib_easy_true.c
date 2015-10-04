/* Adapted from: https://svn.sosy-lab.org/software/sv-benchmarks/trunk/c/pthread/fib_bench_true-unreach-call.c */

#include <assert.h>
#include <stdbool.h>
#include <threads.h>
#include "libinterface.h"

/* volatile */ int i, j;

#define NULL 0

void t1(void* arg) {

  int t1, t2, sum;

  t1 = load_32(&i);
  t2 = load_32(&j);
  sum = t1 + t2;
  store_32(&i, sum);

  t1 = load_32(&i);
  if (t1 > 3) {
    assert(0);
  }
}

void t2(void* arg) {

  int tt1, tt2, sum;

  tt1 = load_32(&j);
  tt2 = load_32(&i);
  sum = tt1 + tt2;
  store_32(&j, sum);

  tt1 = load_32(&j);
  if (tt1 > 3) {
    assert(0);
  }
}

int user_main(int argc, char **argv)
{
  thrd_t a, b;

  store_32(&i, 1);
  store_32(&j, 1);

  thrd_create(&a, t1, NULL);
  thrd_create(&b, t2, NULL);

  thrd_join(a);
  thrd_join(b);

  return 0;
}
