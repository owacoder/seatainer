/** @file
 *
 *  Basic IO services with flexible backends
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h> /* For printf and scanf */
#include <float.h>

#include "io_core.h"

#include "../Containers/common.h"
#include "../Containers/sbuffer.h"
#include "../utility.h"
#include "../seaerror.h"
#include "../container_io.h"

#if LINUX_OS
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#elif WINDOWS_OS
#include <io.h>
#include <windows.h>
#endif

#define IO_COPY_SIZE 256

static int c_isspace(int chr) {
    return strchr(" \n\t\r\v\f", chr) != NULL;
}

/*
 *  HANDLE REGISTERING OF TYPES AND FORMATS FOR SCANF() AND PRINTF()
 */
struct RegisteredType {
    const char *name;
    CommonContainerBase *base;
    struct RegisteredType *next;
};

struct RegisteredFormat {
    const char *name;
    Parser parser;
    Serializer serializer;
    struct RegisteredFormat *next;
};

static Spinlock register_spinlock;
static struct RegisteredType *registered_types;
static struct RegisteredFormat *registered_formats;
static AtomicPointer io_stdin_, io_stdout_, io_stderr_;

/* Called at program exit, clean up registered types and formats */
static void io_at_exit(void) {
    while (registered_types) {
        struct RegisteredType *save = registered_types;

        container_base_destroy_if_dynamic(registered_types->base);

        registered_types = registered_types->next;

        FREE(save);
    }

    while (registered_formats) {
        struct RegisteredFormat *save = registered_formats;

        registered_formats = registered_formats->next;

        FREE(save);
    }

    io_close(io_stdin_);
    io_close(io_stdout_);
    io_close(io_stderr_);
}

static void register_io_funcs(void) {
    static Atomic registered = 0;

    if (!atomic_set(&registered, 1)) {
        atexit(io_at_exit);
    }
}

IO io_get_stdin(void) {
    if (!atomicp_load(&io_stdin_)) {
        register_io_funcs();

#if WINDOWS_OS
        IO new_io;
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE)
            new_io = io_open_file(stdin);
        else
            new_io = io_open_native_file(h, "rt");
#elif LINUX_OS
        IO new_io = io_open_native_file(0, "rb");
#else
        IO new_io = io_open_file(stdin);
#endif

        if (atomicp_cmpxchg(&io_stdin_, new_io, NULL) != NULL)
            io_close(new_io);
    }

    return io_stdin_;
}

IO io_get_stdout(void) {
    if (!atomicp_load(&io_stdout_)) {
        register_io_funcs();

#if WINDOWS_OS
        IO new_io;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE)
            new_io = io_open_file(stdout);
        else
            new_io = io_open_native_file(h, "wt");
#elif LINUX_OS
        IO new_io = io_open_native_file(1, "wb");
#else
        IO new_io = io_open_file(stdout);
#endif
        io_setvbuf(new_io, NULL, _IOFBF, 0xffff);

        if (atomicp_cmpxchg(&io_stdout_, new_io, NULL) != NULL)
            io_close(new_io);
    }

    return io_stdout_;
}

IO io_get_stderr(void) {
    if (!atomicp_load(&io_stderr_)) {
        register_io_funcs();

#if WINDOWS_OS
        IO new_io;
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        if (h == INVALID_HANDLE_VALUE)
            new_io = io_open_file(stderr);
        else
            new_io = io_open_native_file(h, "wt");
#elif LINUX_OS
        IO new_io = io_open_native_file(2, "wb");
#else
        IO new_io = io_open_file(stderr);
#endif
        if (atomicp_cmpxchg(&io_stderr_, new_io, NULL) != NULL)
            io_close(new_io);
    }

    return io_stderr_;
}

int io_register_type(const char *name, CommonContainerBase *base) {
    register_io_funcs();

    if (base == NULL)
        return CC_EINVAL;

    struct RegisteredType *type = MALLOC(sizeof(*type));
    if (type == NULL) {
        FREE(type);
        return CC_ENOMEM;
    }

    type->name = name;
    type->base = base;

    spinlock_lock(&register_spinlock);

    type->next = registered_types;
    registered_types = type;

    spinlock_unlock(&register_spinlock);

    return 0;
}

const CommonContainerBase *io_get_registered_type(const char *name, size_t name_len) {
    const CommonContainerBase *base = NULL;
    spinlock_lock(&register_spinlock);

    struct RegisteredType *type = registered_types;
    while (type) {
        if (strlen(type->name) == name_len && !memcmp(name, type->name, name_len)) {
            base = type->base;
            goto done;
        }

        type = type->next;
    }

done:
    spinlock_unlock(&register_spinlock);
    return base;
}

void io_unregister_type(const char *name) {
    spinlock_lock(&register_spinlock);

    struct RegisteredType *type = registered_types, *last = NULL;
    while (type) {
        if (!strcmp(name, type->name)) {
            if (last == NULL)
                registered_types = type->next;
            else
                last->next = type->next;

            container_base_destroy_if_dynamic(type->base);
            FREE(type);

            goto done;
        }

        last = type;
        type = type->next;
    }

done:
    spinlock_unlock(&register_spinlock);
}

int io_register_format(const char *name, Parser parser, Serializer serializer) {
    register_io_funcs();

    if (parser == NULL && serializer == NULL)
        return CC_EINVAL;

    struct RegisteredFormat *format = MALLOC(sizeof(*format));
    if (format == NULL)
        return CC_ENOMEM;

    format->name = name;
    format->parser = parser;
    format->serializer = serializer;

    spinlock_lock(&register_spinlock);

    format->next = registered_formats;
    registered_formats = format;

    spinlock_unlock(&register_spinlock);

    return 0;
}

static struct RegisteredFormat *io_get_registered_format(const char *name, size_t name_len) {
    struct RegisteredFormat *found = NULL;
    spinlock_lock(&register_spinlock);

    struct RegisteredFormat *format = registered_formats;
    while (format) {
        if (strlen(format->name) == name_len && !memcmp(name, format->name, name_len)) {
            found = format;
            goto done;
        }

        format = format->next;
    }

done:
    spinlock_unlock(&register_spinlock);
    return found;
}

void io_unregister_format(const char *name) {
    spinlock_lock(&register_spinlock);

    struct RegisteredFormat *format = registered_formats, *last = NULL;
    while (format) {
        if (!strcmp(name, format->name)) {
            if (last == NULL)
                registered_formats = format->next;
            else
                last->next = format->next;

            FREE(format);

            goto done;
        }

        last = format;
        format = format->next;
    }

done:
    spinlock_unlock(&register_spinlock);
}

/*
 *  NORMAL I/O DEFINITION AND OPERATION
 */
struct IO_sizes {
    void *ptr2;
    size_t size, pos, capacity;
};

static size_t io_native_unbuffered_read(void *ptr, size_t size, size_t count, IO io);
static size_t io_native_unbuffered_write(const void *ptr, size_t size, size_t count, IO io);

struct IOFileState {
    FILE *fptr;
};

struct IONativeFileState {
    IONativeFileHandle native;
    unsigned char *buffer; /* Just a temporary buffer that buffers read or write input. When reading, this buffer is always right-aligned, when writing, it's always left-aligned */
    size_t buffer_size;
    size_t buffer_bytes; /* Number of bytes in buffer waiting to be read or written */
};

struct IOSizedBufferState {
    unsigned char *buffer; /* Buffer that doesn't ever get reallocated */
    size_t buffer_size;
    size_t buffer_pos;
};

struct IODynamicBufferState {
    unsigned char *buffer; /* Buffer that can grow past the stored capacity */
    size_t buffer_size;
    size_t buffer_capacity;
    size_t buffer_pos;
};

struct IOThreadBufferState {
    Mutex mutex;
    unsigned char *buffer; /* Circular buffer for efficient thread communication */
    size_t buffer_pos; /* Pointer to first character in buffer */
    size_t buffer_endpos; /* Pointer to one-after the last character in buffer. If equal to buffer_pos, buffer is empty */
    size_t buffer_capacity;
};

struct IOCustomState {
    void *ptr;
    const struct InputOutputDeviceCallbacks *callbacks;
    unsigned char tempdata[3 * sizeof(void *)];
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
     *      type == IO_MinimalBuffer: Equal to `ptr` if pointer can be freed, or NULL if ptr was grabbed by user
     *      type == IO_DynamicBuffer: Equal to `ptr` if pointer can be freed, or NULL if ptr was grabbed by user
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
    union {
        struct IOFileState file;
        struct IONativeFileState native_file;
        struct IOSizedBufferState sized_buffer;
        struct IODynamicBufferState dynamic_buffer;
        struct IOThreadBufferState thread_buffer;
        struct IOCustomState custom;
    } data;

    /** Stores the read timeout assigned to this IO device. Read timeouts are only relevant for sockets, or native file devices on Linux */
    long long read_timeout;
    long long write_timeout;

    unsigned long flags;

    enum IO_Type type;
    /** Stores the last platform-specific error code that occured while reading or writing */
    int error;

    unsigned char ungetAvail;
    unsigned char ungetBuf[15];
};

#ifdef CC_IO_STATIC_INSTANCES
#if CC_IO_STATIC_INSTANCES > 0
#define CC_IO_HAS_STATIC_INSTANCES
#endif
#endif

#ifdef CC_IO_HAS_STATIC_INSTANCES
THREAD_STATIC struct InputOutputDevice io_devices[CC_IO_STATIC_INSTANCES];

THREAD_STATIC enum IO_OpenHint io_device_open_hint;
THREAD_STATIC enum IO_OpenHint io_device_open_permanent_hint;

THREAD_STATIC Spinlock io_static_alloc_lock; /* 1 if lock is held, 0 otherwise */

static IO io_static_alloc(enum IO_Type type) {
    IO io = NULL;

    spinlock_lock(&io_static_alloc_lock);

    for (int i = 0; i < CC_IO_STATIC_INSTANCES; ++i) {
        if ((io_devices[i].flags & IO_FLAG_IN_USE) == 0) {
            io = &io_devices[i];
            break;
        }
    }

    if (io == NULL) {
        spinlock_unlock(&io_static_alloc_lock);
        return NULL;
    }

    io->type = type;
    io->flags = IO_FLAG_IN_USE;
    io->ungetAvail = 0;
    io->read_timeout = io->write_timeout = 0;
    memset(&io->data, 0, sizeof(io->data));

    io_hint_next_open(io_device_open_permanent_hint, 0);

    spinlock_unlock(&io_static_alloc_lock);

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
static IO io_static_alloc(enum IO_Type type) {
    UNUSED(type)
    return NULL;
}

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

    if (io_open_hint_for_next_open() == IO_HintStatic) {
        io = io_static_alloc(type);
        if (io != NULL)
            return io;
    }

    io = CALLOC(1, sizeof(struct InputOutputDevice));
    if (io == NULL)
        return NULL;

    io->type = type;
    io->flags = IO_FLAG_IN_USE | IO_FLAG_DYNAMIC;

    io_hint_next_open(io_permanent_open_hint(), 0);

    return io;
}

static void io_destroy(IO io) {
    if (io == NULL)
        return;

    if (io->flags & IO_FLAG_OWNS_BUFFER) {
        switch (io->type) {
            case IO_NativeFile:
            case IO_OwnNativeFile:
                FREE(io->data.native_file.buffer);
                break;
            case IO_SizedBuffer:
                FREE(io->data.sized_buffer.buffer);
                break;
            case IO_DynamicBuffer:
                FREE(io->data.dynamic_buffer.buffer);
                break;
            case IO_ThreadBuffer:
                FREE(io->data.thread_buffer.buffer);
                mutex_destroy(io->data.thread_buffer.mutex);
                break;
            default: break;
        }

        io->flags &= ~IO_FLAG_OWNS_BUFFER;
    }

    if (io->flags & IO_FLAG_DYNAMIC)
        FREE(io);
    else
        io->flags = 0;
}

/* Attempts to grow the dynamic buffer stored in `io` to at least `size` capacity */
/* Returns 0 on success, EOF on failure */
static int io_grow_dynamic(IO io, size_t size) {
    size_t growth;

    if (size <= io->data.dynamic_buffer.buffer_capacity)
        return 0;

    growth = io->data.dynamic_buffer.buffer_capacity + (io->data.dynamic_buffer.buffer_capacity >> 1);
    if (growth < size)
        growth = size;
    if (growth < 16)
        growth = 16;

    unsigned char *new_data = NULL;
    while (1) {
        new_data = REALLOC(io->data.dynamic_buffer.buffer, growth);

        if (new_data != NULL) /* Success! */
            break;
        else if (growth == size) /* Not successful and cannot allocate even required size */
            return EOF;

        growth = size;
    }

    io->data.dynamic_buffer.buffer = new_data;
    io->data.dynamic_buffer.buffer_capacity = growth;

    return 0;
}

static int io_begin_read(IO io) {
    if (((io->flags & IO_FLAG_SUPPORTS_NO_STATE_SWITCH? (IO_FLAG_READABLE | IO_FLAG_ERROR):
                                                   (IO_FLAG_READABLE | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_WRITTEN)) & io->flags) != IO_FLAG_READABLE)
    {
        io->flags |= IO_FLAG_ERROR;
        return io->error = CC_EREAD;
    }

    io->flags |= IO_FLAG_HAS_JUST_READ;

    return 0;
}

static int io_begin_write(IO io) {
    if (((io->flags & IO_FLAG_SUPPORTS_NO_STATE_SWITCH? (IO_FLAG_WRITABLE | IO_FLAG_ERROR):
                                                   (IO_FLAG_WRITABLE | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ)) & io->flags) != IO_FLAG_WRITABLE)
    {
        io->flags |= IO_FLAG_ERROR;
        return io->error = CC_EWRITE;
    }

    io->flags |= IO_FLAG_HAS_JUST_WRITTEN;

    return 0;
}

static void io_lock(IO io) {
    if (io->type == IO_ThreadBuffer)
        mutex_lock(io->data.thread_buffer.mutex);
}

static void io_unlock(IO io) {
    if (io->type == IO_ThreadBuffer)
        mutex_unlock(io->data.thread_buffer.mutex);
}

static int io_unlocki(IO io, int passthrough) {
    io_unlock(io);
    return passthrough;
}

static unsigned long io_unlockul(IO io, unsigned long passthrough) {
    io_unlock(io);
    return passthrough;
}

static size_t io_unlockz(IO io, size_t passthrough) {
    io_unlock(io);
    return passthrough;
}

static void *io_unlockp(IO io, void *passthrough) {
    io_unlock(io);
    return passthrough;
}

static long long io_unlockll(IO io, long long passthrough) {
    io_unlock(io);
    return passthrough;
}

/* Gets a character from the unget buffer if available, and removes it, otherwise returns EOF */
static int io_from_unget_buffer(IO io) {
    if (io->ungetAvail == 0)
        return EOF;

    switch (io->type) {
        case IO_Empty: return EOF;
        case IO_File: break;
        case IO_OwnFile: break;
        case IO_NativeFile: break;
        case IO_OwnNativeFile: break;
        case IO_SizedBuffer: ++io->data.sized_buffer.buffer_pos;
        case IO_ThreadBuffer: break;
        case IO_DynamicBuffer: ++io->data.dynamic_buffer.buffer_pos;
        case IO_Custom: break;
    }

    return io->ungetBuf[--io->ungetAvail];
}

static int io_close_without_destroying(IO io) {
    int result = 0;

    switch (io->type) {
        case IO_OwnFile: result = fclose(io->data.file.fptr); break;
#if LINUX_OS
        case IO_OwnNativeFile:
            if (io_writable(io))
                result = io_flush(io);

            result = (close(io->data.native_file.native) || result)? EOF: 0;
            break;
#elif WINDOWS_OS
        case IO_OwnNativeFile:
            if (io->flags & IO_FLAG_WRITABLE)
                result = io_flush(io);

            result = (CloseHandle(io->data.native_file.native) || result)? EOF: 0;
            break;
#endif
        case IO_SizedBuffer:
            if (io->flags & IO_FLAG_OWNS_BUFFER)
                FREE(io->data.sized_buffer.buffer);
            break;
        case IO_ThreadBuffer:
            if (io->flags & IO_FLAG_OWNS_BUFFER)
                FREE(io->data.thread_buffer.buffer);
            break;
        case IO_DynamicBuffer:
            if (io->flags & IO_FLAG_OWNS_BUFFER)
                FREE(io->data.dynamic_buffer.buffer);
            break;
        case IO_Custom:
            if (io->data.custom.callbacks->close == NULL)
                return 0;
            return io->data.custom.callbacks->close(io->data.custom.ptr, io);
        default: break;
    }

    return result;
}

