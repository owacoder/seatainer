/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CCTUPLE_H
#define CCTUPLE_H

#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* Returns the size of one tuple with specified types */
    size_t cc_t_sizeof(size_t num, ...);

    /* Returns max capacity of one tuple */
    size_t cc_t_max_capacity();


#ifdef __cplusplus
}
#endif

#endif // CCTUPLE_H
