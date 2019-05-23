#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include "io.h"
#include "utility.h"

#include <stddef.h>
#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <math.h> /* For printf and scanf */
#include <float.h>

#if LINUX_OS
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#elif WINDOWS_OS
#include <windows.h>
#endif

struct IO_sizes {
    void *ptr2;
    size_t size, pos, capacity;
};

struct InputOutputDevice {
    /*
     * `ptr` stores the following:
     *
     *      type == IO_File: FILE *
     *      type == IO_NativeFile: Linux - file descriptor number, as integer, Windows - File handle
     *      type == IO_CString: const char *
     *      type == IO_SizedBuffer: char * (not edited if opened as read-only)
     *      type == IO_MinimalBuffer: char * pointing to dynamically allocated string
     *      type == IO_DynamicBuffer: char * pointing to dynamically allocated string
     *      type == IO_Custom: void * (pointing to userdata)
     *
     * `ptr2` stores the following:
     *
     *      type == IO_File: undefined
     *      type == IO_NativeFile: Pointer to read/write buffer
     *      type == IO_CString: undefined
     *      type == IO_SizedBuffer: undefined
     *      type == IO_MinimalBuffer: undefined
     *      type == IO_DynamicBuffer: undefined
     *      type == IO_Custom: undefined
     *
     * `size` stores the following:
     *
     *      type == IO_File: undefined
     *      type == IO_NativeFile: Size of read/write buffer
     *      type == IO_CString: size of C-style string, not including terminating NUL
     *      type == IO_SizedBuffer: size of buffer
     *      type == IO_MinimalBuffer: size of data and buffer
     *      type == IO_DynamicBuffer: size of data in buffer
     *      type == IO_Custom: undefined
     *
     * `pos` stores the following:
     *
     *      type == IO_File: undefined
     *      type == IO_NativeFile: Number of bytes in buffer
     *      type == IO_CString: current read position in string
     *      type == IO_SizedBuffer: current read/write position in string
     *      type == IO_MinimalBuffer: current read/write position in buffer
     *      type == IO_DynamicBuffer: current read/write position in buffer
     *      type == IO_Custom: undefined
     *
     * `capacity` stores the following:
     *
     *      type == IO_DynamicBuffer: capacity of buffer
     *      all others: undefined
     *
     * `ungetAvail` stores the number of characters available to read (backwards) from `ungetBuf`
     *  (the last character in the array is the first to be read)
     *
     */
    void *ptr;
    const struct InputOutputDeviceCallbacks *callbacks;

    union {
        struct IO_sizes sizes;
        unsigned char raw[sizeof(struct IO_sizes)];
    } data;
    enum IO_Type type;

    unsigned long flags; /* Upper 8 bits are last-written character */

    unsigned ungetAvail;
    unsigned char ungetBuf[4];
};

/* Whether IO device is readable or not */
#define IO_FLAG_READABLE ((unsigned) 0x01)
/* Whether IO device is writable or not */
#define IO_FLAG_WRITABLE ((unsigned) 0x02)
/* Whether IO device is opened for update or not */
#define IO_FLAG_UPDATE ((unsigned) 0x04)
/* Whether IO device is opened for append or not */
#define IO_FLAG_APPEND ((unsigned) 0x08)
/* Whether IO device had an error */
#define IO_FLAG_ERROR ((unsigned) 0x10)
/* Whether IO device reached the end of input */
#define IO_FLAG_EOF ((unsigned) 0x20)
/* Whether IO device should fail if the file exists already */
#define IO_FLAG_FAIL_IF_EXISTS ((unsigned) 0x40)
/* Whether IO device is in use (used for static-storage IO device allocation) */
#define IO_FLAG_IN_USE ((unsigned) 0x100)
/* Whether IO device is dynamically allocated and should be freed */
#define IO_FLAG_DYNAMIC ((unsigned) 0x200)
/* Whether IO device owns its vbuf */
#define IO_FLAG_OWNS_BUFFER ((unsigned) 0x400)
/* Whether IO device was just read from */
#define IO_FLAG_HAS_JUST_READ ((unsigned) 0x800)
/* Whether IO device was just written to */
#define IO_FLAG_HAS_JUST_WRITTEN ((unsigned) 0x1000)
/* Whether IO device is opened for text or binary */
#define IO_FLAG_BINARY ((unsigned) 0x2000)

#define IO_FLAG_RESET (IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_UPDATE | IO_FLAG_APPEND | IO_FLAG_ERROR | IO_FLAG_EOF | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN | IO_FLAG_BINARY)

/* **WARNING** - Only define CC_IO_STATIC_INSTANCES if you don't need thread safety */
#ifdef CC_IO_STATIC_INSTANCES
#if CC_IO_STATIC_INSTANCES > 0
#define CC_IO_HAS_STATIC_INSTANCES
#endif
#endif

#ifdef CC_IO_HAS_STATIC_INSTANCES
static struct InputOutputDevice io_devices[CC_IO_STATIC_INSTANCES];

static enum IO_OpenHint io_device_open_hint;
static enum IO_OpenHint io_device_open_permanent_hint;

static Atomic io_static_alloc_lock; /* 1 if lock is held, 0 otherwise */

static IO io_static_alloc(enum IO_Type type) {
    IO io = NULL;

    /* Spinlock until lock is 0 */
    while (atomic_cmpxchg(&io_static_alloc_lock, 1, 0) != 0);

    for (int i = 0; i < CC_IO_STATIC_INSTANCES; ++i) {
        if ((io_devices[i].flags & IO_FLAG_IN_USE) == 0) {
            io = &io_devices[i];
            break;
        }
    }

    if (io == NULL)
        return NULL;

    io->type = type;
    io->flags = IO_FLAG_IN_USE;
    io->callbacks = NULL;
    io->ungetAvail = 0;

    io_hint_next_open(io_device_open_permanent_hint, 0);

    atomic_set(&io_static_alloc_lock, 0);

    return io;
}

void io_hint_next_open(enum IO_OpenHint hint, int permanentHint) {
    io_device_open_hint = hint;
    if (permanentHint)
        io_device_open_permanent_hint = hint;
}

static enum IO_OpenHint io_permanent_open_hint() {
    return io_device_open_permanent_hint;
}

static enum IO_OpenHint io_open_hint_for_next_open() {
    return io_device_open_hint;
}
#else
void io_hint_next_open(enum IO_OpenHint hint, int permanentHint) {
    UNUSED(hint)
    UNUSED(permanentHint)
}

static enum IO_OpenHint io_permanent_open_hint() {
    return IO_HintDynamic;
}

static enum IO_OpenHint io_open_hint_for_next_open() {
    return IO_HintDynamic;
}
#endif

static IO io_alloc(enum IO_Type type) {
    IO io;

#ifdef CC_IO_HAS_STATIC_INSTANCES
    if (io_open_hint_for_next_open() == IO_HintStatic) {
        io = io_static_alloc(type);
        if (io != NULL)
            return io;
    }
#endif

    io = malloc(sizeof(struct InputOutputDevice));
    if (io == NULL)
        return NULL;

    io->type = type;
    io->flags = IO_FLAG_IN_USE | IO_FLAG_DYNAMIC;
    io->callbacks = NULL;
    io->ungetAvail = 0;

    io_hint_next_open(io_permanent_open_hint(), 0);

    return io;
}

static void io_destroy(IO io) {
    if (io->flags & IO_FLAG_OWNS_BUFFER) {
        free(io->data.sizes.ptr2);
        io->flags &= ~IO_FLAG_OWNS_BUFFER;
    }

    if (io->flags & IO_FLAG_DYNAMIC)
        free(io);
    else
        io->flags = 0;
}

/* Attempts to grow the dynamic buffer stored in `io` to at least `size` capacity */
/* Returns 0 on success, EOF on failure */
static int io_grow(IO io, size_t size) {
    size_t growth;

    if (size <= io->data.sizes.capacity)
        return 0;

    if (io->type == IO_MinimalBuffer)
        growth = size;
    else {
        growth = io->data.sizes.capacity + (io->data.sizes.capacity >> 1);
        if (growth < size)
            growth = size;
        if (growth < 16)
            growth = 16;
    }

    char *new_data = NULL;
    while (1) {
        new_data = realloc(io->ptr, growth);

        if (new_data != NULL) /* Success! */
            break;
        else if (growth == size) /* Not successful and cannot allocate even required size */
            return EOF;

        growth = size;
    }

    io->ptr = new_data;
    io->data.sizes.capacity = growth;

    return 0;
}

/* TODO: slate for removal? */
static int io_current_char(IO io) {
    switch (io->type) {
        default: return EOF;
        case IO_CString:
        case IO_SizedBuffer: return ((const unsigned char *) io->ptr)[io->data.sizes.pos];
    }
}

/* Gets a character from the unget buffer if available, and removes it, otherwise returns EOF */
static int io_from_unget_buffer(IO io) {
    if (io->ungetAvail == 0)
        return EOF;

    if (io->type != IO_NativeFile && io->type != IO_OwnNativeFile)
        ++io->data.sizes.pos;

    return io->ungetBuf[--io->ungetAvail];
}

static int io_close_without_destroying(IO io) {
    int result = 0;

    switch (io->type) {
        case IO_OwnFile: result = fclose(io->ptr); break;
#if LINUX_OS
        case IO_OwnNativeFile:
            if (io_writable(io))
                result = io_flush(io);

            result = (close((size_t) io->ptr) || result)? EOF: 0;
            break;
#elif WINDOWS_OS
        case IO_OwnNativeFile:
            if (io_writable(io))
                result = io_flush(io);

            result = (CloseHandle(io->ptr) || result)? EOF: 0;
            break;
#endif
        case IO_Custom:
            if (io->callbacks->close == NULL)
                return 0;
            return io->callbacks->close(io->ptr, io);
        default: break;
    }

    return result;
}

static size_t io_read_internal(void *ptr, size_t size, size_t count, IO io);
static size_t io_write_internal(const void *ptr, size_t size, size_t count, IO io);

void io_clearerr(IO io) {
    switch (io->type) {
        default: io->flags &= ~(IO_FLAG_ERROR | IO_FLAG_EOF); break;
        case IO_File:
        case IO_OwnFile: clearerr(io->ptr); break;
    }
}

int io_close(IO io) {
    int result = io_close_without_destroying(io);

    io_destroy(io);

    return result;
}

int io_readable(IO io) {
    return io->flags & IO_FLAG_READABLE;
}

int io_writable(IO io) {
    return io->flags & IO_FLAG_WRITABLE;
}

int io_binary(IO io) {
    return io->flags & IO_FLAG_BINARY;
}

void *io_userdata(IO io) {
    return io->ptr;
}

char *io_underlying_buffer(IO io) {
    return io->type == IO_MinimalBuffer || io->type == IO_DynamicBuffer? io->ptr: NULL;
}

size_t io_underlying_buffer_size(IO io) {
    return io->type == IO_MinimalBuffer || io->type == IO_DynamicBuffer? io->data.sizes.size: 0;
}

size_t io_underlying_buffer_capacity(IO io) {
    return io->type == IO_MinimalBuffer || io->type == IO_DynamicBuffer? io->data.sizes.capacity: 0;
}