static int io_getc_internal(IO io);
static int io_putc_internal(int ch, IO io);
static size_t io_read_internal(void *ptr, size_t size, size_t count, IO io);
static size_t io_write_internal(const void *ptr, size_t size, size_t count, IO io);

static int io_ungetc_internal(int chr, IO io) {
    if (!(io->flags & IO_FLAG_READABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        io->error = CC_EREAD;
        return EOF;
    }

    if (chr == EOF)
        return EOF;

    switch (io->type) {
        case IO_Empty: return EOF;
        case IO_File:
        case IO_OwnFile: return ungetc(chr, io->data.file.fptr);
        case IO_SizedBuffer: if (io->ungetAvail != sizeof(io->ungetBuf)) --io->data.sized_buffer.buffer_pos; break;
        case IO_DynamicBuffer: if (io->ungetAvail != sizeof(io->ungetBuf)) --io->data.dynamic_buffer.buffer_pos; break;
        default: break;
    }

    if (io->ungetAvail != sizeof(io->ungetBuf))
    {
        io->flags &= ~IO_FLAG_EOF;

        return io->ungetBuf[io->ungetAvail++] = chr;
    }

    return EOF;
}

int io_ungetc(int chr, IO io) {
    io_lock(io);
    return io_unlocki(io, io_ungetc_internal(chr, io));
}

static void io_clearerr_internal(IO io) {
    switch (io->type) {
        case IO_Custom:
            if (io->data.custom.callbacks->clearerr != NULL)
                io->data.custom.callbacks->clearerr(io->data.custom.ptr, io);
            /* fallthrough */
        default: break;
        case IO_File:
        case IO_OwnFile: clearerr(io->data.file.fptr); break;
    }

    io->flags &= ~(IO_FLAG_ERROR | IO_FLAG_EOF);
}

void io_clearerr(IO io) {
    io_lock(io);
    io_clearerr_internal(io);
    io_unlock(io);
}

int io_close(IO io) {
    if (io == NULL)
        return 0;

    io_lock(io);

    int flush_err = io_flush(io)? io_error(io): 0;
    int result = io_close_without_destroying(io);

    io_unlock(io);
    io_destroy(io);

    return flush_err? flush_err: result;
}

int io_vclose(int count, ...) {
    va_list args;
    va_start(args, count);
    int err = 0;

    while (count-- > 0) {
        IO io = va_arg(args, IO);
        err |= io_close(io) != 0;
    }

    va_end(args);
    return err? EOF: 0;
}

unsigned long io_readable(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_READABLE);
}

unsigned long io_writable(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_WRITABLE);
}

unsigned long io_flags(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags);
}

unsigned long io_just_read(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_HAS_JUST_READ);
}

unsigned long io_just_wrote(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_HAS_JUST_WRITTEN);
}

unsigned long io_opened_for_update(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_UPDATE);
}

unsigned long io_opened_for_append(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_APPEND);
}

unsigned long io_binary(IO io) {
    io_lock(io);
    return io_unlockul(io, io->flags & IO_FLAG_BINARY);
}

unsigned long io_text(IO io) {
    return !io_binary(io);
}

void io_grab_file(IO io) {
    switch (io->type) {
        default: break;
        case IO_File: io->type = IO_OwnFile; break;
        case IO_NativeFile: io->type = IO_OwnNativeFile; break;
    }
}

void io_ungrab_file(IO io) {
    switch (io->type) {
        default: break;
        case IO_OwnFile: io->type = IO_File; break;
        case IO_OwnNativeFile: io->type = IO_NativeFile; break;
    }
}

void *io_userdata(IO io) {
    if (io->type == IO_Custom)
        return io->data.custom.ptr;

    return NULL;
}

void io_grab_underlying_buffer(IO io) {
    switch (io->type) {
        default: break;
        case IO_SizedBuffer:
        case IO_DynamicBuffer:
            io->flags |= IO_FLAG_OWNS_BUFFER;
            break;
    }
}

char *io_take_underlying_buffer(IO io) {
    switch (io->type) {
        default: return NULL;
        case IO_SizedBuffer: io->flags &= ~IO_FLAG_OWNS_BUFFER; return (char *) io->data.sized_buffer.buffer;
        case IO_DynamicBuffer: io->flags &= ~IO_FLAG_OWNS_BUFFER; return (char *) io->data.dynamic_buffer.buffer;
    }
}

char *io_underlying_buffer(IO io) {
    switch (io->type) {
        default: return NULL;
        case IO_SizedBuffer: return (char *) io->data.sized_buffer.buffer;
        case IO_DynamicBuffer: return (char *) io->data.dynamic_buffer.buffer;
    }
}

/* Size of content stored in the circular thread buffer */
static size_t io_thread_buffer_size(IO io) {
    if (io->data.thread_buffer.buffer_pos <= io->data.thread_buffer.buffer_endpos)
        return io->data.thread_buffer.buffer_endpos - io->data.thread_buffer.buffer_pos;
    else /* buffer_pos > buffer_endpos, meaning the unused gap is in the middle or start of the array */
        return io->data.thread_buffer.buffer_capacity - (io->data.thread_buffer.buffer_pos - io->data.thread_buffer.buffer_endpos);
}

/* Total unused space in the circular thread buffer */
static size_t io_thread_buffer_empty_size(IO io) {
    return io->data.thread_buffer.buffer_capacity - io_thread_buffer_size(io);
}

/* Maximum unused contiguous space after buffer_endpos in the circular thread buffer */
static size_t io_thread_buffer_contiguous_empty_at_end(IO io) {
    if (io->data.thread_buffer.buffer_pos <= io->data.thread_buffer.buffer_endpos)
        return io->data.thread_buffer.buffer_capacity - io->data.thread_buffer.buffer_endpos;
    else
        return io->data.thread_buffer.buffer_pos - io->data.thread_buffer.buffer_endpos;
}

/* Maximum used contiguous space after buffer_pos in the circular thread buffer */
static size_t io_thread_buffer_contiguous_stored_at_end(IO io) {
    if (io->data.thread_buffer.buffer_pos <= io->data.thread_buffer.buffer_endpos)
        return io->data.thread_buffer.buffer_endpos - io->data.thread_buffer.buffer_pos;
    else
        return io->data.thread_buffer.buffer_capacity - io->data.thread_buffer.buffer_pos;
}

size_t io_underlying_buffer_size(IO io) {
    switch (io->type) {
        default: return 0;
        case IO_SizedBuffer: return io->data.sized_buffer.buffer_size;
        case IO_ThreadBuffer: io_lock(io); return io_unlockz(io, io_thread_buffer_size(io));
        case IO_DynamicBuffer: return io->data.dynamic_buffer.buffer_size;
    }
}

size_t io_underlying_buffer_capacity(IO io) {
    switch (io->type) {
        default: return 0;
        case IO_SizedBuffer: return io->data.sized_buffer.buffer_size;
        case IO_ThreadBuffer: io_lock(io); return io_unlockz(io, io->data.thread_buffer.buffer_capacity);
        case IO_DynamicBuffer: return io->data.dynamic_buffer.buffer_capacity;
    }
}

