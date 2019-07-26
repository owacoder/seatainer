/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef CCERROR_H
#define CCERROR_H

#include "platforms.h"

#if !WINDOWS_OS
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Cross-platform error constants for IO */
#if WINDOWS_OS
# define CC_E2BIG ERROR_BAD_ARGUMENTS
# define CC_EACCES ERROR_ACCESS_DENIED
# define CC_EADDRINUSE ERROR_ADDRESS_ALREADY_ASSOCIATED
# define CC_EADDRNOTAVAIL ERROR_INCORRECT_ADDRESS /* ? */

# define CC_EBADF ERROR_INVALID_HANDLE
# define CC_EBADMSG ERROR_INVALID_DATA

# define CC_ECONNABORTED ERROR_CONNECTION_ABORTED
# define CC_ECONNREFUSED ERROR_CONNECTION_REFUSED

# define CC_EILSEQ ERROR_INVALID_DATA

# define CC_EINVAL ERROR_BAD_ARGUMENTS
# define CC_EIO ERROR_IO_DEVICE
# define CC_EISCONN ERROR_ALREADY_INITIALIZED

# define CC_ENOBUFS ERROR_DISK_FULL

# define CC_ENOMEM ERROR_OUTOFMEMORY

# define CC_ENOTSUP ERROR_NOT_SUPPORTED

# define CC_EPERM ERROR_ACCESS_DENIED
# define CC_EPIPE ERROR_BAD_DEVICE

# define CC_ESPIPE ERROR_SEEK_ON_DEVICE

# define CC_ETIME ERROR_TIMEOUT
# define CC_ETIMEDOUT ERROR_TIMEOUT

# define CC_EREAD ERROR_READ_FAULT
# define CC_EWRITE ERROR_WRITE_FAULT
#else
# define CC_E2BIG E2BIG
# define CC_EACCES EACCES
# define CC_EADDRINUSE EADDRINUSE
# define CC_EADDRNOTAVAIL EADDRNOTAVAIL
# define CC_EAFNOSUPPORT EAFNOSUPPORT
# define CC_EAGAIN EAGAIN
# define CC_EALREADY EALREADY
# define CC_EBADF EBADF
# define CC_EBADMSG EBADMSG
# define CC_EBUSY EBUSY
# define CC_ECANCELED ECANCELED
# define CC_ECHILD ECHILD
# define CC_ECONNABORTED ECONNABORTED
# define CC_ECONNREFUSED ECONNREFUSED
# define CC_ECONNRESET ECONNRESET
# define CC_EDEADLK EDEADLK
# define CC_EDESTADDRREQ EDESTADDRREQ
# define CC_EDOM EDOM
# define CC_EDQUOT EDQUOT
# define CC_EEXIST EEXIST
# define CC_EFAULT EFAULT
# define CC_EFBIG EFBIG
# define CC_EHOSTUNREACH EHOSTUNREACH
# define CC_EIDRM EIDRM
# define CC_EILSEQ EILSEQ
# define CC_EINPROGRESS EINPROGRESS
# define CC_EINTR EINTR
# define CC_EINVAL EINVAL
# define CC_EIO EIO
# define CC_EISCONN EISCONN
# define CC_EISDIR EISDIR
# define CC_ELOOP ELOOP
# define CC_EMFILE EMFILE
# define CC_EMLINK EMLINK
# define CC_EMSGSIZE EMSGSIZE
# define CC_EMULTIHOP EMULTIHOP
# define CC_ENAMETOOLONG ENAMETOOLONG
# define CC_ENETDOWN ENETDOWN
# define CC_ENETRESET ENETRESET
# define CC_ENETUNREACH ENETUNREACH
# define CC_ENFILE ENFILE
# define CC_ENOBUFS ENOBUFS
# define CC_ENODATA ENODATA
# define CC_ENODEV ENODEV
# define CC_ENOENT ENOENT
# define CC_ENOEXEC ENOEXEC
# define CC_ENOLCK ENOLCK
# define CC_ENOLINK ENOLINK
# define CC_ENOMEM ENOMEM
# define CC_ENOMSG ENOMSG
# define CC_ENOPROTOOPT ENOPROTOOPT
# define CC_ENOSPC ENOSPC
# define CC_ENOSR ENOSR
# define CC_ENOSTR ENOSTR
# define CC_ENOSYS ENOSYS
# define CC_ENOTCONN ENOTCONN
# define CC_ENOTDIR ENOTDIR
# define CC_ENOTEMPTY ENOTEMPTY
# define CC_ENOTRECOVERABLE ENOTRECOVERABLE
# define CC_ENOTSOCK ENOTSOCK
# define CC_ENOTSUP ENOTSUP
# define CC_ENOTTY ENOTTY
# define CC_ENXIO ENXIO
# define CC_EOPNOTSUPP EOPNOTSUPP
# define CC_EOVERFLOW EOVERFLOW
# define CC_EOWNERDEAD EOWNERDEAD
# define CC_EPERM EPERM
# define CC_EPIPE EPIPE
# define CC_EPROTO EPROTO
# define CC_EPROTONOSUPPORT EPROTONOSUPPORT
# define CC_EPROTOTYPE EPROTOTYPE
# define CC_ERANGE ERANGE
# define CC_EROFS EROFS
# define CC_ESPIPE ESPIPE
# define CC_ESRCH ESRCH
# define CC_ESTALE ESTALE
# define CC_ETIME ETIME
# define CC_ETIMEDOUT ETIMEDOUT
# define CC_ETXTBUSY ETXTBUSY
# define CC_EWOULDBLOCK EWOULDBLOCK
# define CC_EXDEV EXDEV

# define CC_EREAD EIO
# define CC_EWRITE EIO
#endif

/** @brief Returns a human-readable description of the error code, or an empty string if an error occured.
 *
 * This function is thread-safe.
 *
 * @param err The error code to get a description of.
 * @return A dynamically-allocated string containing a human-readable description of @p err.
 */
char *error_description_alloc(int err);

/** @brief Returns a human-readable description of the error code, or an empty string if an error occured.
 *
 * This function is not thread-safe.
 *
 * @param err The error code to get a description of.
 * @return A statically-allocated string containing a human-readable description of @p err.
 */
const char *error_description(int err);

#ifdef __cplusplus
}
#endif

#endif // CCERROR_H
