#include "utility.h"

int memswap(void *p, void *q, size_t size)
{
    char *pchar = p, *qchar = q;

    while (size--)
    {
        char tmp = *pchar;
        *pchar++ = *qchar;
        *qchar++ = tmp;
    }

    return 0;
}