unsigned char *io_tempdata(IO io) {
    return io->type != IO_Custom? NULL: io->data.raw;
}

size_t io_tempdata_size(IO io) {
    return io->type != IO_Custom? 0: sizeof(struct IO_sizes);
}

int io_error(IO io) {
    switch (io->type) {
        default: return io->flags & IO_FLAG_ERROR;
        case IO_File:
        case IO_OwnFile: return ferror(io->ptr);
    }
}

int io_eof(IO io) {
    switch (io->type) {
        default: return io->flags & IO_FLAG_ERROR;
        case IO_File:
        case IO_OwnFile: return feof(io->ptr);
    }
}

int io_flush(IO io) {
    switch (io->type) {
        default: return 0;
        case IO_File:
        case IO_OwnFile: return fflush(io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if (io->flags & IO_FLAG_HAS_JUST_WRITTEN) {
                if (io->data.sizes.ptr2 == NULL || io->data.sizes.pos == 0)
                    return 0;

#if LINUX_OS
                if (write((size_t) io->ptr, io->data.sizes.ptr2, io->data.sizes.pos) < (ssize_t) io->data.sizes.pos)
                    return EOF;
#elif WINDOWS_OS
                DWORD written;
                if (!WriteFile(io->ptr, io->data.sizes.ptr2, io->data.sizes.pos, &written, NULL) || written != io->data.sizes.pos)
                    return EOF;
#endif
            } else if (io->flags & IO_FLAG_HAS_JUST_READ) {
                if (io_seek64(io, -((long) io->data.sizes.pos), SEEK_CUR) < 0)
                    return EOF;
            }

            io->data.sizes.pos = 0;
            return 0;
        case IO_Custom:
            if (io->callbacks->flush == NULL)
            {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }
            return io->callbacks->flush(io->ptr, io);
    }
}

int io_copy(IO in, IO out) {
    const size_t size = 256;
    char data[size];
    size_t read = size;

    while (read == size) {
        if ((read = io_read(data, 1, size, in)) != size) {
            if (io_error(in))
                return -1;
        }

        if (io_write(data, 1, read, out) != read)
            return 1;
    }

    return 0;
}

int io_getc_internal(IO io) {
    int ch = io_from_unget_buffer(io);
    if (ch != EOF)
        return ch;

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return EOF;
        case IO_File:
        case IO_OwnFile: return fgetc(io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            char buf;

            if (io_read_internal(&buf, 1, 1, io) != 1)
                return EOF;

            return buf;
        }
        case IO_CString:
        {
            char ch = ((const char *) io->ptr)[io->data.sizes.pos];

            if (ch == 0) {
                io->flags |= IO_FLAG_EOF;
                return EOF;
            }

            io->data.sizes.pos++;

            return (unsigned char) ch;
        }
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        {
            if (io->data.sizes.pos >= io->data.sizes.size) {
                io->flags |= IO_FLAG_EOF;
                return EOF;
            }

            return ((const char *) io->ptr)[io->data.sizes.pos++];
        }
        case IO_Custom:
        {
            unsigned char chr;

            if (io->callbacks->read == NULL ||
                    io->callbacks->read(&chr, 1, 1, io->ptr, io) != 1) {
                io->flags |= IO_FLAG_EOF;
                return EOF;
            }

            return chr;
        }
    }
}

int io_getc(IO io) {
    if (!(io->flags & IO_FLAG_READABLE) || (io->flags & IO_FLAG_HAS_JUST_WRITTEN))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    io->flags |= IO_FLAG_HAS_JUST_READ;

    int ch = io_getc_internal(io), ch2;
    if (!(io->flags & IO_FLAG_BINARY) && (ch == '\r' || ch == '\n')) {
        ch2 = io_getc_internal(io);
        if (ch2 == EOF) {
            io_clearerr(io);
            ch = '\n';
        } else if (ch2 + ch != '\r' + '\n')
            io_ungetc(ch2, io);
        else
            ch = '\n';
    }

    return ch;
}

int io_getpos(IO io, IO_Pos *pos) {
    switch (io->type) {
        default: pos->_pos = 0; return 0;
        case IO_File:
        case IO_OwnFile: return fgetpos(io->ptr, &pos->_fpos);
        case IO_CString:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer: pos->_pos = io->data.sizes.pos; return 0;
        case IO_NativeFile:
        case IO_OwnNativeFile:
        case IO_Custom:
        {
            long long tell = io_tell64(io);
            if (tell < 0)
                return -1;
            pos->_pos = tell;
            return 0;
        }
    }
}

char *io_gets(char *str, int num, IO io) {
    char *oldstr = str;
    int oldnum = num;

    if (!(io->flags & IO_FLAG_READABLE) || (io->flags & IO_FLAG_HAS_JUST_WRITTEN))
    {
        io->flags |= IO_FLAG_ERROR;
        return NULL;
    }

    io->flags |= IO_FLAG_HAS_JUST_READ;

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return NULL;
        case IO_File:
        case IO_OwnFile: return fgets(str, num, io->ptr);
        case IO_CString:
        case IO_SizedBuffer:
        case IO_NativeFile:
        case IO_OwnNativeFile:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        case IO_Custom:
        {
            int ch = 0;

            while (--num > 0 && ch != '\n' && (ch = io_getc(io)) != EOF)
                *str++ = (char) ch;

            if (ch == EOF && num == oldnum - 1)
                return NULL;

            if (oldnum > 0)
                *str = 0;

            return io_error(io)? NULL: oldstr;
        }
    }
}

static unsigned io_flags_for_mode(const char *mode) {
#if defined(IO_DEFAULT_TEXT_MODE)
    unsigned flags = 0;
#elif defined(IO_DEFAULT_BINARY_MODE)
    unsigned flags = IO_FLAG_BINARY;
#endif

    for (; *mode; ++mode) {
        switch (*mode) {
            case 'r': flags |= IO_FLAG_READABLE; break;
            case 'w': flags |= IO_FLAG_WRITABLE; break;
            case '+': flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_UPDATE; break;
            case 'a': flags |= IO_FLAG_APPEND; break;
            case 'x': flags |= IO_FLAG_FAIL_IF_EXISTS; break;
            case 'b': flags |= IO_FLAG_BINARY; break;
            case 't': flags &= ~IO_FLAG_BINARY; break;
        }
    }

    return flags;
}

IO io_open(const char *filename, const char *mode) {
    IO io = io_alloc(IO_OwnFile);
    if (io == NULL)
        return NULL;

    io->ptr = fopen(filename, mode);
    if (io->ptr == NULL) {
        io_close(io);
        return NULL;
    }

    io->flags |= io_flags_for_mode(mode);

    return io;
}

#if LINUX_OS
IO io_open_native(const char *filename, const char *mode) {
    unsigned flags = io_flags_for_mode(mode);
    unsigned openFlags = 0;

    IO io = io_alloc(IO_OwnNativeFile);
    if (io == NULL)
        return NULL;

    io->flags |= flags;

    if ((flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE)) == (IO_FLAG_READABLE | IO_FLAG_WRITABLE))
        openFlags = O_RDWR | O_CREAT;
    else if (flags & IO_FLAG_READABLE)
        openFlags = O_RDONLY;
    else if (flags & IO_FLAG_WRITABLE)
        openFlags = O_WRONLY | O_CREAT;

    if (flags & IO_FLAG_APPEND)
        openFlags |= O_APPEND;
    else if (!(flags & IO_FLAG_UPDATE))
        openFlags |= O_TRUNC;

    if (flags & IO_FLAG_FAIL_IF_EXISTS)
        openFlags |= O_EXCL;

    int descriptor = open(filename, openFlags);
    if (descriptor < 0) {
        io_destroy(io);
        return NULL;
    }

    io->ptr = (void *) (size_t) descriptor;
    io->data.sizes.ptr2 = NULL;
    io->data.sizes.size = io->data.sizes.pos = 0;

    return io;
}
#elif WINDOWS_OS
/* TODO: support UTF-8 encoding instead of Windows code page */
IO io_open_native(const char *filename, const char *mode) {
    unsigned flags = io_flags_for_mode(mode);
    DWORD desiredAccess = 0;
    DWORD createFlag = 0;

    IO io = io_alloc(IO_OwnNativeFile);
    if (io == NULL)
        return NULL;

    io->flags |= flags;

    if (flags & IO_FLAG_READABLE)
        desiredAccess |= GENERIC_READ;

    if (flags & IO_FLAG_WRITABLE)
        desiredAccess |= GENERIC_WRITE;

    if ((flags & IO_FLAG_READABLE) && (flags & IO_FLAG_WRITABLE))
        createFlag = (flags & IO_FLAG_UPDATE)? OPEN_ALWAYS: CREATE_ALWAYS;
    else if (flags & IO_FLAG_READABLE)
        createFlag = OPEN_EXISTING;
    else if (flags & IO_FLAG_WRITABLE)
        createFlag = CREATE_ALWAYS;

    if (flags & IO_FLAG_FAIL_IF_EXISTS)
        createFlag = CREATE_NEW;

    HANDLE file = CreateFileA(filename,
                              desiredAccess,
                              0,
                              NULL,
                              createFlag,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if (file == INVALID_HANDLE_VALUE) {
        io_destroy(io);
        return NULL;
    }

    io->ptr = file;
    io->data.sizes.ptr2 = NULL;
    io->data.sizes.size = io->data.sizes.pos = 0;

    return io;
}
#endif

IO io_open_file(FILE *file) {
    if (file == NULL)
        return NULL;

    IO io = io_alloc(IO_File);
    if (io == NULL)
        return NULL;

    io->ptr = file;

    /* We really can't determine what mode the file was opened with, so assume read/write */
    io->flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE;

    return io;
}

IO io_open_empty(void) {
    IO io = io_alloc(IO_Empty);
    if (io != NULL)
        io->flags |= IO_FLAG_READABLE;
    return io;
}

IO io_open_cstring(const char *str) {
    IO io = io_alloc(IO_CString);
    if (io == NULL)
        return NULL;

    io->flags |= IO_FLAG_READABLE;
    io->ptr = (char *) str;
    io->data.sizes.pos = 0;

    return io;
}

IO io_open_buffer(char *buf, size_t size, const char *mode) {
    IO io = io_alloc(IO_SizedBuffer);
    if (io == NULL)
        return NULL;

    io->ptr = buf;
    io->data.sizes.size = size;
    io->data.sizes.pos = 0;

    io->flags |= io_flags_for_mode(mode);

    if (0 == (io->flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE))) {
        io_destroy(io);
        return NULL;
    }

    if ((io->flags & IO_FLAG_WRITABLE) && !(io->flags & IO_FLAG_UPDATE))
        memset(buf, 0, size);

    return io;
}