/* Attempts to grow the dynamic buffer stored in `io` to at least `size` capacity */
/* Returns 0 on success, EOF on failure */
static int io_grow_threadbuf(IO io, size_t size_of_data_to_append) {
    size_of_data_to_append += 1;

    if (io_thread_buffer_empty_size(io) < size_of_data_to_append) {
        const size_t currently_used = io_thread_buffer_size(io);
        const size_t new_size = MAX(io->data.thread_buffer.buffer_capacity + (io->data.thread_buffer.buffer_capacity >> 1), currently_used + size_of_data_to_append);

        if (io->data.thread_buffer.buffer_pos != 0) { /* If data is not aligned to beginning already */
            unsigned char *new_data = MALLOC(new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            if (io->data.thread_buffer.buffer_pos <= io->data.thread_buffer.buffer_endpos) {
                memcpy(new_data, io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_pos, currently_used);
            } else {
                const size_t offset = io->data.thread_buffer.buffer_capacity - io->data.thread_buffer.buffer_pos;
                memcpy(new_data, io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_pos, offset);
                memcpy(new_data + offset, io->data.thread_buffer.buffer, io->data.thread_buffer.buffer_endpos);
            }

            FREE(io->data.thread_buffer.buffer);
            io->data.thread_buffer.buffer = new_data;
            io->data.thread_buffer.buffer_capacity = new_size;
            io->data.thread_buffer.buffer_pos = 0;
            io->data.thread_buffer.buffer_endpos = currently_used;
        } else { /* Data already aligned to start of buffer */
            unsigned char *new_data = REALLOC(io->data.thread_buffer.buffer, new_size);
            if (new_data == NULL)
                return CC_ENOMEM;

            io->data.thread_buffer.buffer = new_data;
            io->data.thread_buffer.buffer_capacity = new_size;
        }
    }

    return 0;
}

unsigned char *io_tempdata(IO io) {
    return io->type != IO_Custom? NULL: io->data.custom.tempdata;
}

size_t io_tempdata_size(IO io) {
    return io->type != IO_Custom? 0: sizeof(io->data.custom.tempdata);
}

static int io_error_internal(IO io) {
    switch (io->type) {
        default: return io->flags & IO_FLAG_ERROR? io->error: 0;
        case IO_File:
        case IO_OwnFile:
            return io->flags & IO_FLAG_ERROR? io->error: ferror(io->data.file.fptr)? CC_EIO: 0;
    }
}

int io_error(IO io) {
    io_lock(io);
    return io_unlocki(io, io_error_internal(io));
}

static void io_set_error_internal(IO io, int err) {
    if (err)
        io->flags |= IO_FLAG_ERROR;
    else
        io->flags &= ~IO_FLAG_ERROR;

    io->error = err;
}

void io_set_error(IO io, int err) {
    io_lock(io);
    io_set_error_internal(io, err);
    io_unlock(io);
}

static unsigned long io_eof_internal(IO io) {
    switch (io->type) {
        default: return io->flags & IO_FLAG_EOF;
        case IO_File:
        case IO_OwnFile: return (io->flags & IO_FLAG_EOF) || feof(io->data.file.fptr);
    }
}

unsigned long io_eof(IO io) {
    io_lock(io);
    return io_unlockul(io, io_eof_internal(io));
}

int io_flush(IO io) {
    switch (io->type) {
        default: return 0;
        case IO_File:
        case IO_OwnFile: return fflush(io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if (io->flags & IO_FLAG_HAS_JUST_WRITTEN) {
                if (io->data.native_file.buffer_bytes == 0)
                    return 0;

#if LINUX_OS
                if (write(io->data.native_file.native, io->data.native_file.buffer, io->data.native_file.buffer_bytes) < (ssize_t) io->data.native_file.buffer_bytes) {
                    io->flags |= IO_FLAG_ERROR;
                    io->error = errno;
                    return EOF;
                }
#elif WINDOWS_OS
                DWORD written;
                if (!WriteFile(io->data.native_file.native, io->data.native_file.buffer, (DWORD) io->data.native_file.buffer_bytes, &written, NULL) || written != io->data.native_file.buffer_bytes) {
                    io->flags |= IO_FLAG_ERROR;
                    io->error = GetLastError();
                    return EOF;
                }
#endif
            } else if ((io->flags & IO_FLAG_HAS_JUST_READ) && io->data.native_file.buffer_bytes) {
                if (io_seek64(io, -((long long) io->data.native_file.buffer_bytes), SEEK_CUR) < 0)
                    return EOF;
            }

            io->data.native_file.buffer_bytes = 0;
            return 0;
        case IO_Custom:
            if (io->data.custom.callbacks->flush == NULL)
                return 0;

            if (io->data.custom.callbacks->flush(io->data.custom.ptr, io) != 0) {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }

            return 0;
    }
}

int io_resize(IO io, long long int size) {
    io_lock(io);

    if (io_flush(io) != 0)
        return io_unlocki(io, EOF);

    if (size < 0) {
        io->flags |= IO_FLAG_ERROR;
        io->error = CC_EINVAL;
        return io_unlocki(io, EOF);
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; io->error = CC_ENOTSUP; return io_unlocki(io, EOF);
        case IO_File:
        case IO_OwnFile: {
#if LINUX_OS
            if (ftruncate((size_t) io->data.file.fptr, (off_t) size)) {
                io->flags |= IO_FLAG_ERROR;
                io->error = errno;
                return EOF;
            }

            return io_seek64(io, size, SEEK_SET)? EOF: 0;
#elif WINDOWS_OS
            if (io_seek64(io, size, SEEK_SET) != 0)
                return EOF;

            HANDLE osfhandle = (HANDLE) _get_osfhandle(_fileno(io->data.file.fptr));

            if (osfhandle == INVALID_HANDLE_VALUE || !SetEndOfFile(osfhandle)) {
                io->flags |= IO_FLAG_ERROR;
                io->error = GetLastError();
                return EOF;
            }

            return 0;
#else
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_ENOTSUP;
            return EOF; // Resizing not supported with platform-independent code
#endif
        }
        case IO_NativeFile:
        case IO_OwnNativeFile:
#if LINUX_OS
            if (ftruncate(io->data.native_file.native, (off_t) size)) {
                io->flags |= IO_FLAG_ERROR;
                io->error = errno;
                return EOF;
            }

            return io_seek64(io, size, SEEK_SET)? EOF: 0;
#elif WINDOWS_OS
            if (io_seek64(io, size, SEEK_SET) != 0)
                return EOF;

            if (!SetEndOfFile(io->data.native_file.native)) {
                io->flags |= IO_FLAG_ERROR;
                io->error = GetLastError();
                return EOF;
            }

            return 0;
#endif
        case IO_DynamicBuffer:
            if ((uintmax_t) size > SIZE_MAX) {
                io->flags |= IO_FLAG_ERROR;
                io->error = CC_EINVAL;
                return EOF;
            }

            if (io->data.dynamic_buffer.buffer_size > (unsigned long long) size) /* Truncating */ {
                io->data.dynamic_buffer.buffer_size = size;
            } else /* Extending */ {
                if (io_grow_dynamic(io, size)) {
                    io->flags |= IO_FLAG_ERROR;
                    io->error = CC_ENOMEM;
                    return EOF;
                }

                size -= io->data.dynamic_buffer.buffer_size;
                if (io_putc_n(0, (size_t) size, io) == EOF)
                    return EOF;
            }

            return 0;
    }
}

int io_copy_and_close(IO in, IO out) {
    if (!in || !out)
        return CC_EINVAL;

    int result = io_copy(in, out);
    io_close(in);
    io_close(out);
    return result;
}

int io_slow_copy(IO in, IO out) {
    int ch;

    while (1) {
        ch = io_getc(in);
        if (ch != EOF) {
            if (io_putc(ch, out) == EOF)
                return io_error(out);
        } else {
            int err = io_error(in);
            if (err)
                return err;

            break;
        }
    }

    return 0;
}

int io_copy(IO in, IO out) {
    const size_t size = IO_COPY_SIZE;
    char data[IO_COPY_SIZE];
    size_t read = size;
    int err = 0;

    do {
        read = io_read(data, 1, size, in);

        if (read != size && (err = io_error(in))) {
            return err;
        } else if (read && io_write(data, 1, read, out) != read) {
            return io_error(out);
        }
    } while (!io_eof(in));

    return 0;
}

static int io_getc_internal(IO io) {
    int ch = io_from_unget_buffer(io);
    if (ch != EOF)
        return ch;

    switch (io->type) {
        case IO_Empty: io->flags |= IO_FLAG_EOF; return EOF;
        case IO_File:
        case IO_OwnFile: return fgetc(io->data.file.fptr);
        default: {
            unsigned char buf;

            if (io_read_internal(&buf, 1, 1, io) != 1)
                return EOF;

            return buf;
        }
    }
}

int io_getc(IO io) {
    io_lock(io);

    if (io_begin_read(io))
        return io_unlocki(io, EOF);

    return io_unlocki(io, io_getc_internal(io));
}

int io_getpos(IO io, IO_Pos *pos) {
    switch (io->type) {
        case IO_File:
        case IO_OwnFile: return fgetpos(io->data.file.fptr, &pos->_fpos);
        case IO_SizedBuffer: pos->_pos = io->data.sized_buffer.buffer_pos; return 0;
        case IO_DynamicBuffer: pos->_pos = io->data.dynamic_buffer.buffer_pos; return 0;
        default: {
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

    io_lock(io);

    if (io_begin_read(io) || (io->flags & IO_FLAG_EOF))
        return io_unlockp(io, NULL);

    switch (io->type) {
        case IO_Empty: io->flags |= IO_FLAG_EOF; return NULL;
        case IO_File:
        case IO_OwnFile: return fgets(str, num, io->data.file.fptr);
        default:
        {
            int ch = 0;

            while (--num > 0 && ch != '\n' && (ch = io_getc_internal(io)) != EOF)
                *str++ = (char) ch;

            if (ch == EOF && num == oldnum - 1)
                return io_unlockp(io, NULL);

            if (oldnum > 0)
                *str = 0;

            return io_unlockp(io, io_error_internal(io)? NULL: oldstr);
        }
    }
}

static int io_set_flags_for_mode(IO io, const char *mode, unsigned int *flags_) {
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

    io->flags |= flags;
    if (flags_)
        *flags_ = flags;

    return 0;
}

IO io_open(const char *filename, const char *mode) {
    IO io = io_alloc(IO_OwnFile);
    if (io == NULL)
        return NULL;

    io->data.file.fptr = fopen(filename, mode);
    if (io->data.file.fptr == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

#if LINUX_OS
IO io_open_native_file(int descriptor, const char *mode) {
    if (descriptor < 0)
        return NULL;

    IO io = io_alloc(strchr(mode, 'g')? IO_OwnNativeFile: IO_NativeFile);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.native_file.native = descriptor;

    return io;
}

IO io_open_native(const char *filename, const char *mode) {
    unsigned openFlags = 0;
    unsigned flags = 0;

    IO io = io_alloc(IO_OwnNativeFile);
    if (io == NULL || io_set_flags_for_mode(io, mode, &flags)) {
        io_destroy(io);
        return NULL;
    }

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

    io->data.native_file.native = descriptor;

    return io;
}
#elif WINDOWS_OS
IO io_open_native_file(HANDLE descriptor, const char *mode) {
    if (descriptor == INVALID_HANDLE_VALUE)
        return NULL;

    IO io = io_alloc(strchr(mode, 'g')? IO_OwnNativeFile: IO_NativeFile);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.native_file.native = descriptor;

    return io;
}

IO io_open_native(const char *filename, const char *mode) {
    unsigned flags = 0;
    DWORD desiredAccess = 0;
    DWORD createFlag = 0;

    IO io = io_alloc(IO_OwnNativeFile);
    if (io == NULL || io_set_flags_for_mode(io, mode, &flags)) {
        io_destroy(io);
        return NULL;
    }

    if (flags & IO_FLAG_READABLE)
        desiredAccess |= GENERIC_READ;

    if (flags & IO_FLAG_WRITABLE)
        desiredAccess |= GENERIC_WRITE;

    if (flags & IO_FLAG_FAIL_IF_EXISTS)
        createFlag = CREATE_NEW;
    else if ((flags & IO_FLAG_READABLE) && (flags & IO_FLAG_WRITABLE))
        createFlag = (flags & (IO_FLAG_UPDATE | IO_FLAG_APPEND))? OPEN_ALWAYS: CREATE_ALWAYS;
    else if (flags & IO_FLAG_READABLE)
        createFlag = OPEN_EXISTING;
    else if (flags & IO_FLAG_WRITABLE)
        createFlag = CREATE_ALWAYS;

    HANDLE file;

    if (strstr(mode, "@ncp") != NULL || str_is_codepage_safe(filename)) {
        file = CreateFileA(filename,
                           desiredAccess,
                           0,
                           NULL,
                           createFlag,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
    } else {
        LPWSTR wide = utf8_to_wide_alloc(filename);
        if (!wide) {
            io_destroy(io);
            return NULL;
        }

        file = CreateFileW(wide,
                           desiredAccess,
                           0,
                           NULL,
                           createFlag,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);

        FREE(wide);
    }

    if (file == INVALID_HANDLE_VALUE) {
        io_destroy(io);
        return NULL;
    }

    io->data.native_file.native = file;

    return io;
}
#endif

IO io_open_file(FILE *file) {
    if (file == NULL)
        return NULL;

    IO io = io_alloc(IO_File);
    if (io == NULL)
        return NULL;

    io->data.file.fptr = file;

    /* We really can't determine what mode the file was opened with, so assume read/write */
    io->flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_BINARY;

    return io;
}

IO io_open_empty(void) {
    IO io = io_alloc(IO_Empty);
    if (io != NULL)
        io->flags |= IO_FLAG_READABLE;
    return io;
}

IO io_open_cstring(const char *str, const char *mode) {
    IO io = io_alloc(IO_SizedBuffer);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.sized_buffer.buffer = (unsigned char *) str;
    io->data.sized_buffer.buffer_size = strlen(str);

    if (io->flags & IO_FLAG_WRITABLE) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

IO io_open_const_buffer(const char *buf, size_t size, const char *mode) {
    IO io = io_alloc(IO_SizedBuffer);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.sized_buffer.buffer = (unsigned char *) buf;
    io->data.sized_buffer.buffer_size = size;

    if (io->flags & IO_FLAG_WRITABLE) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

IO io_open_buffer(char *buf, size_t size, const char *mode) {
    IO io = io_alloc(IO_SizedBuffer);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.sized_buffer.buffer = (unsigned char *) buf;
    io->data.sized_buffer.buffer_size = size;

    if (0 == (io->flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE))) {
        io_destroy(io);
        return NULL;
    }

    if ((io->flags & IO_FLAG_WRITABLE) && !(io->flags & IO_FLAG_UPDATE))
        memset(buf, 0, size);

    return io;
}

IO io_open_thread_buffer() {
    IO io = io_alloc(IO_ThreadBuffer);
    if (io == NULL || (io->data.thread_buffer.mutex = mutex_create_recursive()) == NULL) {
        io_destroy(io);
        return NULL;
    }

    io->data.thread_buffer.buffer = NULL;
    io->flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_SUPPORTS_NO_STATE_SWITCH;

    return io;
}

IO io_open_dynamic_buffer(const char *mode) {
    IO io = io_alloc(IO_DynamicBuffer);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->flags |= IO_FLAG_OWNS_BUFFER;
    io->data.dynamic_buffer.buffer = NULL;

    if (0 == (io->flags & IO_FLAG_WRITABLE)) {
        io_destroy(io);
        return NULL;
    }

    return io;
}

IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode) {
    if (custom == NULL)
        return NULL;

    IO io = io_alloc(IO_Custom);
    if (io == NULL || io_set_flags_for_mode(io, mode, NULL)) {
        io_destroy(io);
        return NULL;
    }

    io->data.custom.ptr = userdata;
    io->data.custom.callbacks = custom;

    if (0 == (io->flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE)) ||
             (custom->open != NULL && (io->data.custom.ptr = custom->open(userdata, io)) == NULL)) {
        io_destroy(io);
        return NULL;
    }

    if (custom->flags != NULL) {
        io->flags |= custom->flags(userdata, io);

        if (io_set_flags_for_mode(io, "", NULL)) {
            io_close(io);
            return NULL;
        }
    }

    return io;
}

static int io_putc_n_internal(int ch, size_t count, IO io) {
    if (count == 0)
        return 0;

    unsigned char buffer[IO_COPY_SIZE];
    memset(buffer, ch, sizeof(buffer));

    while (count > sizeof(buffer)) {
        if (io_write_internal(buffer, 1, sizeof(buffer), io) != sizeof(buffer))
            return EOF;
        count -= sizeof(buffer);
    }

    if (io_write_internal(buffer, 1, count, io) != count)
        return EOF;

    return 0;
}

int io_putc(int ch, IO io) {
    io_lock(io);

    if (io_begin_write(io))
        return io_unlocki(io, EOF);

    return io_unlocki(io, io_putc_internal(ch, io));
}

int io_putc_n(int ch, size_t count, IO io) {
    io_lock(io);

    if (io_begin_write(io))
        return io_unlocki(io, EOF);

    return io_unlocki(io, io_putc_n_internal(ch, count, io));
}

int io_puts(const char *str, IO io) {
    size_t len = strlen(str);
    return io_write(str, 1, len, io) == len? 0: EOF;
}

size_t io_put_uint16_le(IO io, unsigned short value) {
    char buf[2];
    buf[0] = (value     ) & 0xff;
    buf[1] = (value >> 8) & 0xff;
    return io_write(buf, 2, 1, io);
}

size_t io_put_uint16_be(IO io, unsigned short value) {
    char buf[2];
    buf[0] = (value >> 8) & 0xff;
    buf[1] = (value     ) & 0xff;
    return io_write(buf, 2, 1, io);
}

size_t io_put_uint32_le(IO io, unsigned long value) {
    char buf[4];
    buf[0] = (value      ) & 0xff;
    buf[1] = (value >>  8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    return io_write(buf, 4, 1, io);
}

size_t io_put_uint32_be(IO io, unsigned long value) {
    char buf[4];
    buf[0] = (value >> 24) & 0xff;
    buf[1] = (value >> 16) & 0xff;
    buf[2] = (value >>  8) & 0xff;
    buf[3] = (value      ) & 0xff;
    return io_write(buf, 4, 1, io);
}

size_t io_put_uint64_le(IO io, unsigned long long value) {
    char buf[8];
    buf[0] = (value      ) & 0xff;
    buf[1] = (value >>  8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    buf[4] = (value >> 32) & 0xff;
    buf[5] = (value >> 40) & 0xff;
    buf[6] = (value >> 48) & 0xff;
    buf[7] = (value >> 56) & 0xff;
    return io_write(buf, 8, 1, io);
}

size_t io_put_uint64_be(IO io, unsigned long long value) {
    char buf[8];
    buf[0] = (value >> 56) & 0xff;
    buf[1] = (value >> 48) & 0xff;
    buf[2] = (value >> 40) & 0xff;
    buf[3] = (value >> 32) & 0xff;
    buf[4] = (value >> 24) & 0xff;
    buf[5] = (value >> 16) & 0xff;
    buf[6] = (value >>  8) & 0xff;
    buf[7] = (value      ) & 0xff;
    return io_write(buf, 8, 1, io);
}

#define PRINTF_FLAG_MINUS 0x01
#define PRINTF_FLAG_PLUS 0x02
#define PRINTF_FLAG_SPACE 0x04
#define PRINTF_FLAG_ZERO 0x08
#define PRINTF_FLAG_HASH 0x10
#define PRINTF_FLAG_HAS_WIDTH 0x20
#define PRINTF_FLAG_HAS_PRECISION 0x40
#define PRINTF_FLAG_HAS_APOSTROPHE 0x80

#define PRINTF_LEN_NONE 0
#define PRINTF_LEN_HH 1
#define PRINTF_LEN_H 2
#define PRINTF_LEN_L 3
#define PRINTF_LEN_LL 4
#define PRINTF_LEN_J 5
#define PRINTF_LEN_Z 6
#define PRINTF_LEN_T 7
#define PRINTF_LEN_BIG_L 8
#define PRINTF_LEN_I 9

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
    unsigned char internal_buffer[4 * sizeof(long long)];
    size_t buffer_length;
    unsigned flags;
};

typedef struct {
    va_list args;
} va_list_wrapper;

typedef struct {
    int type;
    union {
        int i;
        unsigned int ui;
        long l;
        unsigned long ul;
        long long ll;
        unsigned long long ull;
        double d;
        long double ld;
        void *p;
    } data;
} va_list_positional_argument;

typedef struct {
    va_list_positional_argument static_args[16];
    va_list_positional_argument *args;
    size_t allocated, used;
} va_list_positional_argument_list;

static void positional_argument_list_init(va_list_positional_argument_list *list) {
    list->args = list->static_args;
    list->allocated = sizeof(list->static_args)/sizeof(list->static_args[0]);
    list->used = 0;
}

static int positional_argument_list_add_argument(va_list_positional_argument_list *list, const va_list_positional_argument *arg) {
    if (list->used == list->allocated) {
        const size_t new_size = list->allocated? safe_multiply(list->allocated, list->allocated >> 1): 16;
        if (!new_size)
            return CC_ENOMEM;

        int is_static = list->args == list->static_args;

        va_list_positional_argument *new_args = REALLOC(is_static? NULL: list->args, new_size * sizeof(*list->args));
        if (!new_args)
            return CC_ENOMEM;

        if (is_static)
            memcpy(new_args, list->static_args, list->allocated * sizeof(*list->args));

        list->args = new_args;
        list->allocated = new_size;
    }

    list->args[list->used++] = *arg;

    return 0;
}

static void positional_argument_list_free(va_list_positional_argument_list *list) {
    if (list->args != list->static_args)
        FREE(list->args);
}

#define PA_INT 0
#define PA_CHAR 1
#define PA_STRING 2
#define PA_POINTER 3
#define PA_FLOAT 4
#define PA_DOUBLE 5
#define PA_LAST 6
#define PA_FLAG_PTR 0x1000
#define PA_FLAG_SHORT 0x2000
#define PA_FLAG_LONG 0x4000
#define PA_FLAG_LONG_LONG 0x8000
#define PA_FLAG_LONG_DOUBLE PA_FLAG_LONG_LONG

#define PRINTF_D(type, value, flags, prec, len, state)              \
    do {                                                            \
        unsigned char *eptr = (state)->internal_buffer + sizeof((state)->internal_buffer) - 1; \
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
        (state)->buffer_length = eptr-ptr;                           \
    } while (0)

#define PRINTF_U(type, fmt, value, flags, prec, len, state)         \
    do {                                                            \
        const char *alpha = (fmt) == 'X'? "0123456789ABCDEF": "0123456789abcdef"; \
        unsigned char *eptr = (state)->internal_buffer + sizeof((state)->internal_buffer) - 1; \
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
        (state)->buffer_length = eptr-ptr;                           \
    } while (0)

#define PRINTF_ABS(len, value) (((len) == PRINTF_LEN_BIG_L)? fabsl(value): fabs((double) value))

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
            (state)->buffer = (unsigned char *) (isupper((fmt) & UCHAR_MAX)? "INFINITY": "infinity"); \
            (state)->buffer_length = 8;                              \
        } else if (isnan(mval)) {                                   \
            (state)->buffer = (unsigned char *) (isupper((fmt) & UCHAR_MAX)? "NAN": "nan"); \
            (state)->buffer_length = 3;                              \
        } else {                                                    \
            if ((len) == PRINTF_LEN_BIG_L)                          \
                io_printf_f_long(mval, fmt, fmt_flags, prec, len, state); \
            else                                                    \
                io_printf_f(mval, fmt, fmt_flags, prec, len, state);\
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
        if ((chr = io_getc_internal(io)) == EOF)                    \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc_internal(io);                             \
            if (++read > width || !isdigit(chr)) {io_ungetc_internal(chr, io); return UINT_MAX;}\
        }                                                           \
                                                                    \
        if (chr == '0') {                                           \
            chr = io_getc_internal(io);                             \
            if (chr == 'x' || chr == 'X')                           \
                base = 16;                                          \
            else                                                    \
                base = 8;                                           \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
            if (base == 16) {                                       \
                chr = io_getc_internal(io);                         \
                if (!isxdigit(chr))                                 \
                    return UINT_MAX;                                \
                if (++read > width) {io_ungetc_internal(chr, io); return UINT_MAX;}\
            }                                                       \
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            char *str = strchr(alpha, tolower(chr));                \
            if (str == NULL || (str - alpha) >= base) {             \
                io_ungetc_internal(chr, io);                        \
                break;                                              \
            }                                                       \
                                                                    \
            chr = (int) (str - alpha);                              \
                                                                    \
            (value) = (value) * base + (neg? -chr: chr);            \
                                                                    \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
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
        if ((chr = io_getc_internal(io)) == EOF)                    \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc_internal(io);                             \
            if (++read > width || !isdigit(chr)) {io_ungetc_internal(chr, io); return UINT_MAX;}\
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr)) {                                    \
                io_ungetc_internal(chr, io);                        \
                break;                                              \
            }                                                       \
                                                                    \
            chr -= '0';                                             \
                                                                    \
            (value) = (value) * 10 + (neg? -chr: chr);              \
                                                                    \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
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
        if ((chr = io_getc_internal(io)) == EOF)                    \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr)) {                                    \
                io_ungetc_internal(chr, io);                        \
                break;                                              \
            }                                                       \
                                                                    \
            (value) = (value) * 10 + (chr - '0');                   \
                                                                    \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
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
        if ((chr = io_getc_internal(io)) == EOF)                    \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc_internal(io);                             \
            if (++read > width || !isdigit(chr)) {io_ungetc_internal(chr, io); return UINT_MAX;}\
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isdigit(chr) || chr == '8' || chr == '9') {        \
                io_ungetc_internal(chr, io);                        \
                break;                                              \
            }                                                       \
                                                                    \
            chr -= '0';                                             \
                                                                    \
            (value) = (value) * 8 + (neg? -chr: chr);               \
                                                                    \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
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
        if ((chr = io_getc_internal(io)) == EOF)                    \
            return UINT_MAX;                                        \
        if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
                                                                    \
        if (chr == '+' || chr == '-') {                             \
            neg = (chr == '-');                                     \
            chr = io_getc_internal(io);                             \
            if (++read > width || !isdigit(chr)) {io_ungetc_internal(chr, io); return UINT_MAX;}\
        }                                                           \
                                                                    \
        if (chr == '0') {                                           \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
            if (chr == 'x' || chr == 'X') {                         \
                chr = io_getc_internal(io);                         \
                if (!isxdigit(chr))                                 \
                    return UINT_MAX;                                \
                if (++read > width) {io_ungetc_internal(chr, io); return UINT_MAX;}\
            }                                                       \
        }                                                           \
                                                                    \
        while (chr != EOF) {                                        \
            if (!isxdigit(chr)) {                                   \
                io_ungetc_internal(chr, io);                        \
                break;                                              \
            }                                                       \
                                                                    \
            chr = (int) (strchr(alpha, chr) - alpha);               \
                                                                    \
            (value) = (value) * 16 + (neg? -chr: chr);              \
                                                                    \
            chr = io_getc_internal(io);                             \
            if (++read > width) {io_ungetc_internal(chr, io); return --read;}\
        }                                                           \
                                                                    \
        return --read;                                              \
    } while (0)

static void io_printf_f_long(long double value, unsigned char fmt_char, unsigned flags, unsigned prec, unsigned len, struct io_printf_state *state) {
    UNUSED(len)

    size_t precision = (flags) & PRINTF_FLAG_HAS_PRECISION? prec: 6;

    /* Build format string */
    char fmt[10] = "%.*L";

    if (flags & PRINTF_FLAG_HASH)
        strcpy(fmt, "%#.*L");

    /* Then append format specifier to it */
    size_t pos = strlen(fmt);
    fmt[pos++] = fmt_char;
    fmt[pos] = 0;

    /* Then print */
    int length = snprintf((char *) state->internal_buffer, sizeof(state->internal_buffer), fmt, precision, value);
    if (length < (int) sizeof(state->internal_buffer)) {
        state->buffer = state->internal_buffer;
    } else {
        unsigned char *data = MALLOC(length + 1);
        if (data == NULL) {
            state->flags |= PRINTF_STATE_ERROR;
            return;
        }

        state->flags |= PRINTF_STATE_FREE_BUFFER;
        state->buffer = data;
        snprintf((char *) state->buffer, length + 1, fmt, precision, value);
    }

    if (length < 0) {
        state->flags |= PRINTF_STATE_ERROR;
        state->buffer_length = 0;
    } else
        state->buffer_length = length;
}

static void io_printf_f(double value, unsigned char fmt_char, unsigned flags, unsigned prec, unsigned len, struct io_printf_state *state) {
    UNUSED(len)

    size_t precision = (flags) & PRINTF_FLAG_HAS_PRECISION? prec: 6;

    /* Build format string */
    char fmt[10] = "%.*";

    if (flags & PRINTF_FLAG_HASH)
        strcpy(fmt, "%#.*");

    /* Then append format specifier to it */
    size_t pos = strlen(fmt);
    fmt[pos++] = fmt_char;
    fmt[pos] = 0;

    /* Then print */
    int length = snprintf((char *) state->internal_buffer, sizeof(state->internal_buffer), fmt, precision, value);
    if (length < (int) sizeof(state->internal_buffer)) {
        state->buffer = state->internal_buffer;
    } else {
        unsigned char *data = MALLOC(length + 1);
        if (data == NULL)
            state->flags |= PRINTF_STATE_ERROR;
        else {
            state->flags |= PRINTF_STATE_FREE_BUFFER;

            state->buffer = data;
            snprintf((char *) state->buffer, length + 1, fmt, precision, value);
        }
    }

    if (length < 0) {
        state->flags |= PRINTF_STATE_ERROR;
        state->buffer_length = 0;
    } else
        state->buffer_length = length;
}

static int io_printf_signed_int(struct io_printf_state *state, unsigned flags, unsigned prec, unsigned len, va_list_wrapper *args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            int val = va_arg(args->args, int);

            if (len == PRINTF_LEN_HH)
                val = (signed char) val;
            else if (len == PRINTF_LEN_H)
                val = (signed short) val;

            PRINTF_D(int, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            long val = va_arg(args->args, long);
            PRINTF_D(long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            long long val = va_arg(args->args, long long);
            PRINTF_D(long long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            intmax_t val = va_arg(args->args, intmax_t);
            PRINTF_D(intmax_t, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        {
            size_t val = va_arg(args->args, size_t);
            PRINTF_U(size_t, 'u', val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_T:
        case PRINTF_LEN_I:
        {
            ptrdiff_t val = va_arg(args->args, ptrdiff_t);
            PRINTF_D(ptrdiff_t, val, flags, prec, len, state);
            break;
        }
    }

    return (int) state->buffer_length;
}

static int io_printf_unsigned_int(struct io_printf_state *state, char fmt, unsigned flags, unsigned prec, unsigned len, va_list_wrapper *args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            unsigned val = va_arg(args->args, unsigned int);

            if (len == PRINTF_LEN_HH)
                val = (unsigned char) val;
            else if (len == PRINTF_LEN_H)
                val = (unsigned short) val;

            PRINTF_U(unsigned, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            unsigned long val = va_arg(args->args, unsigned long);
            PRINTF_U(unsigned long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            unsigned long long val = va_arg(args->args, unsigned long long);
            PRINTF_U(unsigned long long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            uintmax_t val = va_arg(args->args, uintmax_t);
            PRINTF_U(uintmax_t, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        case PRINTF_LEN_I:
        {
            size_t val = va_arg(args->args, size_t);
            PRINTF_U(size_t, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_T:
        {
            uintmax_t val = va_arg(args->args, ptrdiff_t);
            PRINTF_U(uintmax_t, fmt, val, flags, prec, len, state);
            break;
        }
    }

    return (int) state->buffer_length;
}

/* fmt must be one of "diuox", returns UINT_MAX on failure, number of characters read on success */
static unsigned io_scanf_int_no_arg(char fmt, IO io, unsigned width) {
    unsigned int dummy;
    UNUSED(dummy)
    switch (fmt) {
        case 'd': SCANF_D(unsigned int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'i': SCANF_I(unsigned int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'o': SCANF_O(unsigned int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'u': SCANF_U(unsigned int, dummy, width, PRINTF_LEN_NONE, io); break;
        case 'x': case 'X': SCANF_X(unsigned int, dummy, width, PRINTF_LEN_NONE, io); break;
        default: return UINT_MAX;
    }
}

/* fmt must be one of "diuox", returns UINT_MAX on failure, number of characters read on success */
static unsigned io_scanf_int(char fmt, IO io, unsigned width, unsigned len, va_list_wrapper *args) {
    switch (len) {
        default: return UINT_MAX;
        case PRINTF_LEN_NONE:
        {
            switch (fmt) {
                case 'd': {signed int *val = va_arg(args->args, signed int *); SCANF_D(signed int, *val, width, len, io); break;}
                case 'i': {signed int *val = va_arg(args->args, signed int *); SCANF_I(signed int, *val, width, len, io); break;}
                case 'o': {unsigned int *val = va_arg(args->args, unsigned int *); SCANF_O(unsigned int, *val, width, len, io); break;}
                case 'u': {unsigned int *val = va_arg(args->args, unsigned int *); SCANF_U(unsigned int, *val, width, len, io); break;}
                case 'x': case 'X': {unsigned int *val = va_arg(args->args, unsigned int *); SCANF_X(unsigned int, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_H:
        {
            switch (fmt) {
                case 'd': {signed short *val = va_arg(args->args, signed short *); SCANF_D(signed short, *val, width, len, io); break;}
                case 'i': {signed short *val = va_arg(args->args, signed short *); SCANF_I(signed short, *val, width, len, io); break;}
                case 'o': {unsigned short *val = va_arg(args->args, unsigned short *); SCANF_O(unsigned short, *val, width, len, io); break;}
                case 'u': {unsigned short *val = va_arg(args->args, unsigned short *); SCANF_U(unsigned short, *val, width, len, io); break;}
                case 'x': case 'X': {unsigned short *val = va_arg(args->args, unsigned short *); SCANF_X(unsigned short, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_HH:
        {
            switch (fmt) {
                case 'd': {signed char *val = va_arg(args->args, signed char *); SCANF_D(signed char, *val, width, len, io); break;}
                case 'i': {signed char *val = va_arg(args->args, signed char *); SCANF_I(signed char, *val, width, len, io); break;}
                case 'o': {unsigned char *val = va_arg(args->args, unsigned char *); SCANF_O(unsigned char, *val, width, len, io); break;}
                case 'u': {unsigned char *val = va_arg(args->args, unsigned char *); SCANF_U(unsigned char, *val, width, len, io); break;}
                case 'x': case 'X': {unsigned char *val = va_arg(args->args, unsigned char *); SCANF_X(unsigned char, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_L:
        {
            switch (fmt) {
                case 'd': {signed long *val = va_arg(args->args, signed long *); SCANF_D(signed long, *val, width, len, io); break;}
                case 'i': {signed long *val = va_arg(args->args, signed long *); SCANF_I(signed long, *val, width, len, io); break;}
                case 'o': {unsigned long *val = va_arg(args->args, unsigned long *); SCANF_O(unsigned long, *val, width, len, io); break;}
                case 'u': {unsigned long *val = va_arg(args->args, unsigned long *); SCANF_U(unsigned long, *val, width, len, io); break;}
                case 'x': case 'X': {unsigned long *val = va_arg(args->args, unsigned long *); SCANF_X(unsigned long, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_LL:
        {
            switch (fmt) {
                case 'd': {signed long long *val = va_arg(args->args, signed long long *); SCANF_D(signed long long, *val, width, len, io); break;}
                case 'i': {signed long long *val = va_arg(args->args, signed long long *); SCANF_I(signed long long, *val, width, len, io); break;}
                case 'o': {unsigned long long *val = va_arg(args->args, unsigned long long *); SCANF_O(unsigned long long, *val, width, len, io); break;}
                case 'u': {unsigned long long *val = va_arg(args->args, unsigned long long *); SCANF_U(unsigned long long, *val, width, len, io); break;}
                case 'x': case 'X': {unsigned long long *val = va_arg(args->args, unsigned long long *); SCANF_X(unsigned long long, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_J:
        {
            switch (fmt) {
                case 'd': {intmax_t *val = va_arg(args->args, intmax_t *); SCANF_D(intmax_t, *val, width, len, io); break;}
                case 'i': {intmax_t *val = va_arg(args->args, intmax_t *); SCANF_I(intmax_t, *val, width, len, io); break;}
                case 'o': {uintmax_t *val = va_arg(args->args, uintmax_t *); SCANF_O(uintmax_t, *val, width, len, io); break;}
                case 'u': {uintmax_t *val = va_arg(args->args, uintmax_t *); SCANF_U(uintmax_t, *val, width, len, io); break;}
                case 'x': case 'X': {uintmax_t *val = va_arg(args->args, uintmax_t *); SCANF_X(uintmax_t, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_Z:
        {
            size_t *val = va_arg(args->args, size_t *);
            switch (fmt) {
                case 'd': SCANF_D(size_t, *val, width, len, io); break;
                case 'i': SCANF_I(size_t, *val, width, len, io); break;
                case 'o': SCANF_O(size_t, *val, width, len, io); break;
                case 'u': SCANF_U(size_t, *val, width, len, io); break;
                case 'x': case 'X': SCANF_X(size_t, *val, width, len, io); break;
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_T:
        {
            ptrdiff_t *val = va_arg(args->args, ptrdiff_t *);
            switch (fmt) {
                case 'd': SCANF_D(ptrdiff_t, *val, width, len, io); break;
                case 'i': SCANF_I(ptrdiff_t, *val, width, len, io); break;
                case 'o': SCANF_O(ptrdiff_t, *val, width, len, io); break;
                case 'u': SCANF_U(ptrdiff_t, *val, width, len, io); break;
                case 'x': case 'X': SCANF_X(ptrdiff_t, *val, width, len, io); break;
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_I:
        {
            switch (fmt) {
                case 'd': {ptrdiff_t *val = va_arg(args->args, ptrdiff_t *); SCANF_D(ptrdiff_t, *val, width, len, io); break;}
                case 'i': {ptrdiff_t *val = va_arg(args->args, ptrdiff_t *); SCANF_I(ptrdiff_t, *val, width, len, io); break;}
                case 'o': {size_t *val = va_arg(args->args, size_t *); SCANF_O(size_t, *val, width, len, io); break;}
                case 'u': {size_t *val = va_arg(args->args, size_t *); SCANF_U(size_t, *val, width, len, io); break;}
                case 'x': case 'X': {size_t *val = va_arg(args->args, size_t *); SCANF_X(size_t, *val, width, len, io); break;}
                default: return UINT_MAX;
            }
            break;
        }
    }

    return UINT_MAX;
}

static unsigned io_scanf_float_f(IO io, unsigned width, float *f) {
    Buffer buffer;
    unsigned read = 0;
    int chr, neg = 0, dec = 0;

    buffer_init(&buffer);

    if ((chr = io_getc_internal(io)) == EOF)
        return UINT_MAX;

    if (++read > width)
        goto finish_early;

    if (chr == '+' || chr == '-') {
        neg = (chr == '-');
        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    while (chr != EOF) {
        if (!isdigit(chr) && (chr != '.' || dec))
            break;

        if (chr == '.')
            dec = 1;

        if (buffer_append_chr(&buffer, chr))
            goto cleanup;

        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    if ((chr == 'i' || chr == 'I') && buffer.length == 0) {
        const char *word = "infinity";

        while (chr != EOF) {
            if (buffer_append_chr(&buffer, tolower(chr)))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;

            ++word;
            if (!*word || *word != tolower(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }
        }

        if (buffer.length != 3 && buffer.length != 8)
            goto cleanup;
    } else if ((chr == 'n' || chr == 'N') && buffer.length == 0) {
        /* TODO: Parse NAN */
    } else if (chr == 'e' || chr == 'E') {
        /* Parse <number>E<exponent> */
        if (buffer_append_chr(&buffer, chr))
            goto cleanup;

        chr = io_getc_internal(io);
        if (++read > width || (!isdigit(chr) && chr != '+' && chr != '-'))
            goto cleanup;

        if (chr == '+' || chr == '-') {
            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width || !isdigit(chr))
                goto cleanup;
        }

        while (chr != EOF) {
            if (!isdigit(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }

            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;
        }
    } else {
        io_ungetc_internal(chr, io);
    }

finish: ;
    /* Assign to float and return error if bad parsing */
    char *end;
    float val = strtof(buffer.data, &end);
    printf("\nParsing %s\n", buffer.data);

    if (*end != 0) {
        buffer_destroy(&buffer);
        return UINT_MAX;
    }

    if (f)
        *f = neg? -val: val;

    buffer_destroy(&buffer);
    return --read;

finish_early:
    io_ungetc_internal(chr, io);
    goto finish;

cleanup:
    buffer_destroy(&buffer);
    return UINT_MAX;
}

static unsigned io_scanf_float_d(IO io, unsigned width, double *f) {
    Buffer buffer;
    unsigned read = 0;
    int chr, neg = 0, dec = 0;

    buffer_init(&buffer);

    if ((chr = io_getc_internal(io)) == EOF)
        return UINT_MAX;

    if (++read > width)
        goto finish_early;

    if (chr == '+' || chr == '-') {
        neg = (chr == '-');
        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    while (chr != EOF) {
        if (!isdigit(chr) && (chr != '.' || dec))
            break;

        if (chr == '.')
            dec = 1;

        if (buffer_append_chr(&buffer, chr))
            goto cleanup;

        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    if ((chr == 'i' || chr == 'I') && buffer.length == 0) {
        const char *word = "infinity";

        while (chr != EOF) {
            if (buffer_append_chr(&buffer, tolower(chr)))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;

            ++word;
            if (!*word || *word != tolower(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }
        }

        if (buffer.length != 3 && buffer.length != 8)
            goto cleanup;
    } else if ((chr == 'n' || chr == 'N') && buffer.length == 0) {
        /* TODO: Parse NAN */
    } else if (chr == 'e' || chr == 'E') {
        /* Parse <number>E<exponent> */
        chr = io_getc_internal(io);
        if (++read > width || (!isdigit(chr) && chr != '+' && chr != '-'))
            goto cleanup;

        if (chr == '+' || chr == '-') {
            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width || !isdigit(chr))
                goto cleanup;
        }

        while (chr != EOF) {
            if (!isdigit(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }

            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;
        }
    } else {
        io_ungetc_internal(chr, io);
    }

finish: ;
    /* Assign to float and return error if bad parsing */
    char *end;
    double val = strtod(buffer.data, &end);

    if (*end != 0) {
        buffer_destroy(&buffer);
        return UINT_MAX;
    }

    if (f)
        *f = neg? -val: val;

    buffer_destroy(&buffer);
    return --read;

finish_early:
    io_ungetc_internal(chr, io);
    goto finish;

cleanup:
    buffer_destroy(&buffer);
    return UINT_MAX;
}

static unsigned io_scanf_float_ld(IO io, unsigned width, long double *f) {
    Buffer buffer;
    unsigned read = 0;
    int chr, neg = 0, dec = 0;

    buffer_init(&buffer);

    if ((chr = io_getc_internal(io)) == EOF)
        return UINT_MAX;

    if (++read > width)
        goto finish_early;

    if (chr == '+' || chr == '-') {
        neg = (chr == '-');
        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    while (chr != EOF) {
        if (!isdigit(chr) && (chr != '.' || dec))
            break;

        if (chr == '.')
            dec = 1;

        if (buffer_append_chr(&buffer, chr))
            goto cleanup;

        chr = io_getc_internal(io);
        if (++read > width)
            goto finish_early;
    }

    if ((chr == 'i' || chr == 'I') && buffer.length == 0) {
        const char *word = "infinity";

        while (chr != EOF) {
            if (buffer_append_chr(&buffer, tolower(chr)))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;

            ++word;
            if (!*word || *word != tolower(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }
        }

        if (buffer.length != 3 && buffer.length != 8)
            goto cleanup;
    } else if ((chr == 'n' || chr == 'N') && buffer.length == 0) {
        /* TODO: Parse NAN */
    } else if (chr == 'e' || chr == 'E') {
        /* Parse <number>E<exponent> */
        chr = io_getc_internal(io);
        if (++read > width || (!isdigit(chr) && chr != '+' && chr != '-'))
            goto cleanup;

        if (chr == '+' || chr == '-') {
            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width || !isdigit(chr))
                goto cleanup;
        }

        while (chr != EOF) {
            if (!isdigit(chr)) {
                io_ungetc_internal(chr, io);
                break;
            }

            if (buffer_append_chr(&buffer, chr))
                goto cleanup;

            chr = io_getc_internal(io);
            if (++read > width)
                goto finish_early;
        }
    } else {
        io_ungetc_internal(chr, io);
    }

finish: ;
    /* Assign to float and return error if bad parsing */
    char *end;
    long double val = strtold(buffer.data, &end);

    if (*end != 0) {
        buffer_destroy(&buffer);
        return UINT_MAX;
    }

    if (f)
        *f = neg? -val: val;

    buffer_destroy(&buffer);
    return --read;

finish_early:
    io_ungetc_internal(chr, io);
    goto finish;

cleanup:
    buffer_destroy(&buffer);
    return UINT_MAX;
}

/* fmt must be one of "fega", returns UINT_MAX on failure, number of characters read on success */
static unsigned io_scanf_float_no_arg(char fmt, IO io, unsigned width) {
    float dummy;
    UNUSED(dummy)
    switch (fmt) {
        case 'f': case 'F':
        case 'e': case 'E':
        case 'g': case 'G': return io_scanf_float_f(io, width, &dummy);
        case 'a': case 'A':
        default: return UINT_MAX;
    }
}

/* fmt must be one of "fega", returns UINT_MAX on failure, number of characters read on success */
/* TODO: scanning hexadecimal float representation is not supported at the moment */
static unsigned io_scanf_float(char fmt, IO io, unsigned width, unsigned len, va_list_wrapper *args) {
    switch (len) {
        default: return UINT_MAX;
        case PRINTF_LEN_NONE:
        {
            switch (fmt) {
                case 'f': case 'F':
                case 'e': case 'E':
                case 'g': case 'G': {float *val = va_arg(args->args, float *); return io_scanf_float_f(io, width, val);}
                case 'a': case 'A':
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_L:
        {
            switch (fmt) {
                case 'f': case 'F':
                case 'e': case 'E':
                case 'g': case 'G': {double *val = va_arg(args->args, double *); return io_scanf_float_d(io, width, val);}
                case 'a': case 'A':
                default: return UINT_MAX;
            }
            break;
        }
        case PRINTF_LEN_BIG_L:
        {
            switch (fmt) {
                case 'f': case 'F':
                case 'e': case 'E':
                case 'g': case 'G': {long double *val = va_arg(args->args, long double *); return io_scanf_float_ld(io, width, val);}
                case 'a': case 'A':
                default: return UINT_MAX;
            }
            break;
        }
    }

    return UINT_MAX;
}

#define CLEANUP(x) do {result = (x); goto cleanup;} while (0)

/* TODO: by default io_vscanf, io_vprintf, and io_ftime should be locale-independent, but should be able to support locales too */
int io_vprintf(IO io, const char *fmt, va_list args) {
    io_lock(io);

    int result = 0;
    size_t written = 0;

    struct io_printf_state state;
    va_list_wrapper args_copy;

    state.buffer = state.internal_buffer;
    state.buffer_length = 0;
    state.flags = 0;

    if (io_begin_write(io))
        return io_unlocki(io, -1);

    const char *save_fmt = NULL; /* Saves the start of fmt since we clobber it */
    unsigned int gathered_positional_args = 0; /* Number of positional arguments that we encountered in the format string */
    unsigned int gathered_normal_args = 0; /* Number of normal arguments that we encountered in the format string */
    unsigned int passes = 1; /* Number of passes to run through the format string */
    va_list_positional_argument_list positional_args_list;
    positional_argument_list_init(&positional_args_list);

    va_copy(args_copy.args, args);

    const char *next_fmt = strchr(fmt, '%');
    if (next_fmt == NULL) { /* No formatting specifiers, just write string */
        written = strlen(fmt);
        if (io_write_internal(fmt, 1, written, io) != written)
            CLEANUP(-1);
        goto done_writing;
    }

    save_fmt = next_fmt;

    for (unsigned int pass = 0; pass < passes; ++pass) {
        do {
            const size_t block_size = next_fmt - fmt;

            /* If any data precedes the format */
            if (block_size) {
                const size_t written_now = io_write_internal(fmt, 1, block_size, io);

                if (block_size != written_now)
                    CLEANUP(-1);

                written += written_now;
            }

            /* Write the format itself */
            {
                unsigned fmt_flags = 0, fmt_width = 0, fmt_prec = 0, fmt_len = 0, fmt_positional_spec = 0;

                fmt = next_fmt + 1; /* Skip leading '%' */

                if (*fmt == '%') {
                    if (io_putc_internal('%', io) == EOF)
                        CLEANUP(-1);
                    ++written;
                    goto done_with_format;
                }

                /* read position specifier */
                ++gathered_normal_args;
                if (*fmt != '0') {
                    const char *old = fmt;
                    fmt_positional_spec = io_stou(fmt, &fmt);
                    if (fmt != old) {
                        if (*fmt != '$') { /* We actually read the width early. Oops! */
                            fmt_width = fmt_positional_spec;
                            fmt_positional_spec = 0;
                            fmt_flags |= PRINTF_FLAG_HAS_WIDTH;
                            goto done_with_width;
                        } else {
                            ++fmt;

                            if (--gathered_normal_args)
                                CLEANUP(-2); /* Disallowed to have positional and normal arguments */

                            if (pass == 0)
                                ++gathered_positional_args;
                        }
                    }
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
                        case '\'': fmt_flags |= PRINTF_FLAG_HAS_APOSTROPHE; break;
                        default: --fmt; goto done_with_flags;
                    }
                }
done_with_flags:

                /* read minimum field width */
                if (*fmt == '*') {
                    /* TODO: positional int argument */

                    int width = va_arg(args_copy.args, int);

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
done_with_width:

                /* read precision */
                if (*fmt == '.') {
                    ++fmt;
                    if (*fmt == '*') {
                        /* TODO: positional int argument */

                        int prec = va_arg(args_copy.args, int);

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
                    case 'I':
                        if (fmt[1] == '3' && fmt[2] == '2') {
                            fmt += 2;
                            /* Set type */
                            if (sizeof(unsigned int) * CHAR_BIT == 32)
                                fmt_len = PRINTF_LEN_NONE;
                            else if (sizeof(unsigned long) * CHAR_BIT == 32)
                                fmt_len = PRINTF_LEN_L;
                            else
                                CLEANUP(-2);
                        } else if (fmt[1] == '6' && fmt[2] == '4') {
                            fmt += 2;
                            /* Set type, if available */
                            if (sizeof(unsigned int) * CHAR_BIT == 64)
                                fmt_len = PRINTF_LEN_NONE;
                            else if (sizeof(unsigned long) * CHAR_BIT == 64)
                                fmt_len = PRINTF_LEN_L;
                            else if (sizeof(unsigned long long) * CHAR_BIT == 64)
                                fmt_len = PRINTF_LEN_LL;
                            else
                                CLEANUP(-2);
                        } else { /* plain I is ptrdiff_t if signed, size_t if unsigned */
                            fmt_len = PRINTF_LEN_I;
                        }
                        break;
                    case 'q':
                        if (sizeof(unsigned int) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_NONE;
                        else if (sizeof(unsigned long) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_L;
                        else if (sizeof(unsigned long long) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_LL;
                        else
                            CLEANUP(-2);
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
                state.buffer = state.internal_buffer;

                /* read format specifier */
                switch (*fmt) {
                    default: CLEANUP(-2); /* incomplete format specifier */
                    case 'c':
                    {
                        state.internal_buffer[0] = va_arg(args_copy.args, int);
                        state.buffer_length = 1;
                        break;
                    }
                    case 's':
                    {
                        char *s = va_arg(args_copy.args, char *);
                        size_t len = strlen(s);

                        if ((fmt_flags & PRINTF_FLAG_HAS_PRECISION) && fmt_prec < len)
                            len = fmt_prec;

                        state.buffer = (unsigned char *) s;
                        state.buffer_length = len;
                        break;
                    }
                    case 'n':
                        switch (fmt_len) {
                            case PRINTF_LEN_NONE: *(va_arg(args_copy.args, int *)) = (int) written; break;
                            case PRINTF_LEN_HH: *(va_arg(args_copy.args, signed char *)) = (signed char) written; break;
                            case PRINTF_LEN_H: *(va_arg(args_copy.args, short *)) = (short) written; break;
                            case PRINTF_LEN_L: *(va_arg(args_copy.args, long *)) = (long) written; break;
                            case PRINTF_LEN_LL: *(va_arg(args_copy.args, long long *)) = (long long) written; break;
                            case PRINTF_LEN_J: *(va_arg(args_copy.args, intmax_t *)) = (intmax_t) written; break;
                            case PRINTF_LEN_I: /* fallthrough */
                            case PRINTF_LEN_Z: *(va_arg(args_copy.args, size_t *)) = written; break;
                            case PRINTF_LEN_T: *(va_arg(args_copy.args, ptrdiff_t *)) = (ptrdiff_t) written; break;
                            default: CLEANUP(-2);
                        }
                        break;
                    case 'd':
                    case 'i':
                        state.flags |= PRINTF_STATE_INTEGRAL | PRINTF_STATE_SIGNED;

                        if (io_printf_signed_int(&state, fmt_flags, fmt_prec, fmt_len, &args_copy) < 0)
                            CLEANUP(-2);

                        if (!(fmt_flags & PRINTF_FLAG_HAS_PRECISION))
                            fmt_prec = 1;
                        break;
                    case 'u':
                    case 'o':
                    case 'x':
                    case 'X':
                        state.flags |= PRINTF_STATE_INTEGRAL;

                        if (io_printf_unsigned_int(&state, *fmt, fmt_flags, fmt_prec, fmt_len, &args_copy) < 0)
                            CLEANUP(-2);

                        if (!(fmt_flags & PRINTF_FLAG_HAS_PRECISION)) {
                            switch (*fmt) {
                                case 'u':
                                case 'x':
                                case 'X': fmt_prec = 1; break;
                                case 'o':
                                    if (fmt_flags & PRINTF_FLAG_HASH) /* alternative implementation */
                                        fmt_prec = (unsigned) state.buffer_length + 1;
                                    else
                                        fmt_prec = 1;
                                    break;
                            }
                        }

                        if ((fmt_flags & PRINTF_FLAG_HASH) && (*fmt == 'x' || *fmt == 'X') && state.buffer_length > 0)
                            state.flags |= PRINTF_STATE_ADD_0X;

                        break;
                    case 'a':
                    case 'A':
                    case 'f':
                    case 'F':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                        state.flags |= PRINTF_STATE_FLOATING_POINT;

                        if (fmt_len == PRINTF_LEN_BIG_L) {
                            long double value = va_arg(args_copy.args, long double);
                            PRINTF_F(long double, *fmt, value, fmt_flags, fmt_prec, fmt_len, &state);
                        } else {
                            double value = va_arg(args_copy.args, double);
                            PRINTF_F(double, *fmt, value, fmt_flags, fmt_prec, fmt_len, &state);
                        }

                        /* Clear precision because the helper function takes care of it for floating-point */
                        fmt_prec = 0;

                        if (state.flags & PRINTF_STATE_ERROR)
                            CLEANUP(-1);

                        break;
                    case 'p':
                        state.flags |= PRINTF_STATE_INTEGRAL | PRINTF_STATE_ADD_0X;

                        fmt_flags |= PRINTF_FLAG_HASH | PRINTF_FLAG_HAS_PRECISION;
                        fmt_prec = sizeof(void *) * 2 /* for hex encoding */ * CHAR_BIT / 8;

                        uintmax_t val = (uintmax_t) va_arg(args_copy.args, void *);
                        PRINTF_U(uintmax_t, 'x', val, fmt_flags, fmt_prec, PRINTF_LEN_NONE, &state);

                        break;
                    case '{': { /* Custom extension, print data using provided CommonContainerBase serializer */
                        /* Formats:
                         *   "%{type}": pass [(void*) data] - default serializer for type is used
                         *   "%{type[format]}": pass [(void*) data] - serializer for registered format is used
                         *   "%{type[*]}": pass ["format", (void*) data] - serializer for registered format is used
                         *   "%{type[?]}": pass [(Serializer) function, (void*) data] - serializer function is used
                         *   "%{*}": pass ["type", (void*) data] - default serializer for registered type is used
                         *   "%{*[format]}": pass ["type", (void*) data] - serializer for registered format is used
                         *   "%{*[*]}": pass ["type", "format", (void*) data] - serializer for registered format is used
                         *   "%{*[?]}": pass ["type", (Serializer) function, (void*) data] - serializer function is used
                         *   "%{?}": pass [(CommonContainerBase *) base, (void*) data] - serializer in container base is used
                         *   "%{?[format]}": pass [(CommonContainerBase *) base, (void*) data] - serializer for registered format is used
                         *   "%{?[*]}": pass [(CommonContainerBase *) base, "format", (void*) data] - serializer for registered format is used
                         *   "%{?[?]}": pass [(CommonContainerBase *) base, (Serializer) function, (void*) data] - serializer function is used
                         */
                        Binary typename, formatname;
                        const CommonContainerBase *type = NULL;
                        Serializer serializer = NULL;

                        typename.data = (char *) ++fmt;
                        typename.length = 0;
                        formatname.data = NULL;
                        formatname.length = 0;
                        size_t nested = 0;

                        while (*fmt && (nested || (*fmt != '[' && *fmt != '}'))) {
                            if (*fmt == '{') ++nested;
                            else if (*fmt == '}') {
                                if (!nested)
                                    break;
                                --nested;
                            }

                            ++typename.length;
                            ++fmt;
                        }

                        if (*fmt == '[') {
                            formatname.data = (char *) ++fmt;

                            while (*fmt && (nested || *fmt != ']')) {
                                if (*fmt == '[') ++nested;
                                else if (*fmt == ']') {
                                    if (!nested)
                                        break;
                                    --nested;
                                }

                                ++formatname.length;
                                ++fmt;
                            }

                            if (*fmt == ']')
                                ++fmt;
                        }

                        if (*fmt != '}')
                            CLEANUP(-2);

                        if (typename.length == 1) {
                            if (typename.data[0] == '*') {
                                char *dynamic_typename = va_arg(args_copy.args, char *);

                                typename.data = dynamic_typename;
                                typename.length = strlen(dynamic_typename);
                            } else if (typename.data[0] == '?') {
                                type = va_arg(args_copy.args, const CommonContainerBase *);
                                if (!type)
                                    CLEANUP(-1);
                            }
                        }

                        if (formatname.length == 1) {
                            if (formatname.data[0] == '*') {
                                char *dynamic_formatname = va_arg(args_copy.args, char *);

                                formatname.data = dynamic_formatname;
                                formatname.length = strlen(dynamic_formatname);
                            } else if (formatname.data[0] == '?') {
                                serializer = va_arg(args_copy.args, Serializer);
                                if (!serializer)
                                    CLEANUP(-1);
                            }
                        }

                        void *data = va_arg(args_copy.args, void *);

                        if (!type) { /* If type not specified, it must be registered */
                            type = io_get_registered_type(typename.data, typename.length);
                            if (!type)
                                CLEANUP(-1);
                        }

                        if (!serializer) { /* If serializer not specified, it could be registered name, or if not present, in the type recipe */
                            if (formatname.data != NULL) {
                                struct RegisteredFormat *registered_fmt = io_get_registered_format(formatname.data, formatname.length);

                                serializer = registered_fmt? registered_fmt->serializer: NULL;
                            } else if (type->serialize)
                                serializer = type->serialize;

                            if (!serializer)
                                CLEANUP(-1);
                        }

                        struct SerializerIdentity serializer_identity;
                        serializer_identity.fmt = NULL;
                        serializer_identity.fmt_length = 0;

                        if ((fmt_flags & PRINTF_FLAG_HAS_WIDTH) ||
                            (fmt_flags & PRINTF_FLAG_HAS_PRECISION)) {
                            /* If a width or precision has been specified, space needs to be allocated to hold the
                             * requested serialized data because it may have to be padded or truncated to fit the field
                             * specifier. Unfortunately there isn't really a more optimal solution at the moment.
                             */

                            IO temp = io_open_dynamic_buffer("wb");
                            if (temp == NULL)
                                CLEANUP(-1);
                            else {
                                if (serializer(temp, data, type, &serializer_identity)) {
                                    io_destroy(temp);
                                    CLEANUP(-1);
                                }

                                state.flags |= PRINTF_STATE_FREE_BUFFER;
                                state.buffer = (unsigned char *) io_take_underlying_buffer(temp);
                                state.buffer_length = io_underlying_buffer_size(temp);

                                if ((fmt_flags & PRINTF_FLAG_HAS_PRECISION) && fmt_prec < state.buffer_length)
                                    state.buffer_length = fmt_prec;

                                io_destroy(temp);
                            }
                        } else {
                            /* If no width or precision has been specified, the custom datatype can be directly serialized
                             * to the underlying IO device, since no padding or truncation needs to be done. The length of the
                             * serialized data is mandated to be returned in the serializer_identity struct so it can be added to
                             * the return value of this function. */
                            if (serializer(io, data, type, &serializer_identity)) {
                                CLEANUP(-1);
                            }

                            written += serializer_identity.written;
                            goto done_with_format;
                        }

                        break;
                    }
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
                size_t precCount = 0;

                if ((state.flags & PRINTF_STATE_INTEGRAL) && state.buffer_length < fmt_prec) /* Integral should be expanded to fill precision */
                    precCount = fmt_prec;
                else if ((fmt_flags & PRINTF_FLAG_HAS_PRECISION) && state.buffer_length > fmt_prec) /* All other types should be capped at a maximum size */
                    precCount = fmt_prec;
                else
                    precCount = state.buffer_length;

                if ((fmt_flags & PRINTF_FLAG_HAS_WIDTH) && fmt_width > precCount + addonCharCount)
                    fillCount = fmt_width - precCount - addonCharCount;

                written += fillCount + precCount + addonCharCount;
                precCount -= MIN(precCount, state.buffer_length); /* Remove actual content so precCount only contains number of padded precision characters */

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
                    if (io_putc_n_internal(' ', fillCount, io) == EOF)
                        CLEANUP(-1);

                    fillCount = 0;
                }

                /* add addon characters */
                if (state.flags & PRINTF_STATE_ADD_0X) {
                    if (io_putc_internal('0', io) == EOF || io_putc_internal(isupper(*fmt & 0xff)? 'X': 'x', io) == EOF)
                        CLEANUP(-1);
                } else if (addonChar) {
                    if (io_putc_internal(addonChar, io) == EOF)
                        CLEANUP(-1);
                }

                /* output precision fill */
                if (io_putc_n_internal('0', precCount, io) == EOF)
                    CLEANUP(-1);

                /* output field itself */
                if (io_write_internal(state.buffer, 1, state.buffer_length, io) != state.buffer_length)
                    CLEANUP(-1);

                /* if left aligned, output field fill (this is a NOP if field was right aligned, since fillCount is now 0) */
                if (io_putc_n_internal(' ', fillCount, io) == EOF)
                    CLEANUP(-1);

                if (state.flags & PRINTF_STATE_FREE_BUFFER) {
                    FREE(state.buffer);
                    state.flags -= PRINTF_STATE_FREE_BUFFER;
                }
            }

done_with_format:
            next_fmt = strchr(++fmt, '%');
        } while (next_fmt != NULL);
    }

    if (*fmt) {
        const size_t len = strlen(fmt);
        if (io_write_internal(fmt, 1, len, io) != len)
            CLEANUP(-1);
        written += len;
    }

done_writing:
    if (written > INT_MAX) {
        io_set_error(io, CC_EOVERFLOW);
        result = -1;
    } else
        result = (int) written;

cleanup:
    if (state.flags & PRINTF_STATE_FREE_BUFFER)
        FREE(state.buffer);

    va_end(args_copy.args);

    positional_argument_list_free(&positional_args_list);

    return io_unlocki(io, result);
}

int io_printf(IO io, const char *fmt, ...) {
    int result;

    va_list args;
    va_start(args, fmt);
    result = io_vprintf(io, fmt, args);
    va_end(args);

    return result;
}

int io_ftime(IO io, const char *fmt, const struct tm *timeptr) {
    char buf[128];

    size_t size = strftime(buf, sizeof(buf), fmt, timeptr);
    if (size != 0) {
        if (io_write(buf, 1, size, io) != size)
            return io_error(io);

        return 0;
    }

    char *dbuf = NULL;
    size_t allocated = 128;

    do {
        const size_t new_size = safe_add(allocated, allocated >> 1);
        if (!new_size)
            goto cleanup;

        char *new_buf = REALLOC(dbuf, new_size);
        if (!new_buf)
            goto cleanup;

        dbuf = new_buf;
        allocated = new_size;

        size = strftime(dbuf, allocated, fmt, timeptr);
    } while (size == 0);

    if (io_write(dbuf, 1, size, io) != size)
        return io_error(io);

    FREE(dbuf);

    return 0;

cleanup:
    FREE(dbuf);
    return CC_ENOMEM;
}

int io_putc_internal(int ch, IO io) {
    switch (io->type) {
        case IO_Empty:
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_EWRITE;
            return EOF;
        case IO_File:
        case IO_OwnFile: return fputc(ch, io->data.file.fptr);
        default:
        {
            unsigned char chr = ch;

            if (io_write_internal(&chr, 1, 1, io) != 1)
                return EOF;

            return chr;
        }
    }
}

static size_t io_native_unbuffered_read(void *ptr, size_t size, size_t count, IO io) {
    size_t max = size*count, totalRead = 0;

    if (io_type(io) == IO_Custom) {
        unsigned char *cptr = ptr;
        size_t read = 0;

        if (io->data.custom.callbacks->read == NULL ||
                (read = io->data.custom.callbacks->read(cptr, 1, max, io->data.custom.ptr, io)) != max) {
            if (read == SIZE_MAX || io_error_internal(io)) {
                if (read == SIZE_MAX)
                    read = 0;

                io->flags |= IO_FLAG_ERROR;
                if (io->data.custom.callbacks->read == NULL)
                    io->error = CC_ENOTSUP;
                else if (io->error == 0)
                    io->error = CC_EREAD;
            } else {
                io->flags |= IO_FLAG_EOF;
            }
        }

        /* return number of blocks read */
        return read / size;
    }

#if LINUX_OS
    do {
        size_t amount = max > SSIZE_MAX? SSIZE_MAX: max;
        ssize_t amountRead = 0;

        if ((amountRead = read(io->data.native_file.native, ptr, amount)) <= 0) {
            io->flags |= amountRead < 0? IO_FLAG_ERROR: IO_FLAG_EOF;
            io->error = errno;
            return totalRead / size;
        }

        ptr = (unsigned char *) ptr + amountRead;
        totalRead += amountRead;
        max -= amountRead;
    } while (max);
#elif WINDOWS_OS
    do {
        DWORD amount = max > 0xffffffffu? 0xffffffffu: (DWORD) max;
        DWORD amountRead = 0;

        if (!ReadFile(io->data.native_file.native, ptr, amount, &amountRead, NULL)) {
            io->flags |= IO_FLAG_ERROR;
            io->error = GetLastError();
            return totalRead / size;
        } else if (amountRead == 0) {
            io->flags |= IO_FLAG_EOF;
            return totalRead / size;
        }

        ptr = (unsigned char *) ptr + amountRead;
        totalRead += amountRead;
        max -= amountRead;
    } while (max);
#endif

    return totalRead / size;
}

static size_t io_read_internal_helper(void *ptr, size_t size, size_t count, IO io) {
    if (io->flags & IO_FLAG_EOF)
        return 0;

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return 0;
        case IO_File:
        case IO_OwnFile: return fread(ptr, size, count, io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            size_t max = size*count;
            unsigned char *cptr = ptr;

            if (io->data.native_file.buffer == NULL) /* No buffering */
                return io_native_unbuffered_read(ptr, size, count, io);
            else if (io->data.native_file.buffer_bytes >= max) { /* Enough in buffer to cover reading the entire amount */
                memcpy(ptr, io->data.native_file.buffer + io->data.native_file.buffer_size - io->data.native_file.buffer_bytes, max);
                io->data.native_file.buffer_bytes -= max;
                return count;
            } else { /* Buffered and requested more than buffer holds */
                size_t read = io->data.native_file.buffer_bytes;
                memcpy(cptr, io->data.native_file.buffer + io->data.native_file.buffer_size - io->data.native_file.buffer_bytes, read);

                max -= read;
                cptr += read;
                io->data.native_file.buffer_bytes = 0;

                /* Pull greater-than-buffer-size chunk directly into output, skipping the buffer entirely */
                if (max >= io->data.native_file.buffer_size)
                    return (read + io_native_unbuffered_read(cptr, 1, max, io)) / size;
                else {
                    /* Refill read buffer */
                    /* However, be careful since io_native_unbuffered_read() can set EOF long before we actually read to it
                     * (the buffer could be huge and the read tiny) */
                    if ((io->data.native_file.buffer_bytes = io_native_unbuffered_read(io->data.native_file.buffer, 1, io->data.native_file.buffer_size, io)) != io->data.native_file.buffer_size && io_error_internal(io)) {
                        return read / size;
                    }

                    if (max < io->data.native_file.buffer_bytes)
                        io->flags &= ~IO_FLAG_EOF;

                    /* Move front-aligned buffer to end-aligned buffer as needed */
                    if (io->data.native_file.buffer_bytes != io->data.native_file.buffer_size)
                        memmove(io->data.native_file.buffer + io->data.native_file.buffer_size - io->data.native_file.buffer_bytes, io->data.native_file.buffer, io->data.native_file.buffer_bytes);

                    /* Allow for end of input */
                    if (max > io->data.native_file.buffer_bytes)
                        max = io->data.native_file.buffer_bytes;

                    memcpy(cptr, io->data.native_file.buffer + io->data.native_file.buffer_size - io->data.native_file.buffer_bytes, max);
                    io->data.native_file.buffer_bytes -= max;

                    return (read + max) / size;
                }
            }
        }
        case IO_SizedBuffer: {
            unsigned char *cptr = ptr;
            size_t max = size*count, blocks = 0;
            size_t avail = io->data.sized_buffer.buffer_size > io->data.sized_buffer.buffer_pos?
                        io->data.sized_buffer.buffer_size - io->data.sized_buffer.buffer_pos:
                        0;

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

            memcpy(cptr, io->data.sized_buffer.buffer + io->data.sized_buffer.buffer_pos, max);
            io->data.sized_buffer.buffer_pos += max;

            /* return number of blocks read */
            return blocks / size;
        }
        case IO_ThreadBuffer: {
            unsigned char *cptr = ptr;
            size_t max = size*count;
            size_t avail = io_thread_buffer_size(io);
            size_t contiguous = io_thread_buffer_contiguous_stored_at_end(io);

            /* not enough to cover reading the requested blocks */
            if (avail < max)
                max = avail / size * size;

            if (max <= contiguous) {
                memcpy(cptr, io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_pos, max);
                io->data.thread_buffer.buffer_pos += max;
                io->data.thread_buffer.buffer_pos %= io->data.thread_buffer.buffer_capacity;
            } else {
                memcpy(cptr, io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_pos, contiguous);
                memcpy(cptr + contiguous, io->data.thread_buffer.buffer, max - contiguous);

                io->data.thread_buffer.buffer_pos = max - contiguous;
            }

            return max / size;
        }
        case IO_DynamicBuffer: {
            unsigned char *cptr = ptr;
            size_t max = size*count, blocks = 0;
            size_t avail = io->data.dynamic_buffer.buffer_size > io->data.dynamic_buffer.buffer_pos?
                        io->data.dynamic_buffer.buffer_size - io->data.dynamic_buffer.buffer_pos:
                        0;

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

            memcpy(cptr, io->data.dynamic_buffer.buffer + io->data.dynamic_buffer.buffer_pos, max);
            io->data.dynamic_buffer.buffer_pos += max;

            /* return number of blocks read */
            return blocks / size;
        }
        case IO_Custom: return io_native_unbuffered_read(ptr, size, count, io);
    }
}

static size_t io_read_internal(void *ptr, size_t size, size_t count, IO io) {
    if (io->flags & IO_FLAG_BINARY)
        return io_read_internal_helper(ptr, size, count, io);

    const size_t total = size*count;
    size_t max = total;
    unsigned char *cptr = ptr;
    for (; max; --max) {
        char chr;
        char chr2;

        if (io_read_internal_helper(&chr, 1, 1, io) != 1)
            break;

        if (chr == '\n' || chr == '\r') {
            if (io_read_internal_helper(&chr2, 1, 1, io) != 1) {
                io_clearerr_internal(io);
            } else if (chr + chr2 != '\r' + '\n')
                io_ungetc_internal(chr2, io);

            chr = '\n';
        }

        *cptr++ = chr;
    }

    return (total - max) / size;
}

size_t io_read(void *ptr, size_t size, size_t count, IO io) {
    const size_t total = safe_multiply(size, count);

    io_lock(io);

    if (total == 0) {
        if (size && count) {
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_EINVAL;
        }
        return io_unlockz(io, 0);
    }

    if (io_begin_read(io))
        return io_unlockz(io, 0);

    return io_unlockz(io, io_read_internal(ptr, size, count, io));
}

IO io_reopen(const char *filename, const char *mode, IO io) {
    io->ungetAvail = 0;

    switch (io->type)
    {
        default:
        {
            FILE *f = fopen(filename, mode);

            if (f == NULL)
            {
                io_destroy(io);
                return NULL;
            }

            io_close_without_destroying(io);
            io->data.file.fptr = f;
            io->type = IO_OwnFile;
            io->flags &= ~IO_FLAG_RESET;

            return io;
        }
        case IO_File:
        case IO_OwnFile:
            io->data.file.fptr = freopen(filename, mode, io->data.file.fptr);

            if (io->data.file.fptr == NULL)
            {
                io_destroy(io);
                return NULL;
            }

            return io;
    }
}

int io_vscanf(IO io, const char *fmt, va_list args) {
    io_lock(io);

    int items = 0;
    size_t bytes = 0;

    va_list_wrapper args_copy;

    va_copy(args_copy.args, args);

    if (io_begin_read(io))
        goto cleanup;

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
                case 'I':
                    if (fmt[1] == '3' && fmt[2] == '2') {
                        fmt += 2;
                        /* Set type */
                        if (sizeof(unsigned int) * CHAR_BIT == 32)
                            fmt_len = PRINTF_LEN_NONE;
                        else if (sizeof(unsigned long) * CHAR_BIT == 32)
                            fmt_len = PRINTF_LEN_L;
                        else
                            goto cleanup;
                    } else if (fmt[1] == '6' && fmt[2] == '4') {
                        fmt += 2;
                        /* Set type, if available */
                        if (sizeof(unsigned int) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_NONE;
                        else if (sizeof(unsigned long) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_L;
                        else if (sizeof(unsigned long long) * CHAR_BIT == 64)
                            fmt_len = PRINTF_LEN_LL;
                        else
                            goto cleanup;
                    } else { /* plain I is ptrdiff_t if signed, size_t if unsigned */
                        fmt_len = PRINTF_LEN_I;
                    }
                    break;
                case 'q':
                    if (sizeof(unsigned int) * CHAR_BIT == 64)
                        fmt_len = PRINTF_LEN_NONE;
                    else if (sizeof(unsigned long) * CHAR_BIT == 64)
                        fmt_len = PRINTF_LEN_L;
                    else if (sizeof(unsigned long long) * CHAR_BIT == 64)
                        fmt_len = PRINTF_LEN_LL;
                    else
                        goto cleanup;
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
                    chr = io_getc_internal(io);
                } while (chr != EOF && c_isspace(chr));
                io_ungetc_internal(chr, io);
            }

            switch (*fmt) {
                default: goto cleanup;
                case 'i':
                case 'd':
                case 'u':
                case 'o':
                case 'x':
                case 'X': {
                    unsigned result = discardResult? io_scanf_int_no_arg(*fmt, io, fmt_width):
                                                     io_scanf_int(*fmt, io, fmt_width, fmt_len, &args_copy);
                    if (result == UINT_MAX || result == 0) {
                        bytes += !io_eof_internal(io) && !io_error_internal(io);
                        goto cleanup;
                    }
                    bytes += result;
                    break;
                }
                case 'f':
                case 'F':
                case 'e':
                case 'E':
                case 'g':
                case 'G':
                case 'a':
                case 'A': {
                    unsigned result = discardResult? io_scanf_float_no_arg(*fmt, io, fmt_width):
                                                     io_scanf_float(*fmt, io, fmt_width, fmt_len, &args_copy);
                    if (result == UINT_MAX || result == 0) {
                        bytes += !io_eof_internal(io) && !io_error_internal(io);
                        goto cleanup;
                    }
                    bytes += result;
                    break;
                }
                case 'c': {
                    if (noWidth)
                        fmt_width = 1;

                    if (discardResult) {
                        unsigned saved_width = fmt_width;

                        for (; fmt_width; --fmt_width) {
                            if (io_getc_internal(io) == EOF)
                                goto cleanup;
                        }

                        bytes += saved_width;
                    } else {
                        char *cptr = va_arg(args_copy.args, char *);
                        size_t read = 0;

                        if ((read = io_read_internal(cptr, 1, fmt_width, io)) != fmt_width) {
                            goto cleanup;
                        }

                        bytes += read;
                    }

                    break;
                }
                case 's': {
                    char *dst = discardResult? NULL: va_arg(args_copy.args, char *);

                    for (; fmt_width; --fmt_width) {
                        int chr = io_getc_internal(io);
                        if (chr == EOF || c_isspace(chr)) {
                            io_ungetc_internal(chr, io);
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
                    char *cptr = discardResult? NULL: va_arg(args_copy.args, char *);
                    int match = 0;

                    for (; fmt_width; --fmt_width) {
                        fmt = oldfmt;

                        match = io_getc_internal(io);
                        if (match == EOF)
                            break;

                        /* fmt now points to first char in set and lastCharInSet points to the last char in set */
                        /* They may be pointing to the same char if it's a one-character set */
                        if (fmt == lastCharInSet) {
                            if ((negateSet? match == *fmt: match != *fmt)) {
                                io_ungetc_internal(match, io);
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
                                io_ungetc_internal(match, io);
                                break;
                            }
                        }

                        if (cptr)
                            *cptr++ = match;

                        ++bytes;
                    }

                    fmt = lastCharInSet + 1;
                    if (cptr)
                        *cptr = 0;

                    break;
                }
                case 'n': {
                    switch (fmt_len) {
                        case PRINTF_LEN_NONE: {int *n = va_arg(args_copy.args, int *); *n = (int) bytes; break;}
                        case PRINTF_LEN_HH: {signed char *n = va_arg(args_copy.args, signed char *); *n = (signed char) bytes; break;}
                        case PRINTF_LEN_H: {signed short *n = va_arg(args_copy.args, signed short *); *n = (unsigned char) bytes; break;}
                        case PRINTF_LEN_L: {long int *n = va_arg(args_copy.args, long int *); *n = (long) bytes; break;}
                        case PRINTF_LEN_LL: {long long int *n = va_arg(args_copy.args, long long int *); *n = (long long) bytes; break;}
                        case PRINTF_LEN_J: {intmax_t *n = va_arg(args_copy.args, intmax_t *); *n = (intmax_t) bytes; break;}
                        case PRINTF_LEN_Z: {size_t *n = va_arg(args_copy.args, size_t *); *n = bytes; break;}
                        case PRINTF_LEN_T: {ptrdiff_t *n = va_arg(args_copy.args, ptrdiff_t *); *n = (ptrdiff_t) bytes; break;}
                    }
                    break;
                }
            }

            ++items;
        } else if (c_isspace(chr)) {
            /* Read zero or more space characters */
            int charRead;

            do {
                charRead = io_getc_internal(io);
                if (charRead != EOF)
                    ++bytes;
            } while (charRead != EOF && c_isspace(charRead));

            if (charRead != EOF)
                io_ungetc_internal(charRead, io);

            if (charRead == EOF)
                goto cleanup;
        } else {
            if (chr == '%')
                ++fmt; /* Skip one of two '%' characters */

            /* Read exact match */
            int charRead = io_getc_internal(io);

            if (charRead == EOF)
                goto cleanup;
            else if (charRead != chr) {
                io_ungetc_internal(charRead, io);
                goto cleanup;
            }

            ++bytes;
        }
    }

cleanup:
    va_end(args_copy.args);

    return io_unlocki(io, bytes == 0? EOF: items);
}

int io_scanf(IO io, const char *fmt, ...) {
    int result;

    va_list args;
    va_start(args, fmt);
    result = io_vscanf(io, fmt, args);
    va_end(args);

    return result;
}

static int io_state_switch(IO io) {
    if (io->type == IO_Custom && io->data.custom.callbacks->state_switch != NULL) {
        if (io->data.custom.callbacks->state_switch(io->data.custom.ptr, io))
            return -1;
    }

    if (io_flush(io))
        return -1;

    io_lock(io);

    io->flags &= ~(IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);

    return io_unlocki(io, 0);
}

int io_seek(IO io, long int offset, int origin) {
    if (offset == 0 && origin == SEEK_CUR)
        return io_state_switch(io);

    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile:
            if (fseek(io->data.file.fptr, offset, origin) < 0)
                return -1;
            break;
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.native_file.buffer_bytes;

#if LINUX_OS
            if (lseek(io->data.native_file.native, offset, origin) < 0)
                return -1;
#elif WINDOWS_OS
            if (sizeof(long) == sizeof(LONG)) {
                if (SetFilePointer(io->data.native_file.native, offset, NULL,
                                             origin == SEEK_SET? FILE_BEGIN:
                                             origin == SEEK_CUR? FILE_CURRENT:
                                             origin == SEEK_END? FILE_END: FILE_BEGIN) == INVALID_SET_FILE_POINTER)
                    return -1;
            } else {
                LARGE_INTEGER li;

                li.QuadPart = offset;

                if ((li.LowPart = SetFilePointer(io->data.native_file.native, li.LowPart, &li.HighPart,
                                                 origin == SEEK_SET? FILE_BEGIN:
                                                 origin == SEEK_CUR? FILE_CURRENT:
                                                 origin == SEEK_END? FILE_END: FILE_BEGIN)) == INVALID_SET_FILE_POINTER &&
                        GetLastError() != NO_ERROR)
                    return -1;
            }
#endif

            io->data.native_file.buffer_bytes = 0;
            break;
        case IO_Custom: {
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;

            long seek;

            if (io->data.custom.callbacks->seek != NULL)
                seek = io->data.custom.callbacks->seek(io->data.custom.ptr, offset, origin, io);
            else if (io->data.custom.callbacks->seek64 != NULL)
                seek = io->data.custom.callbacks->seek64(io->data.custom.ptr, offset, origin, io);
            else
                seek = -1;

            if (seek)
                return seek;

            break;
        }
        case IO_SizedBuffer: {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.sized_buffer.buffer_size < (unsigned long) offset)
                        return -1;
                    io->data.sized_buffer.buffer_pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->data.sized_buffer.buffer_pos) ||
                            (offset > 0 && io->data.sized_buffer.buffer_size - io->data.sized_buffer.buffer_pos < (unsigned long) offset))
                        return -1;
                    io->data.sized_buffer.buffer_pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long) -offset > io->data.sized_buffer.buffer_size)
                        return -1;
                    io->data.sized_buffer.buffer_pos = io->data.sized_buffer.buffer_size + offset;
                    break;
            }
            break;
        }
        case IO_DynamicBuffer: {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.dynamic_buffer.buffer_size < (unsigned long) offset)
                        return -1;
                    io->data.dynamic_buffer.buffer_pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->data.dynamic_buffer.buffer_pos) ||
                            (offset > 0 && io->data.dynamic_buffer.buffer_size - io->data.dynamic_buffer.buffer_pos < (unsigned long) offset))
                        return -1;
                    io->data.dynamic_buffer.buffer_pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long) -offset > io->data.dynamic_buffer.buffer_size)
                        return -1;
                    io->data.dynamic_buffer.buffer_pos = io->data.dynamic_buffer.buffer_size + offset;
                    break;
            }
            break;
        }
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}

static int io_seek64_helper(IO io, long long int offset, int origin)
{
    switch (io->type) {
        default: return -1LL;
        case IO_Custom:
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;

            if (io->data.custom.callbacks->seek64 != NULL) {
                int result = io->data.custom.callbacks->seek64(io->data.custom.ptr, offset, origin, io);
                if (result)
                    return result;
            }
            else if (offset >= LONG_MIN && offset <= LONG_MAX && io->data.custom.callbacks->seek) {
                int result = io->data.custom.callbacks->seek(io->data.custom.ptr, (long) offset, origin, io);
                if (result)
                    return result;
            }
            else
                return -1;

            break;
        case IO_SizedBuffer: {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.sized_buffer.buffer_size < (unsigned long long) offset)
                        return -1;
                    io->data.sized_buffer.buffer_pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->data.sized_buffer.buffer_pos) ||
                            (offset > 0 && io->data.sized_buffer.buffer_size - io->data.sized_buffer.buffer_pos < (unsigned long long) offset))
                        return -1;
                    io->data.sized_buffer.buffer_pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long long) -offset > io->data.sized_buffer.buffer_size)
                        return -1;
                    io->data.sized_buffer.buffer_pos = io->data.sized_buffer.buffer_size + offset;
                    break;
            }
            break;
        }
        case IO_DynamicBuffer: {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->data.dynamic_buffer.buffer_size < (unsigned long long) offset)
                        return -1;
                    io->data.dynamic_buffer.buffer_pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->data.dynamic_buffer.buffer_pos) ||
                            (offset > 0 && io->data.dynamic_buffer.buffer_size - io->data.dynamic_buffer.buffer_pos < (unsigned long long) offset))
                        return -1;
                    io->data.dynamic_buffer.buffer_pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long long) -offset > io->data.dynamic_buffer.buffer_size)
                        return -1;
                    io->data.dynamic_buffer.buffer_pos = io->data.dynamic_buffer.buffer_size + offset;
                    break;
            }
            break;
        }
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}

#if WINDOWS_OS
int io_seek64(IO io, long long int offset, int origin) {
    if (offset == 0 && origin == SEEK_CUR)
        return io_state_switch(io);

    switch (io->type) {
        default: return io_seek64_helper(io, offset, origin);
        case IO_File:
        case IO_OwnFile:
            if (_fseeki64(io->data.file.fptr, offset, origin) < 0)
                return -1;
            break;
        case IO_NativeFile:
        case IO_OwnNativeFile: {
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.native_file.buffer_bytes;

            LARGE_INTEGER li;

            li.QuadPart = offset;

            if ((li.LowPart = SetFilePointer(io->data.native_file.native, li.LowPart, &li.HighPart,
                                             origin == SEEK_SET? FILE_BEGIN:
                                             origin == SEEK_CUR? FILE_CURRENT:
                                             origin == SEEK_END? FILE_END: FILE_BEGIN)) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                return -1;

            io->data.native_file.buffer_bytes = 0;

            break;
        }
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}
#elif LINUX_OS
int io_seek64(IO io, long long int offset, int origin) {
    if (offset == 0 && origin == SEEK_CUR)
        return io_state_switch(io);

    switch (io->type) {
        default: return io_seek64_helper(io, offset, origin);
        case IO_File:
        case IO_OwnFile:
            if (fseeko(io->data.file.fptr, offset, origin) < 0)
                return -1;
            break;
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if ((io->flags & IO_FLAG_HAS_JUST_WRITTEN) && io_flush(io))
                return -1;
            else if ((io->flags & IO_FLAG_HAS_JUST_READ) && origin == SEEK_CUR)
                offset -= io->data.native_file.buffer_bytes;

            if (lseek64(io->data.native_file.native, offset, origin) < 0)
                return -1;

            io->data.native_file.buffer_bytes = 0;

            break;
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}
#else
int io_seek64(IO io, long long offset, int origin) {
    if (offset == 0 && origin == SEEK_CUR)
        return io_state_switch(io);

    switch (io->type) {
        default: return io_seek64_helper(io, offset, origin);
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
        default: return io_seek64(io, pos->_pos, SEEK_SET);
        case IO_File:
        case IO_OwnFile:
            if (fsetpos(io->data.file.fptr, &pos->_fpos)) {
                io_set_error_internal(io, errno);
                return -1;
            }
            break;
        case IO_SizedBuffer: io->data.sized_buffer.buffer_pos = pos->_pos; break;
        case IO_DynamicBuffer: io->data.dynamic_buffer.buffer_pos = pos->_pos; break;
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);
    io->ungetAvail = 0;

    return 0;
}

long int io_tell(IO io) {
    switch (io->type) {
        default: return -1L;
        case IO_File:
        case IO_OwnFile: return ftell(io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
#if LINUX_OS
        {
            long off = lseek(io->data.native_file.native, 0, SEEK_CUR);
            if (off < 0)
                return off;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return off - io->data.native_file.buffer_bytes;
            else
                return off + io->data.native_file.buffer_bytes;
        }
#elif WINDOWS_OS
            if (sizeof(long) == sizeof(LONG)) {
                long offset;
                DWORD val;

                if ((val = SetFilePointer(io->data.native_file.native, 0, NULL, FILE_CURRENT)) == INVALID_SET_FILE_POINTER)
                    return -1;
                else
                    offset = (long) val;

                if (io->flags & IO_FLAG_HAS_JUST_READ)
                    return offset - io->data.native_file.buffer_bytes;
                else
                    return offset + io->data.native_file.buffer_bytes;
            } else {
                LARGE_INTEGER li;

                li.QuadPart = 0;

                if ((li.LowPart = SetFilePointer(io->data.native_file.native, li.LowPart, &li.HighPart, FILE_CURRENT)) == INVALID_SET_FILE_POINTER &&
                        GetLastError() != NO_ERROR)
                    return -1;

                if (io->flags & IO_FLAG_HAS_JUST_READ)
                    return li.QuadPart - io->data.native_file.buffer_bytes;
                else
                    return li.QuadPart + io->data.native_file.buffer_bytes;
            }
#endif
        case IO_Custom:
        {
            if (io->data.custom.callbacks->tell != NULL)
                return io->data.custom.callbacks->tell(io->data.custom.ptr, io);
            else
                return -1L;
        }
        case IO_SizedBuffer: return io->data.sized_buffer.buffer_pos;
        case IO_DynamicBuffer: return io->data.dynamic_buffer.buffer_pos;
    }
}

static long long int io_tell64_helper(IO io) {
    switch (io->type) {
        default: return -1LL;
        case IO_Custom:
        {
            if (io->data.custom.callbacks->tell64 != NULL)
                return io->data.custom.callbacks->tell64(io->data.custom.ptr, io);
            else if (io->data.custom.callbacks->tell != NULL)
                return io->data.custom.callbacks->tell(io->data.custom.ptr, io);
            else
                return -1LL;
        }
        case IO_SizedBuffer: return io->data.sized_buffer.buffer_pos;
        case IO_DynamicBuffer: return io->data.dynamic_buffer.buffer_pos;
    }
}

#if WINDOWS_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return io_tell64_helper(io);
        case IO_File:
        case IO_OwnFile: return _ftelli64(io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile: {
            LARGE_INTEGER li;

            li.QuadPart = 0;

            if ((li.LowPart = SetFilePointer(io->data.native_file.native, li.LowPart, &li.HighPart, FILE_CURRENT)) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                return -1;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return li.QuadPart - io->data.native_file.buffer_bytes;
            else
                return li.QuadPart + io->data.native_file.buffer_bytes;
        }
    }
}
#elif LINUX_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return io_tell64_helper(io);
        case IO_File:
        case IO_OwnFile: return ftello(io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile:
        {
            long long off = lseek64(io->data.native_file.native, 0, SEEK_CUR);
            if (off < 0)
                return off;

            if (io->flags & IO_FLAG_HAS_JUST_READ)
                return off - io->data.native_file.buffer_bytes;
            else
                return off + io->data.native_file.buffer_bytes;
        }
    }
}
#else
long long int io_tell64(IO io) {
    if (io->type == IO_Custom)
        return io_tell64_helper(io);
    else
        return io_tell(io);
}
#endif

long int io_size(IO io) {
    IO_Pos pos;

    if (io_getpos(io, &pos))
        return -1;

    if (io_seek(io, 0, SEEK_END))
        return -1;

    long int tell = io_tell(io);

    io_setpos(io, &pos);

    return tell;
}

long long int io_size64(IO io) {
    IO_Pos pos;

    if (io_getpos(io, &pos))
        return -1;

    if (io_seek64(io, 0, SEEK_END))
        return -1;

    long long int tell = io_tell64(io);

    io_setpos(io, &pos);

    return tell;
}

static size_t io_native_unbuffered_write(const void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    size_t max = size*count, totalWritten = 0;

    if (io_type(io) == IO_Custom) {
        if (io->data.custom.callbacks->write == NULL) {
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_ENOTSUP;
            return 0;
        }

        size_t written = io->data.custom.callbacks->write(ptr, size, count, io->data.custom.ptr, io);
        if (written != count) {
            io->flags |= IO_FLAG_ERROR;
            if (io->error == 0)
                io->error = CC_EWRITE;
        }
        return written;
    }

#if LINUX_OS
    do {
        size_t amount = max > SSIZE_MAX? SSIZE_MAX: max;
        ssize_t amountWritten = 0;

        if ((amountWritten = write(io->data.native_file.native, ptr, amount)) < 0) {
            io->flags |= IO_FLAG_ERROR;
            io->error = errno;
            return totalWritten / size;
        }

        ptr = (unsigned char *) ptr + amountWritten;
        totalWritten += amountWritten;
        max -= amountWritten;
    } while (max);
#elif WINDOWS_OS
    do {
        DWORD amount = max > 0xffffffffu? 0xffffffffu: (DWORD) max;
        DWORD amountWritten = 0;

        if (!WriteFile(io->data.native_file.native, ptr, amount, &amountWritten, NULL) || amountWritten != amount) {
            io->flags |= IO_FLAG_ERROR;
            io->error = GetLastError();
            return (totalWritten + amountWritten) / size;
        }

        ptr = (unsigned char *) ptr + amountWritten;
        totalWritten += amountWritten;
        max -= amountWritten;
    } while (max);
#endif

    return totalWritten / size;
}

static size_t io_write_internal_helper(const void *ptr, size_t size, size_t count, IO io) {
    switch (io->type) {
        default:
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_EWRITE;
            return 0;
        case IO_File:
        case IO_OwnFile: return fwrite(ptr, size, count, io->data.file.fptr);
        case IO_NativeFile:
        case IO_OwnNativeFile: {
            size_t max = size*count;
            const unsigned char *cptr = ptr;

            if (io->flags & IO_FLAG_APPEND) {
                if (io_seek(io, 0, SEEK_END)) {
                    io->flags |= IO_FLAG_ERROR;
                    io->error = CC_ESPIPE;
                    return 0;
                }
            }

            if (io->data.native_file.buffer == NULL) /* No buffering */
                return io_native_unbuffered_write(ptr, size, count, io);
            else if (io->data.native_file.buffer_size - io->data.native_file.buffer_bytes >= max) { /* Room to add to buffer */
                memcpy(io->data.native_file.buffer + io->data.native_file.buffer_bytes, ptr, max);
                io->data.native_file.buffer_bytes += max;
                return count;
            } else { /* Buffered and requested output greater than buffer size */
                const size_t initialAvailable = io->data.native_file.buffer_size - io->data.native_file.buffer_bytes;
                size_t written = 0;
                memcpy(io->data.native_file.buffer + io->data.native_file.buffer_bytes, cptr, initialAvailable);

                max -= initialAvailable;
                cptr += initialAvailable;

                if ((written = io_native_unbuffered_write(io->data.native_file.buffer, 1, io->data.native_file.buffer_size, io)) != io->data.native_file.buffer_size) {
                    memmove(io->data.native_file.buffer, io->data.native_file.buffer + written, io->data.native_file.buffer_size - written);
                    io->data.native_file.buffer_bytes = io->data.native_file.buffer_size - written;

                    if (written < io->data.native_file.buffer_size - initialAvailable)
                        return 0;
                    else
                        return (written - (io->data.native_file.buffer_size - initialAvailable)) / size;
                }

                /* Push greater-than-buffer-size chunk directly into output, skipping the buffer entirely */
                if (max >= io->data.native_file.buffer_size) {
                    io->data.native_file.buffer_bytes = 0;
                    return (initialAvailable + io_native_unbuffered_write(cptr, 1, max, io)) / size;
                }
                else {
                    memcpy(io->data.native_file.buffer, cptr, max);
                    io->data.native_file.buffer_bytes = max;

                    return count;
                }
            }
        }
        case IO_SizedBuffer: {
            size_t max = size*count;
            size_t avail = io->data.sized_buffer.buffer_size - io->data.sized_buffer.buffer_pos;

            /* not enough to cover writing the requested blocks */
            if (avail < max)
            {
                io->flags |= IO_FLAG_ERROR;
                io->error = CC_ENOBUFS;

                max = avail - avail % size;
            }

            /* then copy the blocks */
            memcpy(io->data.sized_buffer.buffer + io->data.sized_buffer.buffer_pos, ptr, max);
            io->data.sized_buffer.buffer_pos += max;

            /* return number of blocks written */
            return max / size;
        }
        case IO_ThreadBuffer: {
            const char *cptr = ptr;
            size_t max = size*count;
            const size_t contiguous_to_end = io_thread_buffer_contiguous_empty_at_end(io);

            int err = io_grow_threadbuf(io, max);
            if (err) {
                io_set_error_internal(io, err);
                return 0;
            }

            if (contiguous_to_end >= max) {
                memcpy(io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_endpos, cptr, max);
                io->data.thread_buffer.buffer_endpos += size;
                io->data.thread_buffer.buffer_endpos %= io->data.thread_buffer.buffer_capacity;
            } else {
                memcpy(io->data.thread_buffer.buffer + io->data.thread_buffer.buffer_endpos, cptr, contiguous_to_end);
                max -= contiguous_to_end;

                memcpy(io->data.thread_buffer.buffer, cptr + contiguous_to_end, max);
                io->data.thread_buffer.buffer_endpos = max;
            }

            return count;
        }
        case IO_DynamicBuffer: {
            if (io->flags & IO_FLAG_APPEND)
                io->data.dynamic_buffer.buffer_pos = io->data.dynamic_buffer.buffer_size;

            size_t max = size*count;
            size_t avail = io->data.dynamic_buffer.buffer_size - io->data.dynamic_buffer.buffer_pos;
            size_t required_size = io->data.dynamic_buffer.buffer_size;
            int grow_with_gap = io->data.dynamic_buffer.buffer_pos > io->data.dynamic_buffer.buffer_size;

            if (grow_with_gap) {
                avail = 0;
                required_size = io->data.dynamic_buffer.buffer_pos + max;
            } else if (max > avail)
                required_size += (max - avail);

            /* grow to desired size */
            if (io_grow_dynamic(io, required_size)) {
                io->flags |= IO_FLAG_ERROR;
                io->error = CC_ENOMEM;

                max = avail - avail % size;
                if (grow_with_gap)
                    required_size = io->data.dynamic_buffer.buffer_size;
            }

            /* then copy the blocks */
            memcpy(io->data.dynamic_buffer.buffer + io->data.dynamic_buffer.buffer_pos, ptr, max);
            io->data.dynamic_buffer.buffer_pos += max;
            io->data.dynamic_buffer.buffer_size = required_size;

            /* return number of blocks written */
            return max / size;
        }
        case IO_Custom: return io_native_unbuffered_write(ptr, size, count, io);
    }
}

static size_t io_write_internal(const void *ptr, size_t size, size_t count, IO io) {
    const size_t total = safe_multiply(size, count);
    if (total == 0)
        return 0;

    if (!(io->flags & IO_FLAG_BINARY)) {
#if WINDOWS_OS
        size_t written = 0;
        size_t length = size*count;
        const char newline[] = "\r\n";

        const char *next_n = memchr(ptr, '\n', length);
        if (next_n != NULL) {
            do {
                const size_t block_size = next_n - (const char *) ptr;

                /* If any data precedes the newline */
                if (block_size) {
                    const size_t written_now = io_write_internal_helper(ptr, 1, block_size, io);

                    if (block_size != written_now)
                        return written / size;

                    written += written_now;
                }

                /* Write the newline itself */
                if (io_write_internal_helper(newline, 1, sizeof(newline)-1, io) != sizeof(newline)-1)
                    return written / size;

                ++written;

                /* Find the next newline */
                ptr = next_n + 1;
                length -= block_size + 1;

                next_n = memchr(ptr, '\n', length);
            } while (next_n != NULL);

            return (written + io_write_internal_helper(ptr, 1, length, io)) / size;
        }
#endif
    }

    return io_write_internal_helper(ptr, size, count, io);
}

size_t io_write(const void *ptr, size_t size, size_t count, IO io) {
    const size_t total = safe_multiply(size, count);

    io_lock(io);

    if (total == 0) {
        if (size && count) {
            io->flags |= IO_FLAG_ERROR;
            io->error = CC_EINVAL;
        }
        return io_unlockz(io, 0);
    }

    if (io_begin_write(io))
        return io_unlockz(io, 0);

    return io_unlockz(io, io_write_internal(ptr, size, count, io));
}

void io_rewind(IO io) {
    io_lock(io);

    io->ungetAvail = 0;

    switch (io->type) {
        default: io_seek(io, 0, SEEK_SET); break;
        case IO_File:
        case IO_OwnFile: rewind(io->data.file.fptr); break;
        case IO_SizedBuffer: io->data.sized_buffer.buffer_pos = 0; break;
        case IO_ThreadBuffer: io->data.thread_buffer.buffer_pos = io->data.thread_buffer.buffer_endpos = 0; break;
        case IO_DynamicBuffer: io->data.dynamic_buffer.buffer_pos = 0; break;
    }

    io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN);

    io_unlock(io);
}

void io_setbuf(IO io, char *buf) {
    switch (io->type) {
        default: break;
        case IO_File:
        case IO_OwnFile:
            setbuf(io->data.file.fptr, buf);
            break;
        case IO_NativeFile:
        case IO_OwnNativeFile:
            io_flush(io);

            if (io->flags & IO_FLAG_OWNS_BUFFER) {
                FREE(io->data.native_file.buffer);
                io->flags &= ~IO_FLAG_OWNS_BUFFER;
            }

            io->data.native_file.buffer = (unsigned char *) buf;
            io->data.native_file.buffer_size = buf? BUFSIZ: 0;
            io->data.native_file.buffer_bytes = 0;
            break;
    }
}

int io_setvbuf(IO io, char *buf, int mode, size_t size) {
    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile:
            return setvbuf(io->data.file.fptr, buf, mode, size);
        case IO_NativeFile:
        case IO_OwnNativeFile:
            if (size > (uintmax_t) LONG_MAX || io_flush(io))
                return -1;

            if (io->flags & IO_FLAG_OWNS_BUFFER) {
                FREE(io->data.native_file.buffer);
                io->flags &= ~IO_FLAG_OWNS_BUFFER;
            }

            if (mode == _IONBF) {
                io->data.native_file.buffer = NULL;
                io->data.native_file.buffer_size = 0;
                io->data.native_file.buffer_bytes = 0;
            } else if (buf == NULL) {
                unsigned char *buf = MALLOC(size);
                if (buf == NULL)
                    return CC_ENOMEM;

                io->data.native_file.buffer = buf;
                io->data.native_file.buffer_size = size;
                io->data.native_file.buffer_bytes = 0;
                io->flags |= IO_FLAG_OWNS_BUFFER;
            } else {
                io->data.native_file.buffer = (unsigned char *) buf;
                io->data.native_file.buffer_size = size;
                io->data.native_file.buffer_bytes = 0;
            }

            return 0;
    }
}

IO io_tmpfile(void) {
    IO io = io_alloc(IO_OwnFile);
    if (io == NULL)
        return NULL;

    io->data.file.fptr = tmpfile();
    if (io->data.file.fptr == NULL) {
        io_destroy(io);
        return NULL;
    }

    io->flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_BINARY;

    return io;
}

static int io_set_timeout(IO io, int type, long long usecs) {
    if (io->type != IO_NativeFile)
        return CC_ENOTSUP;

    const char *desc = io_description(io);

#if WINDOWS_OS
    if (!strcmp(desc, "tcp_socket") ||
        !strcmp(desc, "udp_socket") ||
        !strcmp(desc, "ssl_socket")) {
        DWORD timeout_ms = (DWORD) (usecs / 1000);

        if (usecs && !timeout_ms)
            timeout_ms = 1;
        usecs = timeout_ms * 1000LL;

        if (setsockopt((SOCKET) io->data.native_file.native, SOL_SOCKET, type, (const char *) &timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR)
            return WSAGetLastError();

        if (type == SO_RCVTIMEO)
            io->read_timeout = usecs;
        else if (type == SO_SNDTIMEO)
            io->write_timeout = usecs;

        return 0;
    }

    return ERROR_INVALID_HANDLE;
#else
    if (io->type == IO_NativeFile ||
        io->type == IO_OwnNativeFile ||
        !strcmp(desc, "tcp_socket") ||
        !strcmp(desc, "udp_socket") ||
        !strcmp(desc, "ssl_socket")) {
        struct timeval timeout_us;
        timeout_us.tv_sec = 0;
        timeout_us.tv_usec = usecs;

        if (setsockopt((SOCKET) io->data.native_file.native, SOL_SOCKET, type, (const char *) &timeout_us, sizeof(timeout_us)))
            return errno;

        if (type == SO_RCVTIMEO)
            io->read_timeout = usecs;
        else if (type == SO_SNDTIMEO)
            io->write_timeout = usecs;

        return 0;
    }

    return EINVAL;
#endif
}

int io_set_read_timeout(IO io, long long usecs) {
    return io_set_timeout(io, SO_RCVTIMEO, usecs);
}

int io_set_write_timeout(IO io, long long usecs) {
    return io_set_timeout(io, SO_SNDTIMEO, usecs);
}

long long io_read_timeout(IO io) {
    io_lock(io);
    return io_unlockll(io, io->read_timeout);
}

long long io_write_timeout(IO io) {
    io_lock(io);
    return io_unlockll(io, io->write_timeout);
}

enum IO_Type io_type(IO io) {
    return io->type;
}

const char *io_description(IO io) {
    switch (io->type) {
        default: return "unknown";
        case IO_Empty: return "empty";
        case IO_File: return "file";
        case IO_OwnFile: return "owned_file";
        case IO_NativeFile: return "native_file";
        case IO_OwnNativeFile: return "owned_native_file";
        case IO_SizedBuffer: return "sized_buffer";
        case IO_ThreadBuffer: return "thread_buffer";
        case IO_DynamicBuffer: return "dynamic_buffer";
        case IO_Custom:
            if (io->data.custom.callbacks->what == NULL)
                return "custom";

            return io->data.custom.callbacks->what(io_userdata(io), io);
    }
}

int io_format_file_size(IO out, long long size) {
    if (size < 0)
        return io_puts("N/A", out) != 0;
    else if (size < 1024LL)
        return io_printf(out, "%lld bytes", size) < 0;
    else if (size < 1024LL*1024)
        return io_printf(out, "%.1f KB", (double) size / 1024LL) < 0;
    else if (size < 1024LL*1024*1024)
        return io_printf(out, "%.1f MB", (double) size / (1024LL*1024)) < 0;
    else if (size < 1024LL*1024*1024*1024)
        return io_printf(out, "%.1f GB", (double) size / (1024LL*1024*1024)) < 0;
    else if (size < 1024LL*1024*1024*1024*1024)
        return io_printf(out, "%.1f TB", (double) size / (1024LL*1024*1024*1024)) < 0;
    else if (size < 1024LL*1024*1024*1024*1024*1024)
        return io_printf(out, "%.1f PB", (double) size / (1024LL*1024*1024*1024*1024)) < 0;
    else
        return io_printf(out, "%lld bytes", size) < 0;
}
