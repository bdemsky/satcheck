#include <threads.h>
#include <inttypes.h>
#include "libinterface.h"

static int zero;

int user_main(int argc, char **argv)
{
    store_32(&zero, 0);

    int val = load_32(&zero);

    int val2 = 0;
    MCID _mval2 = MCID_NODEP;

    /** this equality check contains one NODEP and should work. **/
    if(val == val2)
    {
        return 1;
    }

    return 0;
}