static IO io_open_dynamic_buffer_internal(enum IO_Type type, const char *mode) {
    IO io = io_alloc(type);
    if (io == NULL)
        return NULL;

    io->ptr = NULL;
    io->data.sizes.size = io->data.sizes.pos = 0;

    io->flags |= io_flags_for_mode(mode);

    if (0 == (io->flags & IO_FLAG_WRITABLE)) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

IO io_open_minimal_buffer(const char *mode) {
    return io_open_dynamic_buffer_internal(IO_MinimalBuffer, mode);
}

IO io_open_dynamic_buffer(const char *mode) {
    return io_open_dynamic_buffer_internal(IO_DynamicBuffer, mode);
}

IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode) {
    if (custom == NULL)
        return NULL;

    IO io = io_alloc(IO_Custom);
    if (io == NULL)
        return NULL;

    io->ptr = userdata;
    io->callbacks = custom;

    io->flags |= io_flags_for_mode(mode);

    if (0 == (io->flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE)) ||
            (custom->open != NULL && (io->ptr = custom->open(userdata, io)) == NULL)) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

#define PRINTF_FLAG_MINUS 0x01
#define PRINTF_FLAG_PLUS 0x02
#define PRINTF_FLAG_SPACE 0x04
#define PRINTF_FLAG_ZERO 0x08
#define PRINTF_FLAG_HASH 0x10
#define PRINTF_FLAG_HAS_WIDTH 0x20
#define PRINTF_FLAG_HAS_PRECISION 0x40

#define PRINTF_LEN_NONE 0
#define PRINTF_LEN_HH 1
#define PRINTF_LEN_H 2
#define PRINTF_LEN_L 3
#define PRINTF_LEN_LL 4
#define PRINTF_LEN_J 5
#define PRINTF_LEN_Z 6
#define PRINTF_LEN_T 7
#define PRINTF_LEN_BIG_L 8

static unsigned io_stou(const char *str, const char **update) {
    unsigned ret = 0;
    for (; *str && isdigit((unsigned char) *str); ++str)
        ret = (ret * 10) + (*str - '0');

    if (update != NULL)
        *update = str;

    return ret;
}

#define PRINTF_STATE_ADD_0X 1
#define PRINTF_STATE_INTEGRAL 2
#define PRINTF_STATE_SIGNED 4
#define PRINTF_STATE_NEGATIVE 8
#define PRINTF_STATE_FLOATING_POINT 0x10
#define PRINTF_STATE_NUMERIC (PRINTF_STATE_INTEGRAL | PRINTF_STATE_FLOATING_POINT)
#define PRINTF_STATE_FREE_BUFFER 0x20

#define PRINTF_STATE_ERROR 0x80

struct io_printf_state {
    unsigned char *buffer;
    unsigned char internalBuffer[4 * sizeof(long long)];
    size_t bufferLength;
    unsigned flags;
};

#define PRINTF_D(type, value, flags, prec, len, state)              \
    do {                                                            \
        unsigned char *eptr = (state)->internalBuffer + sizeof((state)->internalBuffer) - 1; \
        unsigned char *ptr = eptr;                                  \
        type oldval = (value), mval = oldval;                       \
        int neg = val < 0;                                          \
                                                                    \
        *ptr = 0;                                                   \
        if (neg) {                                                  \
            while (mval < 0) {                                      \
                *--ptr = '0' - mval % 10;                           \
                mval /= 10;                                         \
            }                                                       \
            state->flags |= PRINTF_STATE_NEGATIVE;                  \
        } else {                                                    \
            while (mval > 0) {                                      \
                *--ptr = '0' + mval % 10;                           \
                mval /= 10;                                         \
            }                                                       \
        }                                                           \
                                                                    \
        (state)->buffer = ptr;                                      \
        (state)->bufferLength = eptr-ptr;                           \
    } while (0)

#define PRINTF_U(type, fmt, value, flags, prec, len, state)         \
    do {                                                            \
        const char *alpha = (fmt) == 'X'? "0123456789ABCDEF": "0123456789abcdef"; \
        unsigned char *eptr = (state)->internalBuffer + sizeof((state)->internalBuffer) - 1; \
        unsigned char *ptr = eptr;                                  \
        type oldval = (value), mval = oldval, base = 10;            \
                                                                    \
        switch (fmt) {                                              \
            case 'o': base = 8; break;                              \
            case 'x':                                               \
            case 'X': base = 16; break;                             \
        }                                                           \
                                                                    \
        *ptr = 0;                                                   \
        while (mval > 0) {                                          \
            *--ptr = alpha[mval % base];                            \
            mval /= base;                                           \
        }                                                           \
                                                                    \
        (state)->buffer = ptr;                                      \
        (state)->bufferLength = eptr-ptr;                           \
    } while (0)

#define LOG10(len) (((len) == PRINTF_LEN_BIG_L)? log10l: log10)
#define CEIL(len) (((len) == PRINTF_LEN_BIG_L)? ceill: ceil)
#define FLOOR(len) (((len) == PRINTF_LEN_BIG_L)? floorl: floor)
#define PRINTF_ABS(len, value) (((len) == PRINTF_LEN_BIG_L)? fabsl(value): fabs(value))

#define PRINTF_F(type, fmt, value, fmt_flags, prec, len, state)     \
    do {                                                            \
        type mval = (value);                                        \
                                                                    \
        if (signbit(mval)) {                                        \
            (state)->flags |= PRINTF_STATE_NEGATIVE;                \
            mval = PRINTF_ABS(len, mval);                           \
        }                                                           \
                                                                    \
        if (isinf(mval)) {                                          \
            (state)->buffer = (unsigned char *) (((fmt) == 'F')? "INFINITY": "infinity"); \
            (state)->bufferLength = 8;                              \
        } else if (isnan(mval)) {                                   \
            (state)->buffer = (unsigned char *) (((fmt) == 'F')? "NAN": "nan"); \
            (state)->bufferLength = 3;                              \
        } else {                                                    \
            if ((len) == PRINTF_LEN_BIG_L)                          \
                io_printf_f_long(mval, fmt_flags, prec, len, state);\
            else                                                    \
                io_printf_f(mval, fmt_flags, prec, len, state);     \
        }                                                           \
    } while (0)

#define SCANF_I(type, value, width, len, io)                        \
    do {                                                            \
        unsigned read = 0;                                          \
        char alpha[] = "0123456789abcdef";                          \
        int chr, neg = 0, base = 10;                                \
                                                                    \
        (value) = 0;                                                \
                                                                    \
        if ((chr = io_getc(io)) == EOF)                             \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc(chr, io); return --read;}    \
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        if (chr == '0') {                                           \
            chr = io_getc(io);                                      \
            if (chr == 'x' || chr == 'X')                           \
                base = 16;                                          \
            else                                                    \
                base = 8;                                           \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            char *str = strchr(alpha, chr);                         \
            if (str == NULL || (str - alpha) >= base) {             \
                io_ungetc(chr, io);                                 \
                break;                                              \
            }                                                       \
                                                                    \
            chr = (str - alpha);                                    \
                                                                    \
            (value) = (value) * base + (neg? -chr: chr);            \
                                                                    \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

#define SCANF_D(type, value, width, len, io)                        \
    do {                                                            \
        unsigned read = 0;                                          \
        int chr, neg = 0;                                           \
                                                                    \
        (value) = 0;                                                \
                                                                    \
        if ((chr = io_getc(io)) == EOF)                             \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc(chr, io); return --read;}    \
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr)) {                                    \
                io_ungetc(chr, io);                                 \
                break;                                              \
            }                                                       \
                                                                    \
            chr -= '0';                                             \
                                                                    \
            (value) = (value) * 10 + (neg? -chr: chr);              \
                                                                    \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

#define SCANF_U(type, value, width, len, io)                        \
    do {                                                            \
        unsigned read = 0;                                          \
        int chr;                                                    \
                                                                    \
        (value) = 0;                                                \
                                                                    \
        if ((chr = io_getc(io)) == EOF)                             \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc(chr, io); return --read;}    \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr)) {                                    \
                io_ungetc(chr, io);                                 \
                break;                                              \
            }                                                       \
                                                                    \
            (value) = (value) * 10 + (chr - '0');                   \
                                                                    \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

#define SCANF_O(type, value, width, len, io)                        \
    do {                                                            \
        unsigned read = 0;                                          \
        int chr, neg = 0;                                           \
                                                                    \
        (value) = 0;                                                \
                                                                    \
        if ((chr = io_getc(io)) == EOF)                             \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc(chr, io); return --read;}    \
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr) || chr == '8' || chr == '9') {        \
                io_ungetc(chr, io);                                 \
                break;                                              \
            }                                                       \
                                                                    \
            chr -= '0';                                             \
                                                                    \
            (value) = (value) * 8 + (neg? -chr: chr);               \
                                                                    \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

