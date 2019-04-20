#define _FILE_OFFSET_BITS 64

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

struct InputOutputDevice {
    /*
     * `ptr` stores the following:
     *
     *      type == IO_File: FILE *
     *      type == IO_CString: const char *
     *      type == IO_SizedBuffer: const char *
     *      type == IO_Custom: void * (pointing to userdata)
     *
     * `size` stores the following:
     *
     *      type == IO_File: undefined
     *      type == IO_CString: size of C-style string, not including terminating NUL
     *      type == IO_SizedBuffer: size of buffer
     *      type == IO_Custom: undefined
     *
     * `pos` stores the following:
     *
     *      type == IO_File: undefined
     *      type == IO_CString: current read position in string
     *      type == IO_SizedBuffer: current read/write position in string
     *      type == IO_Custom: undefined
     *
     * `ungetAvail` stores the number of characters available to read (backwards) from `ungetBuf`
     *  (the last character in the array is the first to be read)
     *
     */
    void *ptr;
    const struct InputOutputDeviceCallbacks *callbacks;
    size_t size, pos;
    enum IO_Type type;
    unsigned flags;

    unsigned ungetAvail;
    unsigned char ungetBuf[4];
};

#define IO_FLAG_READABLE ((unsigned) 0x01)
#define IO_FLAG_WRITABLE ((unsigned) 0x02)
#define IO_FLAG_UPDATE ((unsigned) 0x04)
#define IO_FLAG_APPEND ((unsigned) 0x08)
#define IO_FLAG_ERROR ((unsigned) 0x10)
#define IO_FLAG_EOF ((unsigned) 0x20)
#define IO_FLAG_IN_USE ((unsigned) 0x40)
#define IO_FLAG_DYNAMIC ((unsigned) 0x80)
#define IO_FLAG_RESET (IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_UPDATE | IO_FLAG_APPEND | IO_FLAG_ERROR | IO_FLAG_EOF)

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

static IO io_static_alloc(enum IO_Type type) {
    IO io = NULL;

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

/* TODO: slate for removal? */
static int io_current_char(IO io) {
    switch (io->type) {
        default: return EOF;
        case IO_CString:
        case IO_SizedBuffer: return ((const unsigned char *) io->ptr)[io->pos];
    }
}

/* Gets a character from the unget buffer if available, and removes it, otherwise returns EOF */
static int io_from_unget_buffer(IO io) {
    if (io->ungetAvail == 0)
        return EOF;

    ++io->pos;
    return io->ungetBuf[--io->ungetAvail];
}

static int io_close_without_destroying(IO io) {
    int result = 0;

    switch (io->type) {
        case IO_OwnFile: result = fclose(io->ptr); break;
        case IO_Custom:
            if (io->callbacks->close == NULL)
                return 0;
            return io->callbacks->close(io->ptr, io);
        default: break;
    }

    return result;
}

void io_clearerr(IO io) {
    switch (io->type) {
        default: io->flags &= ~(IO_FLAG_ERROR | IO_FLAG_EOF); break;
        case IO_File:
        case IO_OwnFile: clearerr(io->ptr); break;
    }
}

int io_close(IO io) {
    int result = io_close_without_destroying(io);

    if (io->flags & IO_FLAG_DYNAMIC)
        free(io);
    else
        io->flags = 0;

    return result;
}

int io_readable(IO io) {
    return io->flags & IO_FLAG_READABLE;
}

int io_writable(IO io) {
    return io->flags & IO_FLAG_WRITABLE;
}

void *io_userdata(IO io) {
    return io->ptr;
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
        case IO_Custom:
            if (io->callbacks->flush == NULL)
            {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }
            return io->callbacks->flush(io->ptr, io);
    }
}

