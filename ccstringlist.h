#ifndef STRINGLIST_H
#define STRINGLIST_H

#include "platforms.h"
#include "seaerror.h"

struct StringlistStruct;
struct BinarylistStruct;

typedef struct StringlistStruct *Stringlist;
typedef struct BinarylistStruct *Binarylist;

Stringlist stringlist_create();
Stringlist stringlist_copy(Stringlist other);
int stringlist_append(Stringlist list, const char *item);
char **stringlist_array(Stringlist list);
size_t stringlist_size(Stringlist list);
void stringlist_destroy(Stringlist list);

#endif // STRINGLIST_H
