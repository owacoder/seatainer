#ifndef CCUTILITY_H
#define CCUTILITY_H

#include <memory.h>

/* Swaps a number of bytes in p and q, specified by the size parameter
 *
 * p and q must not overlap
 */
int memswap(void *p, void *q, size_t size);

#endif // CCUTILITY_H