int io_getc(IO io) {
    if (!(io->flags & IO_FLAG_READABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    int ch = io_from_unget_buffer(io);
    if (ch != EOF)
        return ch;

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return EOF;
        case IO_File:
        case IO_OwnFile: return fgetc(io->ptr);
        case IO_CString:
        {
            char ch = ((const char *) io->ptr)[io->pos];

            if (ch == 0) {
                io->flags |= IO_FLAG_EOF;
                return EOF;
            }

            io->pos++;

            return (unsigned char) ch;
        }
        case IO_SizedBuffer:
        {
            if (io->pos == io->size) {
                io->flags |= IO_FLAG_EOF;
                return EOF;
            }

            return ((const char *) io->ptr)[io->pos++];
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

int io_getpos(IO io, IO_Pos *pos) {
    switch (io->type) {
        default: pos->_pos = 0; return 0;
        case IO_File:
        case IO_OwnFile: return fgetpos(io->ptr, &pos->_fpos);
        case IO_CString:
        case IO_SizedBuffer: pos->_pos = io->pos; return 0;
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

    if (!(io->flags & IO_FLAG_READABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return NULL;
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return NULL;
        case IO_File:
        case IO_OwnFile: return fgets(str, num, io->ptr);
        case IO_CString:
        case IO_SizedBuffer:
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
    unsigned flags = 0;

    for (; *mode; ++mode) {
        switch (*mode) {
            case 'r': flags |= IO_FLAG_READABLE; break;
            case 'w': flags |= IO_FLAG_WRITABLE; break;
            case '+': flags |= IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_UPDATE; break;
            case 'a': flags |= IO_FLAG_APPEND; break;
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
        free(io);
        return NULL;
    }

    io->flags |= io_flags_for_mode(mode);

    return io;
}

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
    io->pos = 0;

    return io;
}

IO io_open_buffer(char *buf, size_t size, const char *mode) {
    IO io = io_alloc(IO_SizedBuffer);
    if (io == NULL)
        return NULL;

    io->ptr = buf;
    io->size = size;
    io->pos = 0;

    io->flags |= io_flags_for_mode(mode);

    if (0 == (io->flags & (IO_FLAG_READABLE | IO_FLAG_WRITABLE))) {
        free(io);
        return NULL;
    }

    if ((io->flags & IO_FLAG_WRITABLE) && !(io->flags & IO_FLAG_UPDATE))
        memset(buf, 0, size);

    return io;
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
        free(io);
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
                io_printf_f_long(mval, fmt_flags, prec, len, state); \
            else                                                    \
                io_printf_f(mval, fmt_flags, prec, len, state);     \
        }                                                           \
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

static int io_printf_signed_int(struct io_printf_state *state, unsigned flags, unsigned prec, unsigned len, va_list args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            int val = va_arg(args, int);

            if (len == PRINTF_LEN_H)
                val = (signed char) val;
            else if (len == PRINTF_LEN_HH)
                val = (signed short) val;

            PRINTF_D(int, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            long val = va_arg(args, long);
            PRINTF_D(long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            long long val = va_arg(args, long long);
            PRINTF_D(long long, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            intmax_t val = va_arg(args, intmax_t);
            PRINTF_D(intmax_t, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        case PRINTF_LEN_T:
        {
            ptrdiff_t val = va_arg(args, ptrdiff_t);
            PRINTF_D(ptrdiff_t, val, flags, prec, len, state);
            break;
        }
    }

    return state->bufferLength;
}

static int io_printf_unsigned_int(struct io_printf_state *state, char fmt, unsigned flags, unsigned prec, unsigned len, va_list args) {
    UNUSED(flags)
    UNUSED(prec)

    switch (len) {
        default: return -1;
        case PRINTF_LEN_NONE:
        case PRINTF_LEN_H:
        case PRINTF_LEN_HH:
        {
            unsigned val = va_arg(args, unsigned int);

            if (len == PRINTF_LEN_H)
                val = (unsigned char) val;
            else if (len == PRINTF_LEN_HH)
                val = (unsigned short) val;

            PRINTF_U(unsigned, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_L:
        {
            unsigned long val = va_arg(args, unsigned long);
            PRINTF_U(unsigned long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_LL:
        {
            unsigned long long val = va_arg(args, unsigned long long);
            PRINTF_U(unsigned long long, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_J:
        {
            uintmax_t val = va_arg(args, uintmax_t);
            PRINTF_U(uintmax_t, fmt, val, flags, prec, len, state);
            break;
        }
        case PRINTF_LEN_Z:
        case PRINTF_LEN_T:
        {
            size_t val = va_arg(args, size_t);
            PRINTF_U(size_t, fmt, val, flags, prec, len, state);
            break;
        }
    }

    return state->bufferLength;
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
        default: io->flags |= IO_FLAG_ERROR; return -1;
        case IO_File:
        case IO_OwnFile: return vfprintf(io->ptr, fmt, args);
        case IO_SizedBuffer:
        case IO_Custom:
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

                            if (io_printf_signed_int(&state, fmt_flags, fmt_prec, fmt_len, args) < 0)
                                return -2;

                            if (!(fmt_flags & PRINTF_FLAG_HAS_PRECISION))
                                fmt_prec = 1;
                            break;
                        case 'u':
                        case 'o':
                        case 'x':
                        case 'X':
                            state.flags |= PRINTF_STATE_INTEGRAL;

                            if (io_printf_unsigned_int(&state, *fmt, fmt_flags, fmt_prec, fmt_len, args) < 0)
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

int io_putc(int ch, IO io) {
    if (!(io->flags & IO_FLAG_WRITABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return EOF;
        case IO_File:
        case IO_OwnFile: return fputc(ch, io->ptr);
        case IO_SizedBuffer:
            if (io->pos == io->size) {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }
            ((char *) io->ptr)[io->pos++] = (char) ch;
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

int io_puts(const char *str, IO io) {
    if (!(io->flags & IO_FLAG_WRITABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return EOF;
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return EOF;
        case IO_File:
        case IO_OwnFile: return fputs(str, io->ptr);
        case IO_Custom:
        {
            size_t len = strlen(str);
            if (io_write(str, 1, len, io) != len) {
                io->flags |= IO_FLAG_ERROR;
                return EOF;
            }

            return 0;
        }
        case IO_SizedBuffer:
        {
            int result = 0;
            size_t len = strlen(str);
            size_t avail = io->size - io->pos;

            if (avail < len) {
                len = avail;
                io->flags |= IO_FLAG_ERROR;
                result = EOF;
            }

            memcpy((char *) io->ptr + io->pos, str, len);

            io->pos += len;
            return result;
        }
    }
}

size_t io_read(void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    if (!(io->flags & IO_FLAG_READABLE)) {
        io->flags |= IO_FLAG_ERROR;
        return 0;
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_EOF; return 0;
        case IO_File:
        case IO_OwnFile: return fread(ptr, size, count, io->ptr);
        case IO_CString:
        {
            unsigned char *cptr = ptr;
            size_t blocks = 0;
            size_t i = io->ungetAvail, max = size*count;
            size_t start_pos = io->pos;

            /* search for 0 character in C-string */
            for (io->pos += io->ungetAvail; i < max && io_current_char(io) > 0; ++i)
                io->pos++;

            /* if i < max, the NUL is in the desired block,
             * so truncate to reading the most complete blocks we can */
            if (i < max)
                io->flags |= IO_FLAG_EOF;
            else if (i > max)
                i = max;

            i -= i % size;
            io->pos = start_pos;
            blocks = i;

            /* then copy the blocks */
            for (; i && io->ungetAvail; --i)
                *cptr++ = io_from_unget_buffer(io);
            memcpy(cptr, (char *) io->ptr + io->pos, i);
            io->pos += i;

            /* return number of blocks read */
            return blocks / size;
        }
        case IO_SizedBuffer:
        {
            unsigned char *cptr = ptr;
            size_t max = size*count, blocks = 0;
            size_t avail = io->size - io->pos;

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
            memcpy(cptr, (char *) io->ptr + io->pos, max);
            io->pos += max;

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
                free(io);
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
                free(io);
                return NULL;
            }

            return io;
    }
}

/* TODO: scanf */

int io_seek(IO io, long int offset, int origin) {
    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile: return fseek(io->ptr, offset, origin);
        case IO_Custom:
            if (io->callbacks->seek != NULL)
                return io->callbacks->seek(io->ptr, offset, origin, io);
            else if (io->callbacks->seek64 != NULL)
                return io->callbacks->seek64(io->ptr, offset, origin, io);
            else
                return -1;
        case IO_CString:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || (io->pos < (unsigned long) offset && io->pos + strlen(io->ptr) < (unsigned long) offset))
                        return -1;
                    io->pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->pos) || (offset > 0 && strlen(io->ptr) < (unsigned long) offset))
                        return -1;
                    io->pos += offset;
                    break;
                case SEEK_END:
                {
                    size_t len = io->pos + strlen(io->ptr);
                    if (offset > 0 || (unsigned long) -offset > len)
                        return -1;
                    io->pos = len + offset;
                    break;
                }
            }

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            io->ungetAvail = 0;

            return 0;
        }
        case IO_SizedBuffer:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->size < (unsigned long) offset)
                        return -1;
                    io->pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long) -offset > io->pos) || (offset > 0 && io->size < (unsigned long) offset))
                        return -1;
                    io->pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long) -offset > io->size)
                        return -1;
                    io->pos = io->size + offset;
                    break;
            }

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            io->ungetAvail = 0;

            return 0;
        }
    }
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

                io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
                io->ungetAvail = 0;

                return 0;
            }
            else if (offset >= LONG_MIN && offset <= LONG_MAX && io->callbacks->seek) {
                int result = io->callbacks->seek(io->ptr, (long) offset, origin, io);
                if (result)
                    return result;

                io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
                io->ungetAvail = 0;

                return 0;
            }
            else
                return -1;
        case IO_CString:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || (io->pos < (unsigned long long) offset && io->pos + strlen(io->ptr) < (unsigned long long) offset))
                        return -1;
                    io->pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->pos) || (offset > 0 && strlen(io->ptr) < (unsigned long long) offset))
                        return -1;
                    io->pos += offset;
                    break;
                case SEEK_END:
                {
                    size_t len = io->pos + strlen(io->ptr);
                    if (offset > 0 || (unsigned long long) -offset > len)
                        return -1;
                    io->pos = len + offset;
                    break;
                }
            }

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            io->ungetAvail = 0;

            return 0;
        }
        case IO_SizedBuffer:
        {
            switch (origin) {
                case SEEK_SET:
                    if (offset < 0 || io->size < (unsigned long long) offset)
                        return -1;
                    io->pos = offset;
                    break;
                case SEEK_CUR:
                    if ((offset < 0 && (unsigned long long) -offset > io->pos) || (offset > 0 && io->size < (unsigned long long) offset))
                        return -1;
                    io->pos += offset;
                    break;
                case SEEK_END:
                    if (offset > 0 || (unsigned long long) -offset > io->size)
                        return -1;
                    io->pos = io->size + offset;
                    break;
            }

            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            io->ungetAvail = 0;

            return 0;
        }
    }
}

