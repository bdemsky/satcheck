#include <stdio.h>
#include "threads.h"
#include "libinterface.h"

int x;
int y;
int gr1;
int gr2;

static void a(void *obj)
{
	int r1 = load_32(&y);
	store_32(&x, r1);

	store_32(&gr1, r1);
	printf("r1=%d\n",r1);
}

static void b(void *obj)
{
	int r2=load_32(&x);
	store_32(&y, 42);

	store_32(&gr2, r2);
	printf("r2=%d\n",r2);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	store_32(&x, 0);
	store_32(&y, 0);

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");
	int lgr1=load_32(&gr1);

	int lgr2=load_32(&gr2);
	if (lgr1==42) {
		if (lgr2==42)
			;
		else {
		}
	} else
		;

	return 0;
}
