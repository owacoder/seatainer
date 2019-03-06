#ifndef CCUTILITY_H
#define CCUTILITY_H

#include <memory.h>

/* Swaps a number of bytes in p and q, specified by the size parameter
 *
 * p and q must not overlap
 */
int memswap(void *p, void *q, size_t size);

unsigned pearson_hash(const char *data, size_t size);

/* Returns 1 if number is prime, 0 otherwise */
int is_prime(size_t number);

/* Returns next prime number greater than `number` */
size_t next_prime(size_t number);

#endif /* CCUTILITY_H */