#if WINDOWS_OS
int io_seek64(IO io, long long int offset, int origin) {
    switch (io->type) {
        default: return io_seek64_internal(io, offset, origin);
        case IO_File:
        case IO_OwnFile: return _fseeki64(io->ptr, offset, origin);
    }
}
#elif LINUX_OS
int io_seek64(IO io, long long int offset, int origin) {
    switch (io->type) {
        default: return io_seek64_internal(io, offset, origin);
        case IO_File:
        case IO_OwnFile: return fseeko(io->ptr, offset, origin);
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
        case IO_Custom:
            return io_seek64(io, pos->_pos, SEEK_SET);
        case IO_CString:
        case IO_SizedBuffer:
            io->pos = pos->_pos;
            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            io->ungetAvail = 0;
            return 0;
    }
}

long int io_tell(IO io) {
    switch (io->type) {
        default: return -1L;
        case IO_File:
        case IO_OwnFile: return ftell(io->ptr);
        case IO_Custom:
            if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1L;
        case IO_CString:
        case IO_SizedBuffer: return io->pos;
    }
}

#if WINDOWS_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return -1LL;
        case IO_File:
        case IO_OwnFile: return _ftelli64(io->ptr);
        case IO_Custom:
            if (io->callbacks->tell64 != NULL)
                return io->callbacks->tell64(io->ptr, io);
            else if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1LL;
        case IO_CString:
        case IO_SizedBuffer: return io->pos;
    }
}
#elif LINUX_OS
long long int io_tell64(IO io) {
    switch (io->type) {
        default: return -1LL;
        case IO_File:
        case IO_OwnFile: return ftello(io->ptr);
        case IO_Custom:
            if (io->callbacks->tell64 != NULL)
                return io->callbacks->tell64(io->ptr, io);
            else if (io->callbacks->tell != NULL)
                return io->callbacks->tell(io->ptr, io);
            else
                return -1LL;
        case IO_CString:
        case IO_SizedBuffer: return io->pos;
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

size_t io_write(const void *ptr, size_t size, size_t count, IO io) {
    if (size == 0 || count == 0)
        return 0;

    if (!(io->flags & IO_FLAG_WRITABLE))
    {
        io->flags |= IO_FLAG_ERROR;
        return 0;
    }

    switch (io->type) {
        default: io->flags |= IO_FLAG_ERROR; return 0;
        case IO_File:
        case IO_OwnFile: return fwrite(ptr, size, count, io->ptr);
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
            size_t avail = io->size - io->pos;

            /* not enough to cover writing the requested blocks */
            if (avail < max)
            {
                io->flags |= IO_FLAG_ERROR;

                max = avail - avail % size;
            }

            /* then copy the blocks */
            memcpy((char *) io->ptr + io->pos, ptr, max);
            io->pos += max;

            /* return number of blocks written */
            return max / size;
        }
    }
}

void io_rewind(IO io) {
    io->ungetAvail = 0;

    switch (io->type) {
        default: break;
        case IO_File:
        case IO_OwnFile: rewind(io->ptr); break;
        case IO_Custom: io_seek(io, 0, SEEK_SET); break;
        case IO_CString:
        case IO_SizedBuffer:
            io->pos = 0;
            io->flags &= ~(IO_FLAG_EOF | IO_FLAG_ERROR);
            break;
    }
}

void io_setbuf(IO io, char *buf) {
    switch (io->type) {
        default: break;
        case IO_File:
        case IO_OwnFile: setbuf(io->ptr, buf); break;
    }
}

int io_setvbuf(IO io, char *buf, int mode, size_t size) {
    switch (io->type) {
        default: return -1;
        case IO_File:
        case IO_OwnFile: return setvbuf(io->ptr, buf, mode, size);
    }
}

IO io_tmpfile(void) {
    IO io = io_alloc(IO_OwnFile);
    if (io == NULL)
        return NULL;

    io->ptr = tmpfile();
    if (io->ptr == NULL) {
        free(io);
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

    switch (io->type) {
        default: return EOF;
        case IO_File:
        case IO_OwnFile: return ungetc(chr, io->ptr);
        case IO_CString:
            if (chr == 0)
                return EOF;
            /* fallthrough */
        case IO_Custom:
        case IO_SizedBuffer:
            if (io->ungetAvail != sizeof(io->ungetBuf))
            {
                io->flags &= ~IO_FLAG_EOF;

                --io->pos;
                return io->ungetBuf[io->ungetAvail++] = chr;
            }

            return EOF;
    }
}
