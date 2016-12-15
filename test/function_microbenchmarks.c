#include <stdio.h>
#include <threads.h>

#include "libinterface.h"

int x;
int y;
int gr1;
int gr2;

static void a(void *obj)
{
    int r1=load_32(&y);
    if (r1) {
        r1=42;
        store_32(&x, r1);
    } else {
        r1=0;
    }

    store_32(&x, r1);

    int r2 = load_32(&x);
    int cond = r1 == r2;
    if (cond) {
        r2++;
    }

    int cas_value = rmw_32(CAS, &x, r1, r2);
    int cas_value1;
    cas_value1 = rmw_32(ADD, &y, r1, r1);
}

int user_main(int argc, char **argv)
{
    thrd_t t1; thrd_t t2;

    store_32(&x, 0);
    store_32(&y, 0);

    printf("Main thread: creating 2 threads\n");
    thrd_create(&t1, (thrd_start_t)&a, NULL);

    thrd_join(t1);
    printf("Main thread is finished\n");
    int lgr1=load_32(&gr1);

    int lgr2=load_32(&gr2);
    if (lgr1==42) {
        if (lgr2==42) {
        } else {
        }
    } else {
    }

    return 0;
}
