/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stdarg.h>

#include "platforms.h"

#ifdef __cplusplus
extern "C" {
#endif

struct InputOutputDevice;

/** @brief The root object type to perform all IO on.
 *
 * This is an opaque handle to a runtime-polymorphic object that does not need (or want) any member access.
 *
 * Use of an IO device that references a FILE * will pass all calls on to the C Standard Library. Invariants held in the rest of the library may not apply.
 * If you want more standardized file IO, use the native file functions.
 *
 * Standard IO mode flags include:
 *
 *   - `r` - Make IO device readable.
 *   - `w` - Make IO device writable.
 *   - `+` - Make IO device open for update (implies `rw`).
 *   - `a` - Make IO device open for append. Writes will always occur at the end of the device. Not all IO devices support this mode.
 *   - `x` - Make opening the device fail if the file already exists. Only relevant for file devices.
 *   - `t` - Open device in text mode. On Linux, the stream is left unchanged. On Windows, CRLF is converted to LF when reading, and vice versa when writing.
 *   - `b` - Open device in binary mode. This is the opposite of `t`. If both are specified, the last in the mode string is used.
 *   - `<` - Disable hardware acceleration. Currently only implemented for AES IO.
 *   - `g` - Grab ownership of handle when opening a native file descriptor.
 *   - `@ncp` - If specified in this order, the [n]ative [c]ode [p]age is used on Windows to open files instead of UTF-8.
 *
 * The C Standard Library seek() rules apply to this library as well. You must have a call to `io_seek()`, `io_seek64()`, or `io_rewind()` before switching between reading and writing.
 * The call `io_seek(device, 0, SEEK_CUR)` will only ever fail if an internal buffer failed to flush, even if the stream does not support seeking.
 *  A call to `io_rewind()` also works for non-seekable streams, but has no way of reporting buffer-flush errors directly, although the error flag on the device will still be set.
 */
typedef struct InputOutputDevice *IO;

enum IO_Type
{
    IO_Empty,
    IO_File, /* External FILE * object */
    IO_OwnFile, /* Internally-created FILE * object */
    IO_NativeFile, /* External native file descriptor */
    IO_OwnNativeFile, /* Internally-created native file descriptor */
    IO_CString, /* NUL-terminated C-style string */
    IO_SizedBuffer, /* Size-limited buffer for reading and/or writing */
    IO_MinimalBuffer, /* Dynamic buffer that only grows to fit exact space, does NOT free() buffer when closing */
    IO_DynamicBuffer, /* Dynamic buffer with time-efficient buffer allocation, does NOT free() buffer when closing */
    IO_Custom /* Custom callback for reading and/or writing */
};

/** @brief A structure to hold a stream position for an IO object.
 *
 * This class is not designed to be edited by the end user. It's a get-only/set-only object via `io_getpos()` and `io_setpos()`.
 */
typedef struct
{
    fpos_t _fpos;
    long long _pos;
} IO_Pos;

/** @brief An enum to specify where the either the next or all subsequent IO devices should be opened.
 *
 * IO_HintStatic specifies to use static-duration storage if available, and IO_HintDynamic forces all dynamic memory allocations.
 */
enum IO_OpenHint {
    IO_HintStatic,
    IO_HintDynamic,

    IO_HintShortLived = IO_HintStatic,
    IO_HintLongLived = IO_HintDynamic
};

/** @brief Hint the life-duration of the next open.
 *
 * @param hint A hint as to where to store the next IO device opened.
 * @param permanentHint If @p permanentHint is non-zero, where to allocate all subsequent IO devices if possible, until the next call to this function.
 *                      If @p permanentHint is zero, the hint only applies to the next open of an IO device.
 */
void io_hint_next_open(enum IO_OpenHint hint, int permanentHint);

/* A read callback should return SIZE_MAX if an error was encountered while reading, or a value less than size*count if EOF was reached */
typedef size_t (*IO_ReadCallback)(void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef size_t (*IO_WriteCallback)(const void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef int (*IO_SimpleCallback)(void *userdata, IO io);

/** @brief User-defined callbacks for a custom IO device.
 *
 * This class allows the user to define a custom IO device (openable with `io_open_custom()`), without the need to reinvent the wheel.
 *
 * Either reading or writing must be implemented, or the IO device will not be capable of doing anything, much like `io_open_empty()`.
 * If any field is `NULL`, it signifies that the specified operation is not available, and fails automatically.
 * It is allowable for only one type of `seek()` or `tell()` to be implemented. The permitted operations will be limited or expanded to the necessary range.
 * For each field, the `io` object is the custom IO device itself. This is to allow the individual callbacks access to functions such as `io_readable()` or `io_writable()`.
 *
 * The `userdata` parameter is similar, except in the case of `open`. See the member details for more information.
 *
 * ## Example
 *
 * ```
 *     // Define callback structure
 *     static struct InputOutputDeviceCallbacks {
 *         .open = custom_open,
 *         .close = custom_close,
 *         .read = custom_read,
 *         .write = custom_write,
 *         .flush = NULL,
 *         .tell = NULL,
 *         .tell64 = NULL,
 *         .seek = NULL,
 *         .seek64 = NULL
 *     } custom_callbacks;
 *
 *     ...
 *
 *     // Open the custom device with callback structure
 *     IO device = io_open_custom(&custom_callbacks, "r");
 * ```
 */
struct InputOutputDeviceCallbacks {
    /** @brief Reads some data from the IO device.
     *
     * Read callbacks function identically to the `fread()` standard C library call, except for the return value.
     * If a read callback returns `SIZE_MAX`, then a read error occured. Otherwise, if the return value is less than the requested number of objects, EOF is assumed to have been reached.
     */
    IO_ReadCallback read;

    /** @brief Writes some data to the IO device.
     *
     * Read callbacks function identically to the `fwrite()` standard C library call.
     * If a write callback return value is less than the requested number of objects, a write error occured.
     */
    IO_WriteCallback write;

    /** @brief Opens the IO device and can signify failure.
     *
     * Userdata given to `io_open_custom()` is passed as an argument to `open`, then the userdata in the IO object is set to the return value of this function.
     * If this function returns NULL, the call to open will fail and no IO device will be opened.
     * If `open` itself is NULL, the userdata in the IO object is set to the userdata parameter of `io_open_custom()`, and there is no way to prevent opening the IO device here.
     *
     * @param userdata Userdata field passed as parameter to `io_open_custom()`.
     * @param io The IO device being opened. No reads from or writes to the device should be performed.
     * @return The userdata that should be set in @p io, or NULL if the operation should fail.
     */
    void *(*open)(void *userdata, IO io);
    IO_SimpleCallback close, flush;

    long int (*tell)(void *userdata, IO io);
    long long int (*tell64)(void *userdata, IO io);

    int (*seek)(void *userdata, long int offset, int origin, IO io);
    int (*seek64)(void *userdata, long long int offset, int origin, IO io);
};

/* For Large File Support on Linux, the compile flag -D_FILE_OFFSET_BITS=64 must be used for
 * io_[seek/tell]64() functions to work with 64-bit offsets. io.c specifies these defines */

/* Define IO_DEFAULT_TEXT_MODE to default to text-mode reading and writing, and define
 * IO_DEFAULT_BINARY_MODE to default to binary-mode reading and writing. The default if neither is specified
 * is text mode, just like the C standard library.
 */
#if !defined(IO_DEFAULT_TEXT_MODE) && !defined(IO_DEFAULT_BINARY_MODE)
# define IO_DEFAULT_TEXT_MODE
#endif

void io_clearerr(IO io);
int io_close(IO io);
int io_readable(IO io);
int io_writable(IO io);
int io_binary(IO io);
int io_text(IO io);

void io_grab_file(IO io);
void io_ungrab_file(IO io);

/* Returns pointer to under-the-hood data
 * This pointer should *never* be freed
 */
void *io_userdata(IO io);
/* Returns pointer to dynamic buffer for IO_MinimalBuffer and IO_DynamicBuffer, NULL otherwise */
char *io_underlying_buffer(IO io);
size_t io_underlying_buffer_size(IO io);
size_t io_underlying_buffer_capacity(IO io);
/* Returns pointer to object-specific raw buffer (the size is given by io_tempdata_size(), but is guaranteed to be >= 8 for Custom-type objects)
 * This buffer can be used for anything
 * If this function returns NULL, no buffer is available
 */
unsigned char *io_tempdata(IO io);
size_t io_tempdata_size(IO io);
int io_error(IO io);
int io_eof(IO io);
int io_flush(IO io);
int io_getc(IO io);
int io_getpos(IO io, IO_Pos *pos);
char *io_gets(char *str, int num, IO io);
IO io_open(const char *filename, const char *mode);
/* If `mode` contains "@ncp" on Windows, the [n]ative [c]ode [p]age is used, instead of UTF-8 */
IO io_open_native(const char *filename, const char *mode);
#if LINUX_OS
IO io_open_native_file(int descriptor, const char *mode);
#elif WINDOWS_OS
IO io_open_native_file(HANDLE descriptor, const char *mode);
#endif
IO io_open_file(FILE *file);
IO io_open_empty(void); /* No input is read (i.e. the first read returns EOF) and all writes fail to this device */
IO io_open_cstring(const char *str, const char *mode);
IO io_open_buffer(char *buf, size_t size, const char *mode);
IO io_open_minimal_buffer(const char *mode);
IO io_open_dynamic_buffer(const char *mode);
IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode);
/* Reads all data from `in` and pushes it to `out`
 * Returns -1 on input error, 0 on success, and 1 on output error
 */
int io_copy(IO in, IO out);
int io_vprintf(IO io, const char *fmt, va_list args);
int io_printf(IO io, const char *fmt, ...);
int io_putc(int ch, IO io);
int io_puts(const char *str, IO io);
size_t io_read(void *ptr, size_t size, size_t count, IO io);
IO io_reopen(const char *filename, const char *mode, IO io);
int io_vscanf(IO io, const char *fmt, va_list args);
int io_scanf(IO io, const char *fmt, ...);
int io_seek(IO io, long int offset, int origin);
int io_seek64(IO io, long long int offset, int origin);
int io_setpos(IO io, const IO_Pos *pos);
long int io_tell(IO io);
long long int io_tell64(IO io);
long int io_size(IO io);
long long int io_size64(IO io);
size_t io_write(const void *ptr, size_t size, size_t count, IO io);
void io_rewind(IO io);
void io_setbuf(IO io, char *buf);
int io_setvbuf(IO io, char *buf, int mode, size_t size);
IO io_tmpfile(void);
int io_ungetc(int chr, IO io);

#ifdef __cplusplus
} // extern "C"

#include <stdexcept>
#include <string>

typedef IO_Pos IOPosition;

class IODevice {
    IODevice(const IODevice &) {}

    IODevice(IO io) : m_io(io) {if (!m_io) throw std::bad_alloc();}

public:
    enum ErrorType {
        NoError,
        InputError,
        OutputError
    };

    IODevice(IODevice &&other) : m_io(std::move(other.m_io)) {
        other.m_io = NULL;
    }
    IODevice() : m_io(io_open_empty()) {if (!m_io) throw std::bad_alloc();}
    IODevice(const char *cstring) : m_io(io_open_cstring(cstring, "r")) {if (!m_io) throw std::bad_alloc();}
    IODevice(char *buffer, size_t size, const char *mode) : m_io(io_open_buffer(buffer, size, mode)) {if (!m_io) throw std::bad_alloc();}
    IODevice(const char *filename, const char *mode) : m_io(io_open_native(filename, mode)) {if (!m_io) throw std::bad_alloc();}
    IODevice(FILE *file) : m_io(io_open_file(file)) {if (!m_io) throw std::bad_alloc();}

    virtual ~IODevice() {if (m_io) io_close(m_io);}

    static IODevice makeEmptyDevice() {return {};}
    static IODevice makeStringReader(const char *cstring) {return IODevice(cstring);}
    static IODevice makeStringReader(const char *cstring, size_t size) {return IODevice((char *) cstring, size, "r");}
    static IODevice makeStringReader(const std::string &str) {return makeStringReader(str.data(), str.size());}
    static IODevice makeStringWriter(const char *mode = "w") {return IODevice(io_open_dynamic_buffer(mode));}
    static IODevice makeStringWriter(char *buffer, size_t size, const char *mode = "w") {return IODevice(buffer, size, mode);}
    static IODevice makeSizedBufferDevice(char *buffer, size_t size, const char *mode) {return IODevice(buffer, size, mode);}
    static IODevice makeFileReader(const char *filename) {return makeFileDevice(filename, "r");}
    static IODevice makeFileDevice(const char *filename, const char *mode) {return IODevice(filename, mode);}
    static IODevice makeFileDevice(FILE *file) {return IODevice(file);}

    /* Clears EOF and Error flags */
    void clearError() {io_clearerr(m_io);}

    /* Closes the device and opens a new empty device */
    bool close() {
        int result = io_close(m_io);

        m_io = io_open_empty();
        if (!m_io)
            throw std::bad_alloc();

        return result == 0;
    }

    /* Returns true if the device is readable */
    bool isReadable() const {return io_readable(m_io);}
    /* Returns true if the device is writable */
    bool isWritable() const {return io_writable(m_io);}

    /* Returns true if the error flag is set (this flag is sticky until cleared) */
    bool error() const {return io_error(m_io);}
    /* Returns true if the end of the file was reached */
    bool eof() const {return io_eof(m_io);}

    /* Attempts to flush the device and returns true on success, false on failure
     * For devices that have an input buffer, the buffer is cleared
     */
    bool flush() {return io_flush(m_io) == 0;}

    /* Gets a character and returns true on success, false on failure. Check eof() or error() to determine cause of failure */
    bool getChar(char &chr) {
        int ch = io_getc(m_io);

        if (ch == EOF)
            return false;

        chr = ch;
        return true;
    }
    /* Gets a character and returns it on success, or EOF on failure. Check eof() or error() to determine cause of failure */
    int getChar() {
        return io_getc(m_io);
    }

    /* Attempts to read `max` characters into `buffer`, without NUL-terminating. Returns the number of characters read.
     * Check eof() or error() to determine cause of failure
     */
    size_t read(char *buffer, size_t max) {
        return io_read(buffer, 1, max, m_io);
    }
    /* Reads until EOF or newline, whichever comes first */
    /* NUL-terminates the buffer, so max must be greater than 0. Returns the number of characters read, including the newline
     * The newline character is included in the output
     * Check eof() or error() to determine cause of failure
     */
    size_t readLine(char *buffer, size_t max) {
        char *newBuffer = buffer, ch;

        for (--max; max && getChar(ch); --max) {
            *newBuffer++ = ch;
            if (ch == '\n')
                break;
        }

        *newBuffer = 0;

        return newBuffer - buffer;
    }
    /* Reads until EOF or newline, whichever comes first
     * The newline character is included in the output
     * Returns true on success, false on failure
     */
    bool readLine(std::string &result) {
        char ch;
        result.clear();
        while (getChar(ch)) {
            result.push_back(ch);
            if (ch == '\n')
                break;
        }
        return !error();
    }
    /* Reads until EOF or newline, whichever comes first
     * The newline character is included in the output
     * There is no way to report errors, so an empty output either means no input was available or an error occured
     */
    std::string readLine() {
        std::string result;
        if (!readLine(result))
            return std::string();
        return result;
    }

    /* Reads up to `max` characters from input and returns them in a std::string
     * Check eof() or error() to determine cause of failure
     */
    bool read(std::string &result, size_t max) {
        char ch;
        result.clear();
        while (max-- && getChar(ch))
            result.push_back(ch);
        return !error();
    }
    std::string read(size_t max) {
        std::string result;
        if (!read(result, max))
            return std::string();

        return result;
    }
    /* Reads all the input and returns it in a std::string
     * Check eof() or error() to determine cause of failure
     */
    bool readAll(std::string &result) {
        char ch;
        result.clear();
        while (getChar(ch))
            result.push_back(ch);
        return !error();
    }
    std::string readAll() {
        std::string result;
        if (!readAll(result))
            return std::string();

        return result;
    }

    /* Writes a character and returns true on success, false on failure */
    bool putChar(char chr) {
        return io_putc((unsigned char) chr, m_io) != EOF;
    }
    /* Writes a NUL-terminated string and returns true on success, false on failure */
    bool putString(const char *str) {
        return io_puts(str, m_io) == 0;
    }
    /* Writes a string and returns true on success, false on failure */
    bool putString(const char *str, size_t len) {
        return io_write(str, 1, len, m_io) == len;
    }
    /* Writes a string and returns true on success, false on failure */
    bool putString(const std::string &str) {
        return putString(str.data(), str.size());
    }
    /* Writes a NUL-terminated string followed by a newline ('\n') and returns true on success, false on failure */
    bool putLine(const char *str) {
        return putString(str) && putChar('\n');
    }
    /* Writes a string followed by a newline ('\n') and returns true on success, false on failure */
    bool putLine(const char *str, size_t len) {
        return putString(str, len) && putChar('\n');
    }
    /* Writes a string followed by a newline ('\n') and returns true on success, false on failure */
    bool putLine(const std::string &str) {
        return putString(str) && putChar('\n');
    }
    /* Writes a NUL-terminated string and returns true on success, false on failure */
    bool write(const char *str) {
        return putString(str);
    }
    /* Writes a string and returns true on success, false on failure */
    bool write(const char *str, size_t len) {
        return putString(str, len);
    }
    /* Writes a string and returns true on success, false on failure */
    bool write(const std::string &str) {
        return putString(str);
    }

    bool putInt(signed char i) {
        return printf("%hhd", i) >= 0;
    }
    bool putInt(signed short i) {
        return printf("%hd", i) >= 0;
    }
    bool putInt(signed int i) {
        return printf("%d", i) >= 0;
    }
    bool putInt(signed long i) {
        return printf("%ld", i) >= 0;
    }
    bool putInt(signed long long i) {
        return printf("%lld", i) >= 0;
    }

    bool putInt(unsigned char i, int base = 10) {
        if (base == 10) {
            return printf("%hhu", i) >= 0;
        } else if (base == 8) {
            return printf("%hho", i) >= 0;
        } else if (base == 16) {
            return printf("%hhx", i) >= 0;
        }

        return false;
    }
    bool putInt(unsigned short i, int base = 10) {
        if (base == 10) {
            return printf("%hu", i) >= 0;
        } else if (base == 8) {
            return printf("%ho", i) >= 0;
        } else if (base == 16) {
            return printf("%hx", i) >= 0;
        }

        return false;
    }
    bool putInt(unsigned int i, int base = 10) {
        if (base == 10) {
            return printf("%u", i) >= 0;
        } else if (base == 8) {
            return printf("%o", i) >= 0;
        } else if (base == 16) {
            return printf("%x", i) >= 0;
        }

        return false;
    }
    bool putInt(unsigned long i, int base = 10) {
        if (base == 10) {
            return printf("%lu", i) >= 0;
        } else if (base == 8) {
            return printf("%lo", i) >= 0;
        } else if (base == 16) {
            return printf("%lx", i) >= 0;
        }

        return false;
    }
    bool putInt(unsigned long long i, int base = 10) {
        if (base == 10) {
            return printf("%llu", i) >= 0;
        } else if (base == 8) {
            return printf("%llo", i) >= 0;
        } else if (base == 16) {
            return printf("%llx", i) >= 0;
        }

        return false;
    }

    // TODO: some sort of getLine. Perhaps include gets(char *, int count)?

    /* Reads from this device and pushes all the data to `out`, returning true on success, false on either read or write failure
     * The respective error flag on the failing device will be set
     */
    bool copyTo(IODevice &out) {
        return io_copy(m_io, out.m_io) == 0;
    }
    /* Reads from `in` and pushes all the data to this device, returning true on success, false on either read or write failure
     * The respective error flag on the failing device will be set
     */
    bool copyFrom(IODevice &in) {
        return io_copy(in.m_io, m_io) == 0;
    }

    bool getPosition(IOPosition &position) const {
        return io_getpos(m_io, &position) == 0;
    }
    bool setPosition(IOPosition position) {
        return io_setpos(m_io, &position) == 0;
    }

    /* printf functions return -1 if a writing error occured, or the number of characters written otherwise */
    int vprintf(const char *fmt, va_list args) {
        int res = io_vprintf(m_io, fmt, args);

        if (res == -2)
            throw std::runtime_error("invalid format provided to IOObject::printf()");

        return res;
    }
    int printf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int result;
        try {
            result = vprintf(fmt, args);
        } catch (...) {
            va_end(args);
            throw;
        }
        va_end(args);
        return result;
    }

    bool tell(long long &offset) const {
        long long res = io_tell64(m_io);
        if (res < 0)
            return false;

        offset = res;
        return true;
    }
    long long tell() const {return io_tell64(m_io);}

    bool seek(long long offset, int origin) {
        return io_seek64(m_io, offset, origin) == 0;
    }

    static IODevice tmpfile() {
        return IODevice(io_tmpfile());
    }

    void rewind() {io_rewind(m_io);}

    /* Returns a character back to the input stream, as the next character to return
     * Returns true if the character was put back, false otherwise (the buffer may be full)
     * At least 1 level of ungetChar is supported()
     */
    bool ungetChar(char chr) {
        return io_ungetc((unsigned char) chr, m_io) != EOF;
    }

protected:
    IO m_io; /* The invariant `m_io != NULL` holds everywhere in this class */
};
#endif

#endif // IO_H