#define SCANF_X(type, value, width, len, io)                        \
    do {                                                            \
        unsigned read = 0;                                          \
        char alpha[] = "0123456789abcdef";                          \
        int chr, neg = 0;                                           \
                                                                    \
        (value) = 0;                                                \
                                                                    \
        if ((chr = io_getc(io)) == EOF)                             \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc(chr, io); return --read;}    \
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        if (chr == '0') {                                           \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
            if (chr == 'x' || chr == 'X') {                         \
                chr = io_getc(io);                                  \
                if (++read > width) {io_ungetc(chr, io); return --read;}\
            }                                                       \
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isxdigit(chr)) {                                   \
                io_ungetc(chr, io);                                 \
                break;                                              \
            }                                                       \
                                                                    \
            chr = strchr(alpha, chr) - alpha;                       \
                                                                    \
            (value) = (value) * 16 + (neg? -chr: chr);              \
                                                                    \
            chr = io_getc(io);                                      \
            if (++read > width) {io_ungetc(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

static void io_printf_f_long(long double value, unsigned flags, unsigned prec, unsigned len, struct io_printf_state *state) {
    UNUSED(len)

    size_t mantissa_digits = (value <= 1.0)? 1: floorl(log10l(value)) + 1;
    size_t precision = (flags) & PRINTF_FLAG_HAS_PRECISION? prec: 6;
    size_t decimal_point_len = (precision || (flags & PRINTF_FLAG_HASH))? 1: 0;
    size_t complete_len = mantissa_digits + decimal_point_len + precision;
    size_t allowed = LDBL_DIG + 2;
    unsigned char *mbuf, *mptr;
    double oldval = value;

    if (complete_len > sizeof(state->internalBuffer)) {
        mbuf = state->buffer = malloc(complete_len);
        if (mbuf == NULL) {
            state->flags |= PRINTF_STATE_ERROR;
            return;
        }
        state->flags |= PRINTF_STATE_FREE_BUFFER;
    } else
        mbuf = state->buffer;

    mptr = mbuf + mantissa_digits;
    do {
        *--mptr = '0' + (int) floorl(fmodl(value, 10.0));
        value /= 10.0;
    } while (value >= 1.0);

    if (mantissa_digits > allowed) {
        memset(mbuf + allowed, '0', mantissa_digits - allowed);
        allowed = 0;
    }

    mptr = mbuf + mantissa_digits;
    if (decimal_point_len)
        *mptr++ = '.';

    value = (oldval - floorl(oldval)) + 0.5 / powl(10.0, precision);

    while (precision--) {
        value *= 10.0;
        if (allowed) {
            *mptr++ = '0' + (int) floorl(fmodl(value, 10.0));
            --allowed;
        } else
            *mptr++ = '0';
    }

    state->bufferLength = complete_len;
}

static void io_printf_f(double value, unsigned flags, unsigned prec, unsigned len, struct io_printf_state *state) {
    UNUSED(len)

    size_t mantissa_digits = (value <= 1.0)? 1: floor(log10(value)) + 1;
    size_t precision = (flags) & PRINTF_FLAG_HAS_PRECISION? prec: 6;
    size_t decimal_point_len = (precision || (flags & PRINTF_FLAG_HASH))? 1: 0;
    size_t complete_len = mantissa_digits + decimal_point_len + precision;
    size_t allowed = DBL_DIG + 2;
    unsigned char *mbuf, *mptr;
    double oldval = value;

    if (complete_len > sizeof(state->internalBuffer)) {
        mbuf = state->buffer = malloc(complete_len);
        if (mbuf == NULL) {
            state->flags |= PRINTF_STATE_ERROR;
            return;
        }
        state->flags |= PRINTF_STATE_FREE_BUFFER;
    } else
        mbuf = state->buffer;

    mptr = mbuf + mantissa_digits;
    do {
        *--mptr = '0' + (int) floor(fmod(value, 10.0));
        value /= 10.0;
    } while (value >= 1.0);

    if (mantissa_digits > allowed) {
        memset(mbuf + allowed, '0', mantissa_digits - allowed);
        allowed = 0;
    }

    mptr = mbuf + mantissa_digits;
    if (decimal_point_len)
        *mptr++ = '.';

    value = (oldval - floor(oldval)) + 0.5 / pow(10.0, precision);

    while (precision--) {
        value *= 10.0;
        if (allowed) {
            *mptr++ = '0' + (int) floor(fmod(value, 10.0));
            --allowed;
        } else
            *mptr++ = '0';
    }

    state->bufferLength = complete_len;
}

static int io_printf_signed_int(struct io_printf_state *state, unsigned flags, unsigned prec, unsigned len, va_list *args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            int val = va_arg(*args, int);

            if (len == PRINTF_LEN_H)
                val = (signed char) val;
            else if (len == PRINTF_LEN_HH)
                val = (signed short) val;

            PRINTF_D(int, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            long val = va_arg(*args, long);
            PRINTF_D(long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            long long val = va_arg(*args, long long);
            PRINTF_D(long long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            intmax_t val = va_arg(*args, intmax_t);
            PRINTF_D(intmax_t, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        case PRINTF_LEN_T:
        {
            ptrdiff_t val = va_arg(*args, ptrdiff_t);
            PRINTF_D(ptrdiff_t, val, flags, prec, len, state);
            break;
        }
    }

    return state->bufferLength;
}

static int io_printf_unsigned_int(struct io_printf_state *state, char fmt, unsigned flags, unsigned prec, unsigned len, va_list *args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            unsigned val = va_arg(*args, unsigned int);

            if (len == PRINTF_LEN_H)
                val = (unsigned char) val;
            else if (len == PRINTF_LEN_HH)
                val = (unsigned short) val;

            PRINTF_U(unsigned, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            unsigned long val = va_arg(*args, unsigned long);
            PRINTF_U(unsigned long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            unsigned long long val = va_arg(*args, unsigned long long);
            PRINTF_U(unsigned long long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            uintmax_t val = va_arg(*args, uintmax_t);
            PRINTF_U(uintmax_t, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        case PRINTF_LEN_T:
        {
            size_t val = va_arg(*args, size_t);
            PRINTF_U(size_t, fmt, val, flags, prec, len, state);
            break;
        }
    }

    return state->bufferLength;
}

/* fmt must be one of "diuox", returns UINT_MAX on failure, number of characters read on success */
static unsigned io_scanf_int_no_arg(char fmt, IO io, unsigned width) {
    int dummy;
    UNUSED(dummy)
    switch (fmt) {
        case 'd': SCANF_D(int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'i': SCANF_I(int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'o': SCANF_O(int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'u': SCANF_U(int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'x': SCANF_X(int, dummy, width, PRINTF_LEN_NONE, io); break;
        default: return UINT_MAX;
    }
}

/* fmt must be one of "diuox", returns UINT_MAX on failure, number of characters read on success */
static unsigned io_scanf_int(char fmt, IO io, unsigned width, unsigned len, va_list *args) {
    switch (len) {
        default: return UINT_MAX;
        case PRINTF_LEN_NONE:
        {
            switch (fmt) {
                case 'd': {signed int *val = va_arg(*args, signed int *); SCANF_D(signed int, *val, width, len, io); break;}
                case 'i': {signed int *val = va_arg(*args, signed int *); SCANF_I(signed int, *val, width, len, io); break;}
                case 'o': {unsigned int *val = va_arg(*args, unsigned int *); SCANF_O(unsigned int, *val, width, len, io); break;}
                case 'u': {unsigned int *val = va_arg(*args, unsigned int *); SCANF_U(unsigned int, *val, width, len, io); break;}
                case 'x': {unsigned int *val = va_arg(*args, unsigned int *); SCANF_X(unsigned int, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_H:
        {
            switch (fmt) {
                case 'd': {signed short *val = va_arg(*args, signed short *); SCANF_D(signed short, *val, width, len, io); break;}
                case 'i': {signed short *val = va_arg(*args, signed short *); SCANF_I(signed short, *val, width, len, io); break;}
                case 'o': {unsigned short *val = va_arg(*args, unsigned short *); SCANF_O(unsigned short, *val, width, len, io); break;}
                case 'u': {unsigned short *val = va_arg(*args, unsigned short *); SCANF_U(unsigned short, *val, width, len, io); break;}
                case 'x': {unsigned short *val = va_arg(*args, unsigned short *); SCANF_X(unsigned short, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_HH:
        {
            switch (fmt) {
                case 'd': {signed char *val = va_arg(*args, signed char *); SCANF_D(signed char, *val, width, len, io); break;}
                case 'i': {signed char *val = va_arg(*args, signed char *); SCANF_I(signed char, *val, width, len, io); break;}
                case 'o': {unsigned char *val = va_arg(*args, unsigned char *); SCANF_O(unsigned char, *val, width, len, io); break;}
                case 'u': {unsigned char *val = va_arg(*args, unsigned char *); SCANF_U(unsigned char, *val, width, len, io); break;}
                case 'x': {unsigned char *val = va_arg(*args, unsigned char *); SCANF_X(unsigned char, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_L:
        {
            switch (fmt) {
                case 'd': {signed long *val = va_arg(*args, signed long *); SCANF_D(signed long, *val, width, len, io); break;}
                case 'i': {signed long *val = va_arg(*args, signed long *); SCANF_I(signed long, *val, width, len, io); break;}
                case 'o': {unsigned long *val = va_arg(*args, unsigned long *); SCANF_O(unsigned long, *val, width, len, io); break;}
                case 'u': {unsigned long *val = va_arg(*args, unsigned long *); SCANF_U(unsigned long, *val, width, len, io); break;}
                case 'x': {unsigned long *val = va_arg(*args, unsigned long *); SCANF_X(unsigned long, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_LL:
        {
            switch (fmt) {
                case 'd': {signed long long *val = va_arg(*args, signed long long *); SCANF_D(signed long long, *val, width, len, io); break;}
                case 'i': {signed long long *val = va_arg(*args, signed long long *); SCANF_I(signed long long, *val, width, len, io); break;}
                case 'o': {unsigned long long *val = va_arg(*args, unsigned long long *); SCANF_O(unsigned long long, *val, width, len, io); break;}
                case 'u': {unsigned long long *val = va_arg(*args, unsigned long long *); SCANF_U(unsigned long long, *val, width, len, io); break;}
                case 'x': {unsigned long long *val = va_arg(*args, unsigned long long *); SCANF_X(unsigned long long, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_J:
        {
            switch (fmt) {
                case 'd': {intmax_t *val = va_arg(*args, intmax_t *); SCANF_D(intmax_t, *val, width, len, io); break;}
                case 'i': {intmax_t *val = va_arg(*args, intmax_t *); SCANF_I(intmax_t, *val, width, len, io); break;}
                case 'o': {uintmax_t *val = va_arg(*args, uintmax_t *); SCANF_O(uintmax_t, *val, width, len, io); break;}
                case 'u': {uintmax_t *val = va_arg(*args, uintmax_t *); SCANF_U(uintmax_t, *val, width, len, io); break;}
                case 'x': {uintmax_t *val = va_arg(*args, uintmax_t *); SCANF_X(uintmax_t, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_Z:
        {
            size_t *val = va_arg(*args, size_t *);
            switch (fmt) {
                case 'd': SCANF_D(size_t, *val, width, len, io); break;
                case 'i': SCANF_I(size_t, *val, width, len, io); break;
                case 'o': SCANF_O(size_t, *val, width, len, io); break;
                case 'u': SCANF_U(size_t, *val, width, len, io); break;
                case 'x': SCANF_X(size_t, *val, width, len, io); break;
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_T:
        {
            ptrdiff_t *val = va_arg(*args, ptrdiff_t *);
            switch (fmt) {
                case 'd': SCANF_D(ptrdiff_t, *val, width, len, io); break;
                case 'i': SCANF_I(ptrdiff_t, *val, width, len, io); break;
                case 'o': SCANF_O(ptrdiff_t, *val, width, len, io); break;
                case 'u': SCANF_U(ptrdiff_t, *val, width, len, io); break;
                case 'x': SCANF_X(ptrdiff_t, *val, width, len, io); break;
                default: return UINT_MAX;
            }
            break;
        }
    }

    return UINT_MAX;
}

#define CLEANUP(x) do {result = (x); goto cleanup;} while (0)

/* TODO: handle floating point args for C-style strings and sized buffers */
int io_vprintf(IO io, const char *fmt, va_list args) {
    int result = 0;
    int written = 0;
    unsigned fmt_flags = 0, fmt_width = 0, fmt_prec = 0, fmt_len = 0;

    struct io_printf_state state;

    state.buffer = state.internalBuffer;
    state.bufferLength = 0;
    state.flags = 0;

    if (!(io->flags & IO_FLAG_WRITABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return -1;
    }

    switch (io->type) {
        case IO_File:
        case IO_OwnFile: return vfprintf(io->ptr, fmt, args);
        default:
            for (; *fmt; ++fmt) {
                if (*fmt == '%') {
                    ++fmt;

                    if (*fmt == '%') {
                        if (io_putc('%', io) == EOF)
                            return -2;
                        ++written;
                        continue;
                    }

                    /* read flags */
                    while (1)
                    {
                        switch (*fmt++)
                        {
                            case '-': fmt_flags |= PRINTF_FLAG_MINUS; break;
                            case '+': fmt_flags |= PRINTF_FLAG_PLUS; break;
                            case ' ': fmt_flags |= PRINTF_FLAG_SPACE; break;
                            case '#': fmt_flags |= PRINTF_FLAG_HASH; break;
                            case '0': fmt_flags |= PRINTF_FLAG_ZERO; break;
                            default: --fmt; goto done_with_flags;
                        }
                    }
done_with_flags:

                    /* read minimum field width */
                    if (*fmt == '*') {
                        int width = va_arg(args, int);

                        ++fmt;
                        if (width < 0) {
                            fmt_flags |= PRINTF_FLAG_MINUS;
                            fmt_width = -width;
                        } else
                            fmt_width = width;

                        fmt_flags |= PRINTF_FLAG_HAS_WIDTH;
                    } else {
                        const char *old = fmt;
                        fmt_width = io_stou(fmt, &fmt);
                        if (fmt != old)
                            fmt_flags |= PRINTF_FLAG_HAS_WIDTH;
                    }

                    /* read precision */
                    if (*fmt == '.') {
                        ++fmt;
                        if (*fmt == '*') {
                            int prec = va_arg(args, int);

                            ++fmt;
                            if (prec >= 0) {
                                fmt_prec = prec;
                                fmt_flags |= PRINTF_FLAG_HAS_PRECISION;
                            }
                        } else {
                            const char *old = fmt;
                            fmt_prec = io_stou(fmt, &fmt);
                            if (fmt != old)
                                fmt_flags |= PRINTF_FLAG_HAS_PRECISION;
                        }
                    }

                    /* read length modifier */
                    switch (*fmt) {
                        case 'h':
                            if (fmt[1] == 'h') {
                                ++fmt;
                                fmt_len = PRINTF_LEN_HH;
                            }
                            else
                                fmt_len = PRINTF_LEN_H;
                            break;
                        case 'l':
                            if (fmt[1] == 'l') {
                                ++fmt;
                                fmt_len = PRINTF_LEN_LL;
                            }
                            else
                                fmt_len = PRINTF_LEN_L;
                            break;
                        case 'j': fmt_len = PRINTF_LEN_J; break;
                        case 'z': fmt_len = PRINTF_LEN_Z; break;
                        case 't': fmt_len = PRINTF_LEN_T; break;
                        case 'L': fmt_len = PRINTF_LEN_BIG_L; break;
                        default: --fmt; break;
                    }
                    ++fmt;

                    /* reset state flags to internal buffer */
                    state.flags = 0;
                    state.buffer = state.internalBuffer;

                    /* read format specifier */
                    switch (*fmt) {
                        default: return -2; /* incomplete format specifier */
                        case 'c':
                        {
                            state.internalBuffer[0] = va_arg(args, int);
                            state.bufferLength = 1;
                            break;
                        }
                        case 's':
                        {
                            char *s = va_arg(args, char *);
                            size_t len = strlen(s);

                            if ((fmt_flags & PRINTF_FLAG_HAS_PRECISION) && fmt_prec < len)
                                len = fmt_prec;

                            state.buffer = (unsigned char *) s;
                            state.bufferLength = len;
                            break;
                        }
                        case 'n':
                            switch (fmt_len) {
                                case PRINTF_LEN_NONE: *(va_arg(args, int *)) = written; break;
                                case PRINTF_LEN_H: *(va_arg(args, signed char *)) = (signed char) written; break;
                                case PRINTF_LEN_HH: *(va_arg(args, short *)) = (short) written; break;
                                case PRINTF_LEN_L: *(va_arg(args, long *)) = written; break;
                                case PRINTF_LEN_LL: *(va_arg(args, long long *)) = written; break;
                                case PRINTF_LEN_J: *(va_arg(args, intmax_t *)) = written; break;
                                case PRINTF_LEN_Z:
                                case PRINTF_LEN_T: *(va_arg(args, ptrdiff_t *)) = written; break;
                                default: return -2;
                            }
                            break;
                        case 'd':
                        case 'i':
                            state.flags |= PRINTF_STATE_INTEGRAL | PRINTF_STATE_SIGNED;

                            if (io_printf_signed_int(&state, fmt_flags, fmt_prec, fmt_len, &args) < 0)
                                return -2;

                            if (!(fmt_flags & PRINTF_FLAG_HAS_PRECISION))
                                fmt_prec = 1;
                            break;
                        case 'u':
                        case 'o':
                        case 'x':
                        case 'X':
                            state.flags |= PRINTF_STATE_INTEGRAL;

                            if (io_printf_unsigned_int(&state, *fmt, fmt_flags, fmt_prec, fmt_len, &args) < 0)
                                return -2;

                            if (!(fmt_flags & PRINTF_FLAG_HAS_PRECISION)) {
                                switch (*fmt) {
                                    case 'u':
                                    case 'x':
                                    case 'X': fmt_prec = 1; break;
                                    case 'o':
                                        if (fmt_flags & PRINTF_FLAG_HASH) /* alternative implementation */
                                            fmt_prec = state.bufferLength + 1;
                                        else
                                            fmt_prec = 1;
                                        break;
                                }
                            }

                            if ((fmt_flags & PRINTF_FLAG_HASH) && (*fmt == 'x' || *fmt == 'X') && state.bufferLength > 0)
                                state.flags |= PRINTF_STATE_ADD_0X;

                            break;
                        case 'f':
                        case 'F':
                            state.flags |= PRINTF_STATE_FLOATING_POINT;

                            if (fmt_len == PRINTF_LEN_BIG_L) {
                                long double value = va_arg(args, long double);
                                PRINTF_F(long double, *fmt, value, fmt_flags, fmt_prec, fmt_len, &state);
                            } else {
                                double value = va_arg(args, double);
                                PRINTF_F(double, *fmt, value, fmt_flags, fmt_prec, fmt_len, &state);
                            }

                            if (state.flags & PRINTF_STATE_ERROR)
                                CLEANUP(-1);

                            break;
                        case 'p':
                            state.flags |= PRINTF_STATE_INTEGRAL | PRINTF_STATE_ADD_0X;

                            fmt_flags |= PRINTF_FLAG_HASH | PRINTF_FLAG_HAS_PRECISION;
                            fmt_prec = sizeof(void *) * 2 /* for hex encoding */ * CHAR_BIT / 8;

                            size_t val = (size_t) va_arg(args, void *);
                            PRINTF_U(size_t, 'x', val, fmt_flags, fmt_prec, PRINTF_LEN_NONE, &state);

                            break;
                    }

                    /* calculate addon characters for format */
                    char addonChar = 0;
                    size_t addonCharCount = 0;

                    if (state.flags & PRINTF_STATE_NUMERIC) {
                        if (state.flags & PRINTF_STATE_NEGATIVE) {
                            addonChar = '-';
                            addonCharCount = 1;
                        } else if ((state.flags & PRINTF_STATE_SIGNED)) {
                            if (fmt_flags & PRINTF_FLAG_PLUS) {
                                addonChar = '+';
                                addonCharCount = 1;
                            }
                            else if (fmt_flags & PRINTF_FLAG_SPACE) {
                                addonChar = ' ';
                                addonCharCount = 1;
                            }
                        } else if (state.flags & PRINTF_STATE_ADD_0X)
                            addonCharCount = 2;
                    }

                    /* calculate number of fill characters for field width for format */
                    size_t fillCount = 0;
                    size_t precCount = state.bufferLength;

                    if (((state.flags & PRINTF_STATE_INTEGRAL) ||
                            (fmt_flags & PRINTF_FLAG_HAS_PRECISION)) && (precCount < fmt_prec))
                        precCount = fmt_prec;

                    if ((fmt_flags & PRINTF_FLAG_HAS_WIDTH) && fmt_width > precCount + addonCharCount)
                        fillCount = fmt_width - precCount - addonCharCount;

                    written += fillCount + precCount + addonCharCount;
                    precCount -= state.bufferLength;

                    /* calculate fill character for field */
                    if ((state.flags & PRINTF_STATE_NUMERIC) &&
                            !(fmt_flags & (PRINTF_FLAG_MINUS | PRINTF_FLAG_HAS_PRECISION)) &&
                            (fmt_flags & PRINTF_FLAG_ZERO)) {
                        precCount += fillCount;
                        fillCount = 0;
                    }

                    /* ----- ACTUAL OUTPUT ----- */
                    /* if right aligned, output field fill */
                    if (!(fmt_flags & PRINTF_FLAG_MINUS)) /* right-align field */ {
                        for (; fillCount; --fillCount)
                            if (io_putc(' ', io) == EOF)
                                CLEANUP(-1);
                    }

                    /* add addon characters */
                    if (state.flags & PRINTF_STATE_ADD_0X) {
                        if (io_putc('0', io) == EOF || io_putc(*fmt == 'X'? 'X': 'x', io) == EOF)
                            CLEANUP(-1);
                    } else if (addonChar) {
                        if (io_putc(addonChar, io) == EOF)
                            CLEANUP(-1);
                    }

                    /* output precision fill */
                    for (; precCount; --precCount)
                        if (io_putc('0', io) == EOF)
                            CLEANUP(-1);

                    /* output field itself */
                    if (io_write(state.buffer, 1, state.bufferLength, io) != state.bufferLength)
                        CLEANUP(-1);

                    /* if left aligned, output field fill (this is a NOP if field was right aligned, since fillCount is now 0) */
                    for (; fillCount; --fillCount)
                        if (io_putc(' ', io) == EOF)
                            CLEANUP(-1);

                    if (state.flags & PRINTF_STATE_FREE_BUFFER)
                        free(state.buffer);
                } else if (io_putc(*fmt, io) == EOF)
                    return -1;
                else
                    ++written;
            }

            break;
    }

    return written;

cleanup:
    if (state.flags & PRINTF_STATE_FREE_BUFFER)
        free(state.buffer);

    return result;
}

int io_printf(IO io, const char *fmt, ...) {
    int result;

    va_list args;
    va_start(args, fmt);
    result = io_vprintf(io, fmt, args);
    va_end(args);

    return result;
}

int io_putc_internal(int ch, IO io) {
    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return EOF;
        case IO_File:
        case IO_OwnFile: return fputc(ch, io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        {
            unsigned char chr = ch;

            if (io_write_internal(&chr, 1, 1, io) != 1)
                return EOF;

            return chr;
        }
        case IO_SizedBuffer:
            if (io->data.sizes.pos == io->data.sizes.size) {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }
            ((char *) io->ptr)[io->data.sizes.pos++] = (char) ch;
            return (unsigned char) ch;
        case IO_Custom:
        {
            unsigned char chr = ch;

            if (io->callbacks->write == NULL ||
                    io->callbacks->write(&chr, 1, 1, io->ptr, io) != 1) {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }

            return chr;
        }
    }
}

int io_putc(int ch, IO io) {
    if (!(io->flags & IO_FLAG_WRITABLE) || (io->flags & IO_FLAG_HAS_JUST_READ))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    io->flags |= IO_FLAG_HAS_JUST_WRITTEN;

    if (!(io->flags & IO_FLAG_BINARY) && ch == '\n') {
#if WINDOWS_OS
        if (io_putc_internal('\r', io) == EOF ||
                io_putc_internal('\n', io) == EOF)
            return EOF;
        return ch;
#endif
    }

    return io_putc_internal(ch, io);
}

int io_puts(const char *str, IO io) {
    if (!(io->flags & IO_FLAG_WRITABLE) || (io->flags & IO_FLAG_HAS_JUST_READ))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    io->flags |= IO_FLAG_HAS_JUST_WRITTEN;

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return EOF;
        case IO_File:
        case IO_OwnFile: return fputs(str, io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        case IO_Custom:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        {
            size_t len = strlen(str);
            if (io_write(str, 1, len, io) != len)
                return EOF;

            return 0;
        }
        case IO_SizedBuffer:
        {
            int result = 0;
            size_t len = strlen(str);
            size_t avail = io->data.sizes.size - io->data.sizes.pos;

            if (avail < len) {
                len = avail;
                io->flags |= IO_FLAG_ERROR;
                result = EOF;
            }

            memcpy((char *) io->ptr + io->data.sizes.pos, str, len);

            io->data.sizes.pos += len;
            return result;
        }
    }
}

static size_t io_native_unbuffered_read(void *ptr, size_t size, size_t count, IO io) {
    size_t max = size*count, totalRead = 0;

#if LINUX_OS
    do {
        size_t amount = max > SSIZE_MAX? SSIZE_MAX: max;
        ssize_t amountRead = 0;

        if ((amountRead = read((size_t) io->ptr, ptr, amount)) < (ssize_t) amount) {
            io->flags |= amountRead < 0? IO_FLAG_ERROR: IO_FLAG_EOF;
            return (totalRead + (amountRead < 0? 0: amountRead)) / size;
        }

        ptr = (unsigned char *) ptr + amount;
        totalRead += amount;
        max -= amount;
    } while (max);
#elif WINDOWS_OS
    do {
        DWORD amount = max > 0xffffffffu? 0xffffffffu: max;
        DWORD amountRead = 0;

        if (!ReadFile(io->ptr, ptr, amount, &amountRead, NULL)) {
            io->flags |= IO_FLAG_ERROR;
            return totalRead / size;
        } else if (amountRead < amount) {
            io->flags |= IO_FLAG_EOF;
            return (totalRead + amountRead) / size;
        }

        ptr = (unsigned char *) ptr + amount;
        totalRead += amount;
        max -= amount;
    } while (max);
#endif

    return totalRead / size;
}

static size_t io_read_internal(void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    /* Not readable or not switched over to reading yet */
    if (!(io->flags & IO_FLAG_READABLE) || (io->flags & IO_FLAG_HAS_JUST_WRITTEN)) {
        io->flags |= IO_FLAG_ERROR;
        return 0;
    }

    io->flags |= IO_FLAG_HAS_JUST_READ;

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return 0;
        case IO_File:
        case IO_OwnFile: return fread(ptr, size, count, io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            size_t max = size*count;
            unsigned char *cptr = ptr;

            if (io->data.sizes.ptr2 == NULL) /* No buffering */
                return io_native_unbuffered_read(ptr, size, count, io);
            else if (io->data.sizes.pos >= max) {
                memcpy(ptr, io->data.sizes.ptr2 + io->data.sizes.size - io->data.sizes.pos, max);
                io->data.sizes.pos -= max;
                return count;
            } else { /* Buffered and requested more than buffer holds */
                size_t read = io->data.sizes.pos;
                memcpy(cptr, io->data.sizes.ptr2 + io->data.sizes.size - io->data.sizes.pos, read);

                max -= read;
                cptr += read;
                io->data.sizes.pos = 0;

                /* Pull greater-than-buffer-size chunk directly into output, skipping the buffer entirely */
                if (max >= io->data.sizes.size)
                    return (read + io_native_unbuffered_read(cptr, 1, max, io)) / size;
                else {
                    /* Refill read buffer */
                    if ((io->data.sizes.pos = io_native_unbuffered_read(io->data.sizes.ptr2, 1, io->data.sizes.size, io)) != io->data.sizes.size && io_error(io))
                        return read / size;

                    /* Move front-aligned buffer to end-aligned buffer as needed */
                    if (io->data.sizes.pos != io->data.sizes.size)
                        memmove(io->data.sizes.ptr2 + io->data.sizes.size - io->data.sizes.pos, io->data.sizes.ptr2, io->data.sizes.pos);

                    /* Allow for end of input */
                    if (max > io->data.sizes.pos)
                        max = io->data.sizes.pos;

                    memcpy(cptr, io->data.sizes.ptr2 + io->data.sizes.size - io->data.sizes.pos, max);
                    io->data.sizes.pos -= max;

                    return (read + max) / size;
                }
            }
        }
        case IO_CString:
        {
            unsigned char *cptr = ptr;
            size_t blocks = 0;
            size_t i = io->ungetAvail, max = size*count;
            size_t start_pos = io->data.sizes.pos;

            /* search for 0 character in C-string */
            for (io->data.sizes.pos += io->ungetAvail; i < max && io_current_char(io) > 0; ++i)
                io->data.sizes.pos++;

            /* if i < max, the NUL is in the desired block,
             * so truncate to reading the most complete blocks we can */
            if (i < max)
                io->flags |= IO_FLAG_EOF;
            else if (i > max)
                i = max;

            i -= i % size;
            io->data.sizes.pos = start_pos;
            blocks = i;

            /* then copy the blocks */
            for (; i && io->ungetAvail; --i)
                *cptr++ = io_from_unget_buffer(io);
            memcpy(cptr, (char *) io->ptr + io->data.sizes.pos, i);
            io->data.sizes.pos += i;

            /* return number of blocks read */
            return blocks / size;
        }
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        {
            unsigned char *cptr = ptr;
            size_t max = size*count, blocks = 0;
            size_t avail = io->data.sizes.size - io->data.sizes.pos;

            if (io->data.sizes.pos > io->data.sizes.size)
                avail = 0;

            /* not enough to cover reading the requested blocks */
            if (avail < max)
            {
                io->flags |= IO_FLAG_EOF;

                max = avail - avail % size;
            }

            blocks = max;

            /* then copy the blocks */
            for (; max && io->ungetAvail; --max)
                *cptr++ = io_from_unget_buffer(io);
            memcpy(cptr, (char *) io->ptr + io->data.sizes.pos, max);
            io->data.sizes.pos += max;

            /* return number of blocks read */
            return blocks / size;
        }
        case IO_Custom:
        {
            unsigned char *cptr = ptr;
            size_t max = size*count, blocks = 0, read = 0;

            /* copy the blocks */
            for (; max && io->ungetAvail; --max, ++blocks)
                *cptr++ = io_from_unget_buffer(io);

            if (io->callbacks->read == NULL ||
                    (read = io->callbacks->read(cptr, 1, max, io->ptr, io)) != max) {
                if (read == SIZE_MAX) {
                    read = 0;
                    io->flags |= IO_FLAG_ERROR;
                } else {
                    io->flags |= IO_FLAG_EOF;
                }
            }
            blocks += read;

            /* return number of blocks read */
            return blocks / size;
        }
    }
}

size_t io_read(void *ptr, size_t size, size_t count, IO io) {
    if (io->flags & IO_FLAG_BINARY)
        return io_read_internal(ptr, size, count, io);

    size_t max = size*count;
    unsigned char *cptr = ptr;
    for (; max; --max) {
        int ch = io_getc(io);
        if (ch == EOF)
            break;
        *cptr++ = ch;
    }

    return (size*count - max) / size;
}

IO io_reopen(const char *filename, const char *mode, IO io) {
    io->ungetAvail = 0;

    switch (io->type)
    {
        default:
        {
            io_close_without_destroying(io);

            FILE *f = fopen(filename, mode);

            if (f == NULL)
            {
                io_destroy(io);
                return NULL;
            }

            io->ptr = f;
            io->type = IO_OwnFile;
            io->flags &= IO_FLAG_RESET;

            return io;
        }
        case IO_File:
        case IO_OwnFile:
            io->ptr = freopen(filename, mode, io->ptr);

            if (io->ptr == NULL)
            {
                io_destroy(io);
                return NULL;
            }

            return io;
    }
}

/* TODO: scanf */
int io_vscanf(IO io, const char *fmt, va_list args) {
    int items = 0;
    size_t bytes = 0;

    for (; *fmt; ++fmt) {
        unsigned char chr = *fmt;

        if (chr == '%' && fmt[1] != '%') {
            int discardResult = 0;
            int noWidth = 1;
            unsigned fmt_width = 0;
            unsigned fmt_len = PRINTF_LEN_NONE;

            ++fmt;

            if (*fmt == '*') {
                discardResult = 1;
                ++fmt;
            }

            const char *oldfmt = fmt;
            fmt_width = io_stou(fmt, &fmt);
            if (oldfmt == fmt)
                fmt_width = UINT_MAX - 1;
            else
                noWidth = 0;

            /* read length modifier */
            switch (*fmt) {
                case 'h':
                    if (fmt[1] == 'h') {
                        ++fmt;
                        fmt_len = PRINTF_LEN_HH;
                    }
                    else
                        fmt_len = PRINTF_LEN_H;
                    break;
                case 'l':
                    if (fmt[1] == 'l') {
                        ++fmt;
                        fmt_len = PRINTF_LEN_LL;
                    }
                    else
                        fmt_len = PRINTF_LEN_L;
                    break;
                case 'j': fmt_len = PRINTF_LEN_J; break;
                case 'z': fmt_len = PRINTF_LEN_Z; break;
                case 't': fmt_len = PRINTF_LEN_T; break;
                case 'L': fmt_len = PRINTF_LEN_BIG_L; break;
                default: --fmt; break;
            }
            ++fmt;

            if (*fmt != '[' && *fmt != 'c' && *fmt != 'n') {
                int chr = 0;
                do {
                    chr = io_getc(io);
                } while (chr != EOF && isspace(chr));
                io_ungetc(chr, io);
            }

            switch (*fmt) {
                default: goto cleanup;
                case 'i':
                case 'd':
                case 'u':
                case 'o':
                case 'x': {
                    unsigned result = discardResult? io_scanf_int_no_arg(*fmt, io, fmt_width):
                                                     io_scanf_int(*fmt, io, fmt_width, fmt_len, &args);
                    if (result == UINT_MAX || result == 0) {
                        bytes += result == 0;
                        goto cleanup;
                    }
                    bytes += result;
                    break;
                }
                /* TODO: floating point scanning "fega" */
                case 'c': {
                    if (noWidth)
                        fmt_width = 1;

                    if (discardResult) {
                        for (; fmt_width; --fmt_width) {
                            if (io_getc(io) == EOF)
                                goto cleanup;
                            ++bytes;
                        }
                    } else {
                        char *cptr = va_arg(args, char *);
                        size_t read = 0;

                        if ((read = io_read(cptr, 1, fmt_width, io)) != fmt_width) {
                            goto cleanup;
                        }

                        bytes += read;
                    }

                    break;
                }
                case 's': {
                    char *dst = discardResult? NULL: va_arg(args, char *);

                    for (; fmt_width; --fmt_width) {
                        int chr = io_getc(io);
                        if (chr == EOF || isspace(chr)) {
                            io_ungetc(chr, io);
                            break;
                        }

                        if (dst)
                            *dst++ = (char) chr;

                        ++bytes;
                    }

                    if (dst)
                        *dst = 0;

                    break;
                }
                case '[': {
                    if (noWidth)
                        fmt_width = 1;

                    if (fmt[1] == 0 || fmt[2] == 0)
                        goto cleanup;

                    const char *lastCharInSet = fmt + 2;

                    while (*lastCharInSet && (lastCharInSet[-1] == '-' || (lastCharInSet == fmt + 2 && lastCharInSet[-1] == '^') || *lastCharInSet != ']'))
                        ++lastCharInSet;

                    if (*lastCharInSet != ']') /* Set not concluded properly */
                        goto cleanup;

                    --lastCharInSet;
                    ++fmt;

                    int negateSet = *fmt == '^';
                    if (negateSet) {
                        if (fmt == lastCharInSet) /* Negated set with nothing in it isn't valid */
                            goto cleanup;
                        ++fmt;
                    }

                    const char *oldfmt = fmt;
                    char *cptr = discardResult? NULL: va_arg(args, char *);
                    int match = 0;

                    for (; fmt_width; --fmt_width) {
                        fmt = oldfmt;

                        match = io_getc(io);
                        if (match == EOF)
                            break;

                        /* fmt now points to first char in set and lastCharInSet points to the last char in set */
                        /* They may be pointing to the same char if it's a one-character set */
                        if (fmt == lastCharInSet) {
                            if ((negateSet? match == *fmt: match != *fmt)) {
                                io_ungetc(match, io);
                                break;
                            }
                        } else { /* Complex set, possibly negated */
                            int matched = negateSet; /* If matched is non-zero, the set matches */

                            for (; fmt <= lastCharInSet; ++fmt) {
                                if (fmt[1] == '-') { /* Compute range */
                                    int rangeLow = (unsigned char) fmt[0];
                                    int rangeHigh = (unsigned char) fmt[2];

                                    /* Swap range if backwards */
                                    if (rangeHigh < rangeLow) {
                                        int temp = rangeHigh;
                                        rangeHigh = rangeLow;
                                        rangeLow = temp;
                                    }

                                    if (rangeLow <= match && match <= rangeHigh) {
                                        matched = !negateSet; /* Set to 1 if normal set, and 0 if negated */
                                        break;
                                    }

                                    fmt += 2;
                                } else if (match == *fmt) {
                                    matched = !negateSet;
                                    break;
                                }
                            }

                            if (!matched) {
                                io_ungetc(match, io);
                                break;
                            }
                        }

                        if (cptr)
                            *cptr++ = match;
                    }

                    fmt = lastCharInSet + 1;
                    if (cptr)
                        *cptr = 0;

                    break;
                }
                case 'n': {
                    switch (fmt_len) {
                        case PRINTF_LEN_NONE: {int *n = va_arg(args, int *); *n = bytes; break;}
                        case PRINTF_LEN_HH: {signed char *n = va_arg(args, signed char *); *n = bytes; break;}
                        case PRINTF_LEN_H: {signed short *n = va_arg(args, signed short *); *n = bytes; break;}
                        case PRINTF_LEN_L: {long int *n = va_arg(args, long int *); *n = bytes; break;}
                        case PRINTF_LEN_LL: {long long int *n = va_arg(args, long long int *); *n = bytes; break;}
                        case PRINTF_LEN_J: {intmax_t *n = va_arg(args, intmax_t *); *n = bytes; break;}
                        case PRINTF_LEN_Z: {size_t *n = va_arg(args, size_t *); *n = bytes; break;}
                        case PRINTF_LEN_T: {ptrdiff_t *n = va_arg(args, ptrdiff_t *); *n = bytes; break;}
                    }
                    break;
                }
            }

            ++items;
        } else if (isspace(chr)) {
            /* Read zero or more space characters */
            int charRead;

            do {
                charRead = io_getc(io);
                if (charRead != EOF)
                    ++bytes;
            } while (charRead != EOF && isspace(charRead));

            if (charRead != EOF)
                io_ungetc(charRead, io);

            if (charRead == EOF)
                return bytes == 0? EOF: items;
        } else {
            if (chr == '%')
                ++fmt; /* Skip one of two '%' characters */

            /* Read exact match */
            int charRead = io_getc(io);

            if (charRead == EOF)
                return bytes == 0? EOF: items;
            else if (charRead != chr) {
                io_ungetc(charRead, io);
                return items;
            }

            ++bytes;
        }
    }

cleanup:
    return bytes == 0? EOF: items;
}

int io_scanf(IO io, const char *fmt, ...) {
    int result;

    va_list args;
    va_start(args, fmt);
    result = io_vscanf(io, fmt, args);
    va_end(args);

    return result;
}

int io_seek(IO io, long int offset, int origin) {
    switch (io->type) {
        default: break;
        case IO_File:
        case IO_OwnFile: return fseek(io->ptr, offset, origin); break;
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.sizes.pos;

#if LINUX_OS
            if (lseek((size_t) io->ptr, offset, origin) < 0)
                return -1;
#elif WINDOWS_OS
            if (sizeof(long) == sizeof(LONG)) {
                DWORD val;
                if ((val = SetFilePointer(io->ptr, offset, NULL,
                                             origin == SEEK_SET? FILE_BEGIN:
                                             origin == SEEK_CUR? FILE_CURRENT:
                                             origin == SEEK_END? FILE_END: FILE_BEGIN)) == INVALID_SET_FILE_POINTER)
                    return -1;

                return offset = (long) val;
            } else {
                LARGE_INTEGER li;

                li.QuadPart = offset;

                if ((li.LowPart = SetFilePointer(io->ptr, li.LowPart, &li.HighPart,
                                                 origin == SEEK_SET? FILE_BEGIN:
                                                 origin == SEEK_CUR? FILE_CURRENT:
                                                 origin == SEEK_END? FILE_END: FILE_BEGIN)) == INVALID_SET_FILE_POINTER &&
                        GetLastError() != NO_ERROR)
                    return -1;
            }
#endif

            io->data.sizes.pos = 0;
            break;
        case IO_Custom: {
            long seek;

            if (io->callbacks->seek != NULL)
                seek = io->callbacks->seek(io->ptr, offset, origin, io);
            else if (io->callbacks->seek64 != NULL)
                seek = io->callbacks->seek64(io->ptr, offset, origin, io);
            else
                seek = -1;

            if (seek < 0)
                return seek;
            break;
        }
        case IO_CString:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || (io->data.sizes.pos < (unsigned long) offset && io->data.sizes.pos + strlen(io->ptr) < (unsigned long) offset))
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->data.sizes.pos) || (offset > 0 && strlen(io->ptr) < (unsigned long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                {
                    size_t len = io->data.sizes.pos + strlen(io->ptr);
                    if (offset > 0 || (unsigned long) -offset > len)
                        return -1;
                    io->data.sizes.pos = len + offset;
                    break;
                }
            }
            break;
        }
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.sizes.size < (unsigned long) offset)
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->data.sizes.pos) || (offset > 0 && io->data.sizes.size < (unsigned long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long) -offset > io->data.sizes.size)
                        return -1;
                    io->data.sizes.pos = io->data.sizes.size + offset;
                    break;
            }
            break;
        }
        case IO_DynamicBuffer:
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || SIZE_MAX < (unsigned long) offset)
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->data.sizes.pos) || (offset > 0 && SIZE_MAX - io->data.sizes.pos < (unsigned long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long) -offset > SIZE_MAX - io->data.sizes.size)
                        return -1;
                    io->data.sizes.pos = io->data.sizes.size + offset;
                    break;
            }
            break;
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}

static int io_seek64_internal(IO io, long long int offset, int origin)
{
    switch (io->type) {
        default: return -1LL;
        case IO_Custom:
            if (io->callbacks->seek64 != NULL) {
                int result = io->callbacks->seek64(io->ptr, offset, origin, io);
                if (result)
                    return result;
            }
            else if (offset >= LONG_MIN && offset <= LONG_MAX && io->callbacks->seek) {
                int result = io->callbacks->seek(io->ptr, (long) offset, origin, io);
                if (result)
                    return result;
            }
            else
                return -1;
            break;
        case IO_CString:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || (io->data.sizes.pos < (unsigned long long) offset && io->data.sizes.pos + strlen(io->ptr) < (unsigned long long) offset))
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->data.sizes.pos) || (offset > 0 && strlen(io->ptr) < (unsigned long long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                {
                    size_t len = io->data.sizes.pos + strlen(io->ptr);
                    if (offset > 0 || (unsigned long long) -offset > len)
                        return -1;
                    io->data.sizes.pos = len + offset;
                    break;
                }
            }
            break;
        }
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.sizes.size < (unsigned long long) offset)
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->data.sizes.pos) || (offset > 0 && io->data.sizes.size < (unsigned long long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long long) -offset > io->data.sizes.size)
                        return -1;
                    io->data.sizes.pos = io->data.sizes.size + offset;
                    break;
            }
            break;
        }
        case IO_DynamicBuffer:
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || SIZE_MAX < (unsigned long long) offset)
                        return -1;
                    io->data.sizes.pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->data.sizes.pos) || (offset > 0 && SIZE_MAX - io->data.sizes.pos < (unsigned long long) offset))
                        return -1;
                    io->data.sizes.pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long long) -offset > SIZE_MAX - io->data.sizes.size)
                        return -1;
                    io->data.sizes.pos = io->data.sizes.size + offset;
                    break;
            }
            break;
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}

#if WINDOWS_OS
int io_seek64(IO io, long long int offset, int origin) {
    switch (io->type) {
        default: return io_seek64_internal(io, offset, origin);
        case IO_File:
        case IO_OwnFile: return _fseeki64(io->ptr, offset, origin);
        case IO_NativeFile:
        case IO_OwnNativeFile: {
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.sizes.pos;

            LARGE_INTEGER li;

            li.QuadPart = offset;

            if ((li.LowPart = SetFilePointer(io->ptr, li.LowPart, &li.HighPart,
                                             origin == SEEK_SET? FILE_BEGIN:
                                             origin == SEEK_CUR? FILE_CURRENT:
                                             origin == SEEK_END? FILE_END: FILE_BEGIN)) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                return -1;

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
            io->data.sizes.pos = 0;
            io->ungetAvail = 0;

            return 0;
        }
    }
}
#elif LINUX_OS
int io_seek64(IO io, long long int offset, int origin) {
    switch (io->type) {
        default: return io_seek64_internal(io, offset, origin);
        case IO_File:
        case IO_OwnFile: return fseeko(io->ptr, offset, origin);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.sizes.pos;

            if (lseek64((size_t) io->ptr, offset, origin) < 0)
                return -1;

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
            io->data.sizes.pos = 0;
            io->ungetAvail = 0;

            return 0;
    }
}
#else
int io_seek64(IO io, long long offset, int origin) {
    switch (io->type) {
        default: return io_seek64_internal(io, offset, origin);
        case IO_File:
        case IO_OwnFile:
            if (offset < LONG_MIN || offset > LONG_MAX)
                return -1;
            return io_seek(io, offset, origin);
    }
}
#endif

int io_setpos(IO io, const IO_Pos *pos) {
    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile:
            return fsetpos(io->ptr, &pos->_fpos);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        case IO_Custom:
            return io_seek64(io, pos->_pos, SEEK_SET);
        case IO_CString:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
            io->data.sizes.pos = pos->_pos;
            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
            io->ungetAvail = 0;
            return 0;
    }
}

long int io_tell(IO io) {
    switch (io->type) {
        default: return -1L;
        case IO_File:
        case IO_OwnFile: return ftell(io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
#if LINUX_OS
        {
            long off = lseek((size_t) io->ptr, 0, SEEK_CUR);
            if (off < 0)
                return off;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return off - io->data.sizes.pos;
            else
                return off + io->data.sizes.pos;
        }
#elif WINDOWS_OS
            if (sizeof(long) == sizeof(LONG)) {
                long offset;
                DWORD val;

                if ((val = SetFilePointer(io->ptr, 0, NULL, FILE_CURRENT)) == INVALID_SET_FILE_POINTER)
                    offset = -1;
                else
                    offset = (long) val;

                if (io->flags & IO_FLAG_HAS_JUST_READ)
                    return offset - io->data.sizes.pos;
                else
                    return offset + io->data.sizes.pos;
            } else {
                LARGE_INTEGER li;

                li.QuadPart = 0;

                if ((li.LowPart = SetFilePointer(io->ptr, li.LowPart, &li.HighPart, FILE_CURRENT)) == INVALID_SET_FILE_POINTER &&
                        GetLastError() != NO_ERROR)
                    li.QuadPart = -1;

                if (io->flags & IO_FLAG_HAS_JUST_READ)
                    return li.QuadPart - io->data.sizes.pos;
                else
                    return li.QuadPart + io->data.sizes.pos;
            }
#endif
        case IO_Custom:
            if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1L;
        case IO_CString:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
            return io->data.sizes.pos;
    }
}

#if WINDOWS_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return -1LL;
        case IO_File:
        case IO_OwnFile: return _ftelli64(io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile: {
            LARGE_INTEGER li;

            li.QuadPart = 0;

            if ((li.LowPart = SetFilePointer(io->ptr, li.LowPart, &li.HighPart, FILE_CURRENT)) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                li.QuadPart = -1;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return li.QuadPart - io->data.sizes.pos;
            else
                return li.QuadPart + io->data.sizes.pos;
        }
        case IO_Custom:
            if (io->callbacks->tell64 != NULL)
                return io->callbacks->tell64(io->ptr, io);
            else if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1LL;
        case IO_CString:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
            return io->data.sizes.pos;
    }
}
#elif LINUX_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return -1LL;
        case IO_File:
        case IO_OwnFile: return ftello(io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            long long off = lseek64((size_t) io->ptr, 0, SEEK_CUR);
            if (off < 0)
                return off;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return off - io->data.sizes.pos;
            else
                return off + io->data.sizes.pos;
        }
        case IO_Custom:
            if (io->callbacks->tell64 != NULL)
                return io->callbacks->tell64(io->ptr, io);
            else if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1LL;
        case IO_CString:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
            return io->data.sizes.pos;
    }
}
#else
long long int io_tell64(IO io) {
    if (io->type == IO_Custom) {
        if (io->callbacks->tell64 != NULL)
            return io->callbacks->tell64(io->ptr, io);
        else if (io->callbacks->tell != NULL)
            return io->callbacks->tell(io->ptr, io);
        else
            return -1LL;
    }
    else
        return io_tell(io);
}
#endif

size_t io_native_unbuffered_write(const void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    size_t max = size*count, totalWritten = 0;

#if LINUX_OS
    do {
        size_t amount = max > SSIZE_MAX? SSIZE_MAX: max;
        ssize_t amountWritten = 0;

        if ((amountWritten = write((size_t) io->ptr, ptr, amount)) < (ssize_t) amount) {
            io->flags |= IO_FLAG_ERROR;
            return (totalWritten + (amountWritten < 0? 0: amountWritten)) / size;
        }

        ptr = (unsigned char *) ptr + amount;
        totalWritten += amount;
        max -= amount;
    } while (max);
#elif WINDOWS_OS
    do {
        DWORD amount = max > 0xffffffffu? 0xffffffffu: max;
        DWORD amountWritten = 0;

        if (!WriteFile(io->ptr, ptr, amount, &amountWritten, NULL) || amountWritten != amount) {
            io->flags |= IO_FLAG_ERROR;
            return (totalWritten + amountWritten) / size;
        }

        ptr = (unsigned char *) ptr + amount;
        totalWritten += amount;
        max -= amount;
    } while (max);
#endif

    return totalWritten / size;
}

static size_t io_write_internal(const void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    /* Not writable or not switched over to writing yet */
    if (!(io->flags & IO_FLAG_WRITABLE) || (io->flags & IO_FLAG_HAS_JUST_READ))
    {
        io->flags |= IO_FLAG_ERROR;
        return 0;
    }

    io->flags |= IO_FLAG_HAS_JUST_WRITTEN;

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return 0;
        case IO_File:
        case IO_OwnFile: return fwrite(ptr, size, count, io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            size_t max = size*count;
            const unsigned char *cptr = ptr;

            if (io->flags & IO_FLAG_APPEND) {
                if (io_seek(io, 0, SEEK_END)) {
                    io->flags |= IO_FLAG_ERROR;
                    return 0;
                }
            }

            if (io->data.sizes.ptr2 == NULL) /* No buffering */
                return io_native_unbuffered_write(ptr, size, count, io);
            else if (io->data.sizes.size - io->data.sizes.pos >= max) {
                memcpy(io->data.sizes.ptr2 + io->data.sizes.pos, ptr, max);
                io->data.sizes.pos += max;
                return count;
            } else { /* Buffered and requested output greater than buffer size */
                size_t written = io->data.sizes.size - io->data.sizes.pos;
                memcpy(io->data.sizes.ptr2 + io->data.sizes.pos, cptr, written);

                max -= written;
                cptr += written;

                if (io_native_unbuffered_write(io->data.sizes.ptr2, 1, io->data.sizes.size, io) != io->data.sizes.size)
                    return written / size;

                io->data.sizes.pos = 0;

                /* Push greater-than-buffer-size chunk directly into output, skipping the buffer entirely */
                if (max >= io->data.sizes.size)
                    return (written + io_native_unbuffered_write(cptr, 1, max, io)) / size;
                else {
                    memcpy(io->data.sizes.ptr2, cptr, max);
                    io->data.sizes.pos = max;

                    return count;
                }
            }
        }
        case IO_Custom:
        {
            if (io->callbacks->write == NULL) {
                io->flags |= IO_FLAG_ERROR;
                return 0;
            }

            size_t written = io->callbacks->write(ptr, size, count, io->ptr, io);
            if (written != count)
                io->flags |= IO_FLAG_ERROR;
            return written;
        }
        case IO_SizedBuffer:
        {
            size_t max = size*count;
            size_t avail = io->data.sizes.size - io->data.sizes.pos;

            /* not enough to cover writing the requested blocks */
            if (avail < max)
            {
                io->flags |= IO_FLAG_ERROR;

                max = avail - avail % size;
            }

            /* then copy the blocks */
            memcpy((char *) io->ptr + io->data.sizes.pos, ptr, max);
            io->data.sizes.pos += max;

            /* return number of blocks written */
            return max / size;
        }
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
        {
            if (io->flags & IO_FLAG_APPEND)
                io->data.sizes.pos = io->data.sizes.size;

            size_t max = size*count;
            size_t avail = io->data.sizes.size - io->data.sizes.pos;
            size_t required_size = io->data.sizes.size;
            int grow_with_gap = io->data.sizes.pos > io->data.sizes.size;

            if (grow_with_gap) {
                avail = 0;
                required_size = io->data.sizes.pos + max;
            } else if (max > avail)
                required_size += (max - avail);

            /* grow to desired size */
            if (io_grow(io, required_size)) {
                io->flags |= IO_FLAG_ERROR;

                max = avail - avail % size;
                if (grow_with_gap)
                    required_size = io->data.sizes.size;
            }

            /* then copy the blocks */
            memcpy((char *) io->ptr + io->data.sizes.pos, ptr, max);
            io->data.sizes.pos += max;
            io->data.sizes.size = required_size;

            /* return number of blocks written */
            return max / size;
        }
    }
}

size_t io_write(const void *ptr, size_t size, size_t count, IO io) {
    if (io->flags & IO_FLAG_BINARY)
        return io_write_internal(ptr, size, count, io);

    size_t max = size*count;
    const unsigned char *cptr = ptr;
    for (; max; --max) {
        if (io_putc(*cptr++, io) == EOF)
            break;
    }

    return (size*count - max) / size;
}

void io_rewind(IO io) {
    io->ungetAvail = 0;

    switch (io->type) {
        default: io_seek(io, 0, SEEK_SET); break;
        case IO_File:
        case IO_OwnFile: rewind(io->ptr); break;
        case IO_CString:
        case IO_SizedBuffer:
            io->data.sizes.pos = 0;
            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            break;
    }
}

void io_setbuf(IO io, char *buf) {
    switch (io->type) {
        default: break;
        case IO_File:
        case IO_OwnFile:
            setbuf(io->ptr, buf);
            break;
        case IO_NativeFile:
        case IO_OwnNativeFile:
            io->data.sizes.ptr2 = buf;
            io->data.sizes.size = buf? BUFSIZ: 0;
            io->data.sizes.pos = 0;
            break;
    }
}

int io_setvbuf(IO io, char *buf, int mode, size_t size) {
    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile:
            return setvbuf(io->ptr, buf, mode, size);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if (io->flags & IO_FLAG_OWNS_BUFFER) {
                free(io->data.sizes.ptr2);
                io->flags &= ~IO_FLAG_OWNS_BUFFER;
            }

            if (mode == _IONBF) {
                io->data.sizes.ptr2 = NULL;
                io->data.sizes.size = io->data.sizes.pos = 0;
            } else if (buf == NULL) {
                io->data.sizes.size = io->data.sizes.pos = 0;
                if ((io->data.sizes.ptr2 = malloc(size)) == NULL)
                    return -1;
                io->data.sizes.size = size;
                io->flags |= IO_FLAG_OWNS_BUFFER;
            } else {
                io->data.sizes.ptr2 = buf;
                io->data.sizes.size = size;
                io->data.sizes.pos = 0;
            }

            return 0;
    }
}

IO io_tmpfile(void) {
    IO io = io_alloc(IO_OwnFile);
    if (io == NULL)
        return NULL;

    io->ptr = tmpfile();
    if (io->ptr == NULL) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

int io_ungetc(int chr, IO io) {
    if (!(io->flags & IO_FLAG_READABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    if (chr == EOF)
        return EOF;

    switch (io->type) {
        default: return EOF;
        case IO_File:
        case IO_OwnFile: return ungetc(chr, io->ptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if (io->ungetAvail != sizeof(io->ungetBuf))
            {
                io->flags &= ~IO_FLAG_EOF;

                return io->ungetBuf[io->ungetAvail++] = chr;
            }

            return EOF;
        case IO_CString:
            if (chr == 0)
                return EOF;
            /* fallthrough */
        case IO_Custom:
        case IO_SizedBuffer:
        case IO_MinimalBuffer:
        case IO_DynamicBuffer:
            if (io->ungetAvail != sizeof(io->ungetBuf))
            {
                io->flags &= ~IO_FLAG_EOF;

                --io->data.sizes.pos;
                return io->ungetBuf[io->ungetAvail++] = chr;
            }

            return EOF;
    }
}