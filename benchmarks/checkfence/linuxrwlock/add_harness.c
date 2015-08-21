#include "llock.c"

/* ---- harness for queue ---- */

rwlock_t mylock;

void i() 
{
  lock_init(&mylock);
}
void e()
{
  bar(&mylock);
}
void d()
{
  bar(&mylock);
}
