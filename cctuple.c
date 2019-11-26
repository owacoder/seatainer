/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "cctuple.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "utility.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
/** @brief Tuple contains a tuple of any number of distinct types, along with a buffer to manage them
  */
struct Tuple {
    /** Specifies number of items in the tuple. */
    size_t args;

    /** A flat layout of the ElementData structure, followed by the number of ContainerElementType instances specified by args, followed by the instance data of the items themselves */
    void *buf[];
};

size_t cc_t_sizeof(size_t num, ...) {
    size_t result = sizeof(struct Tuple) + cc_el_sizeof() + num * sizeof(ContainerElementType);

    va_list args;
    va_start(args, num);

    while (num--) {
        ContainerElementType type = va_arg(ContainerElementType, args);

        if (type == El_Tuple)
            return 0;

        result += cc_el_size_type(type);
    }

    return result;
}

size_t cc_t_sizeof_instance(HTuple tuple) {

}

#define CC_TUPLE_BUFFER(tuple) ((HElementData) (tuple)->buf)
#define CC_INSTANCES_POINTER(tuple) ((ContainerElementType *), )

int cc_t_init_at(void *buf, size_t buffer_size, size_t args, ...)
{

}
#endif

#ifdef __cplusplus
}
#endif
