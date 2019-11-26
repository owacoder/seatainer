/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "platforms.h"

#if !WINDOWS_OS
#include <errno.h>
#endif

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
 *         .stateSwitch = NULL,
 *         .tell = NULL,
 *         .tell64 = NULL,
 *         .seek = NULL,
 *         .seek64 = NULL,
 *         .what = NULL,
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
     * If a read callback returns `SIZE_MAX`, or sets an error, then a read error occured.
     * The callback must set the `io` parameter's error code with `io_set_error()`.
     * If no error is returned, and the return value is less than the requested number of objects, EOF is assumed to have been reached.
     */
    IO_ReadCallback read;

    /** @brief Writes some data to the IO device.
     *
     * Read callbacks function identically to the `fwrite()` standard C library call.
     * If a write callback return value is less than the requested number of objects, a write error occured. The callback must set the `io` parameter's error code with `io_set_error()`.
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

    /** @brief Closes the IO device and frees any storage allocated.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being closed. Reads from or writes to the device are allowed.
     * @return Zero on success, non-zero on error. The callback should return an error code, not just a constant non-zero value.
     */
    IO_SimpleCallback close;

    /** @brief Flushes the input buffer or output buffer of the device.
     *
     * This callback must not call any seek function. The callback must set the `io` parameter's error code with `io_set_error()`.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being flushed. Reads from or writes to the device are allowed.
     * @return Zero on success, non-zero on error.
     */
    IO_SimpleCallback flush;

    /** @brief Notifies the device that a state switch was requested.
     *
     * This callback must not call any seek function. The callback must set the `io` parameter's error code with `io_set_error()`.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being acted on. Reads from or writes to the device are allowed.
     * @return Zero on success, non-zero on error.
     */
    IO_SimpleCallback stateSwitch;

    /** @brief Requests a clearing of all embedded IO objects.
     *
     * This callback must only call `io_clearerr()` on any embedded IO objects.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being acted on. Reads from or writes to the device are not allowed.
     */
    void (*clearerr)(void *userdata, IO io);

    /** @brief Requests the current position of the read/write pointer.
     *
     * This callback must not call a seek function, but can call any other function.
     *
     * If only one of `tell` or `tell64` are non-NULL, that overload will be used exclusively.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device that the position is being requested from.
     * @return The current position of the read/write pointer in @p io, or -1 if no position is available or an error occurred.
     */
    long int (*tell)(void *userdata, IO io);
    long long int (*tell64)(void *userdata, IO io);

    /** @brief Requests the current position of the read/write pointer.
     *
     * If only one of `seek` or `seek64` are non-NULL, that overload will be used exclusively, with appropriate range handling.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device that the position is being changed on.
     * @return Zero on success, or non-zero if seeking is not available or an error occurred.
     */
    int (*seek)(void *userdata, long int offset, int origin, IO io);
    int (*seek64)(void *userdata, long long int offset, int origin, IO io);

    /** @brief Returns a constant (non-allocated) string with a machine-friendly description of the device type
     *
     * For example, the native types return the following:
     *
     *    - IO_Empty: "empty"
     *    - IO_File: "file"
     *    - IO_OwnFile: "owned_file"
     *    - IO_NativeFile: "native_file"
     *    - IO_OwnNativeFile: "owned_native_file"
     *    - IO_CString: "cstring"
     *    - IO_SizedBuffer: "sized_buffer"
     *    - IO_MinimalBuffer: "minimal_buffer"
     *    - IO_DynamicBuffer: "dynamic_buffer"
     *    - IO_Custom: "custom"
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being closed. Reads from or writes to the device are allowed.
     * @return A machine-friendly string identifying the type of the IO device.
     */
    const char *(*what)(void *userdata, IO io);
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

/* For Large File Support on Linux, the compile flag -D_FILE_OFFSET_BITS=64 must be used for
 * io_[seek/tell]64() functions to work with 64-bit offsets. io.c specifies these defines */

/** Define IO_DEFAULT_TEXT_MODE to default to text-mode reading and writing, and define
 *  IO_DEFAULT_BINARY_MODE to default to binary-mode reading and writing. The default if neither is specified
 *  is text mode, just like the C standard library.
 */
#if !defined(IO_DEFAULT_TEXT_MODE) && !defined(IO_DEFAULT_BINARY_MODE)
# define IO_DEFAULT_TEXT_MODE
#endif

/** @brief Clears any error or EOF flag present in an IO device.
 *
 * @param io The device to clear error flags on.
 */
void io_clearerr(IO io);

/** @brief Closes a device.
 *
 * @param io The device to close. The handle is invalidated after this function is called.
 * @return An error code showing what error occurred when trying to close @p io. Returns 0 on success.
 */
int io_close(IO io);

/** @brief Closes multiple devices at once.
 *
 * @param count The number of devices to close.
 * @param ... The IO devices to close.
 * @return Returns EOF if an error occurred, or 0 on success. The specific error that occurred is not accessible.
 */
int io_vclose(int count, ...);

/** @brief Detect whether an IO device is readable or not.
 *
 * @param io The IO device on which to check read ability.
 * @return Non-zero if @p io is readable, zero otherwise.
 */
int io_readable(IO io);

/** @brief Detect whether an IO device is writable or not.
 *
 * @param io The IO device on which to check write ability.
 * @return Non-zero if @p io is writable, zero otherwise.
 */
int io_writable(IO io);

/** @brief Returns the IO flags specified for a device. The flags are ORed together.
 *
 * @param io The IO device on which to get the flags.
 * @return A bit vector containing the flags currently set on the device. There is no way to set the flags on an IO device after it's opened.
 */
unsigned io_flags(IO io);

/** @brief Detect whether the last operation performed on an IO device was a read operation.
 *
 * @param io The IO device on which to check the last operation.
 * @return Non-zero if @p io was just read from, zero otherwise.
 */
int io_just_read(IO io);

/** @brief Detect whether the last operation performed on an IO device was a write operation.
 *
 * @param io The IO device on which to check the last operation.
 * @return Non-zero if @p io was just written to, zero otherwise.
 */
int io_just_wrote(IO io);

/** @brief Detect whether an IO device is opened for update.
 *
 * Opening a stream for update does not clear the data that is already present, but allows writing.
 *
 * @param io The IO device to check the update status of.
 * @return Non-zero if @p io is opened for update.
 */
int io_opened_for_update(IO io);

/** @brief Detect whether an IO device is opened for append.
 *
 * Opening a stream for append does not clear the data that is already present, and positions the write point at the end.
 *
 * @param io The IO device to check the append status of.
 * @return Non-zero if @p io is opened for append.
 */
int io_opened_for_append(IO io);

/** @brief Detect whether an IO device is opened as a binary device.
 *
 * Binary devices pass all data through unmodified.
 *
 * @param io The IO device to check the binary/text status of.
 * @return Non-zero if @p io is a binary device.
 */
int io_binary(IO io);

/** @brief Detect whether an IO device is opened as a text device.
 *
 * Text devices convert the native line ending into a single '\\n' character when reading,
 * and convert a single '\\n' character into the native line ending when writing.
 *
 * @param io The IO device to check the binary/text status of.
 * @return Non-zero if @p io is a binary device.
 */
int io_text(IO io);

/** @brief If the IO device was opened accessing an external file, grabs ownership of that file descriptor.
 *
 * @param io The device containing the file descriptor to grab ownership of.
 */
void io_grab_file(IO io);

/** @brief Releases ownership of any file descriptor referenced by an IO device.
 *
 * @param io The device containing the file descriptor to release ownership of.
 */
void io_ungrab_file(IO io);

/** @brief Returns pointer to type-specific data.
 *
 * This pointer should *never* be freed.
 *
 * @param io The device to get the userdata reference from.
 * @return The userdata stored in @p io.
 */
void *io_userdata(IO io);

/** @brief Retakes the ownership of the dynamically-allocated buffer.
 *
 * This function can be used to give back ownership of the underlying buffer to the IO device,
 * where it was previously taken by `io_take_underlying_buffer()`
 *
 * @param io The device to operate on.
 */
void io_grab_underlying_buffer(IO io);

/** @brief If the device is a dynamically-allocated buffer, returns a pointer to that buffer, NULL otherwise.
 *
 * This function only returns a non-NULL pointer for IO_MinimalBuffer and IO_DynamicBuffer.
 * The pointer returned by this function is not freed when the IO device is destroyed, and must be done manually.
 * Ownership of the buffer is transferred to the caller, but may be returned with the `io_grab_underlying_buffer()` function.
 *
 * The returned pointer will never be invalidated, but the pointer must not be freed before the device is closed, or an error may occur.
 *
 * @param io The device to get the buffer from.
 * @return The underlying buffer that was allocated to store data by the IO device.
 */
char *io_take_underlying_buffer(IO io);

/** @brief If the device is a dynamically-allocated buffer, returns a pointer to that buffer, NULL otherwise.
 *
 * This function only returns a non-NULL pointer for IO_MinimalBuffer and IO_DynamicBuffer.
 * The pointer returned by this function is freed when the IO device is destroyed, if the device has ownership of it.
 * Ownership of the buffer is not transferred to the caller, but may be with the `io_take_underlying_buffer()` function.
 *
 * @param io The device to get the buffer from.
 * @return The underlying buffer that was allocated to store data by the IO device.
 */
char *io_underlying_buffer(IO io);

/** @brief If the device is a dynamically-allocated buffer, returns the size if data contained in that buffer, 0 otherwise.
 *
 * This function only returns a non-zero value for IO_MinimalBuffer and IO_DynamicBuffer.
 *
 * @param io The device to get the buffer size of.
 * @return The size of data contained in the underlying buffer that was allocated to store data by the IO device.
 */
size_t io_underlying_buffer_size(IO io);

/** @brief If the device is a dynamically-allocated buffer, returns the capacity (maximum data size) of that buffer, 0 otherwise.
 *
 * This function only returns a non-zero value for IO_MinimalBuffer and IO_DynamicBuffer.
 *
 * @param io The device to get the buffer capacity of.
 * @return The capacity of the underlying buffer that was allocated to store data by the IO device.
 */
size_t io_underlying_buffer_capacity(IO io);

/** @brief Returns pointer to object-specific raw buffer.
 *
 * The size of the returned buffer is given by io_tempdata_size(), and is guaranteed to be >= 8 for Custom-type objects and hold at least two pointers.
 * The returned buffer can be used for anything.
 *
 * @param io The object to obtain the temporary buffer of.
 * @return If this function returns NULL, no buffer is available. Otherwise, the object-specific raw buffer is returned.
 */
unsigned char *io_tempdata(IO io);

/** @brief Returns size of object-specific raw buffer.
 *
 * The size is guaranteed to be >= 8 for Custom-type objects and hold at least two pointers.
 *
 * @param io The object to obtain the temporary buffer size of.
 * @return If this function returns 0, no buffer is available. Otherwise, the size of the object-specific raw buffer is returned.
 */
size_t io_tempdata_size(IO io);

/** @brief Returns what the last error that occured on the device was.
 *
 * These values are system-dependant (i.e. errno for Linux, GetLastError() and WSAGetLastError() for Windows),
 * but convenience comparison macros are available using the `seaerror.h` header, using the prefix CC_<errno value>.
 *
 * @param io The IO device to retrieve the last error from.
 * @return The last error announced by the device, or 0 if no error is currently set.
 */
int io_error(IO io);

/** @brief Sets or clears the error code in an IO device.
 *
 * @param io The IO device to set the error code of.
 * @param err The error code to set on the device. If @p err is 0, the error flag is cleared (but the EOF flag is unmodified).
 * @return 0 on success, EOF on failure.
 */
int io_set_error(IO io, int err);

/** @brief Returns the state of the EOF flag on an IO device.
 *
 * @param io The IO device to get the EOF flag from.
 * @return Non-zero if the EOF flag is set, zero otherwise.
 */
int io_eof(IO io);

/** @brief Flushes the input or output buffer of an IO device, depending on the current mode.
 *
 * @param io The IO device to flush.
 * @return Zero on success, or EOF if the flush failed.
 */
int io_flush(IO io);

/** @brief Resizes an IO device to the given size.
 *
 * All input or output past the given size is discarded, if the IO device is larger than `size`.
 * If the IO device is smaller than `size`, the device is resized to `size` and the new data is filled with zeroes.
 *
 * This function will fail on read-only devices.
 *
 * @param io The IO device to truncate.
 * @param size The size to set the IO device to.
 * @return Zero on success, or EOF if the resize operation failed.
 */
int io_resize(IO io, long long size);

/** @brief Gets the next character from the input stream.
 *
 * Continued calls to this function after EOF is reached will continue to return EOF and will not create an error.
 *
 * @param io The IO device to get the next character from.
 * @return Returns the next character (byte) in the input stream, or EOF if there is no more input or an error occurred.
 */
int io_getc(IO io);

/** @brief Gets the current position of the IO device, if available.
 *
 * @param io The IO device to get the current read/write position from.
 * @param pos The location to save the current position in. This value must not be modified by the end user.
 * @return Zero on success, non-zero if an error occurred.
 */
int io_getpos(IO io, IO_Pos *pos);

char *io_gets(char *str, int num, IO io);
IO io_open(const char *filename, const char *mode);
/* If `mode` contains "@ncp" on Windows, the [n]ative [c]ode [p]age is used, instead of UTF-8 */
IO io_open_native(const char *filename, const char *mode);
#if WINDOWS_OS
#define IO_NATIVE_FILE_HANDLE HANDLE
#define IO_INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
#define IO_NATIVE_FILE_HANDLE int
#define IO_INVALID_FILE_HANDLE (-1)
#endif

IO io_open_native_file(IO_NATIVE_FILE_HANDLE descriptor, const char *mode);
IO io_open_file(FILE *file);
IO io_open_empty(void); /* No input is read (i.e. the first read returns EOF) and all writes fail to this device */
IO io_open_cstring(const char *str, const char *mode);
IO io_open_buffer(char *buf, size_t size, const char *mode);
IO io_open_minimal_buffer(const char *mode);
IO io_open_dynamic_buffer(const char *mode);
IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode);
/* Reads all data from `in` and pushes it to `out`
 * Returns 0 on success, any error that occured on failure. Detection of which stream failed is left up to the caller.
 */
int io_copy(IO in, IO out);
int io_copy_and_close(IO in, IO out);

/** @brief Print a number of arguments to an IO device using a specific format string.
 *
 * With the exception of wide strings, and hexadecimal floating-point output, this function should perform identically to the standard
 * library functions in the `printf()` family. Wide strings and hexadecimal floating-point output are not supported.
 *
 * @param io The IO device to write to.
 * @param fmt The format string specifying what arguments to print.
 * @param args The va_list containing the arguments to print.
 * @return Returns the number of characters successfully written, -1 if a write error occurred, or -2 if the format string was formatted improperly.
 */
int io_vprintf(IO io, const char *fmt, va_list args);

/** @brief Print a number of arguments to an IO device using a specific format string.
 *
 * With the exception of wide strings, and hexadecimal floating-point output, this function should perform identically to the standard
 * library functions in the `printf()` family. Wide strings and hexadecimal floating-point output are not supported.
 *
 * This function forwards the argument list on to io_vprintf().
 *
 * @param io The IO device to write to.
 * @param fmt The format string specifying what arguments to print.
 * @param ... A list of arguments to print.
 * @return Returns the number of characters successfully written, -1 if a write error occurred, or -2 if the format string was formatted improperly.
 */
int io_printf(IO io, const char *fmt, ...);

/** @brief Writes a character to an IO device.
 *
 * If the stream is opened as a text stream, any newline (10) characters written to the stream will be converted to the native
 * newline as needed.
 *
 * @param ch The character to write, which will be casted to `unsigned char` internally.
 * @param io The IO device to write to.
 * @return The character written, or EOF if an error occurred.
 */
int io_putc(int ch, IO io);

/** @brief Writes a value in binary format to an IO device.
 *
 * If the stream is opened as a text stream, the behavior is undefined.
 *
 * @param io The IO device to write to.
 * @param value The value to write to @p io.
 * @return 1 on success, 0 if an error occurred.
 */
size_t io_put_uint16_le(IO io, uint16_t value);
size_t io_put_uint16_be(IO io, uint16_t value);
size_t io_put_uint32_le(IO io, uint32_t value);
size_t io_put_uint32_be(IO io, uint32_t value);
size_t io_put_uint64_le(IO io, uint64_t value);
size_t io_put_uint64_be(IO io, uint64_t value);

/** @brief Writes a string to an IO device.
 *
 * No newline is appended to the string when writing.
 *
 * @param str The string to write to the IO device.
 * @param io The IO device to write to.
 * @return Zero on success, EOF if an error occurred.
 */
int io_puts(const char *str, IO io);

/** @brief Reads a block of data from the IO device.
 *
 * If the return value is equal to @p count, the entire read was successful. If the return value is less than @p count,
 * then EOF was reached or an error occured. Check io_error() or io_eof() to make sure which event happened. Regardless of what occured,
 * the data read from this function is always valid, and may be necessary to finalize the stream, if it is recoverable.
 *
 * @param ptr The destination buffer to write to.
 * @param size The element size of the elements being read. If not needed, just set to 1.
 * @param count The number of elements being read.
 * @param io The IO device that is being read from.
 * @return The number of elements successfully read.
 */
size_t io_read(void *ptr, size_t size, size_t count, IO io);
int io_set_read_timeout(IO io, long long usecs);
int io_set_write_timeout(IO io, long long usecs);
long long io_read_timeout(IO io);
long long io_write_timeout(IO io);
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
enum IO_Type io_type(IO io);
const char *io_description(IO io);
IO io_tmpfile(void);
int io_ungetc(int chr, IO io);

#ifdef __cplusplus
} // extern "C"

#include <stdexcept>
#include <string>
#include <cstring>
#include <memory>

typedef IO_Pos IOPosition;

#define IO_READABLE IOMode("r")
#define IO_WRITABLE IOMode("w")
#define IO_APPEND IOMode("a")
#define IO_UPDATE IOMode("+")
#define IO_EXCLUSIVE IOMode("x")
#define IO_TEXT IOMode("t")
#define IO_BINARY IOMode("b")
#define IO_DISABLE_ACCELERATION IOMode("<")
#define IO_GRAB_OWNERSHIP IOMode("g")
#define IO_NATIVE_CODEPAGE IOMode("@ncp")

struct IOMode {
    IOMode(const std::string &mode) : mode(mode) {}
    IOMode(const char *mode) : mode(mode) {}

    IOMode operator+(const IOMode &other) const {return *this | other;}
    IOMode operator|(const IOMode &other) const {
        return mode + other.mode;
    }

    operator const char *() const {
        return mode.c_str();
    }

    std::string mode;
};

class IODevice {
    IODevice(const IODevice &) {}
    size_t references; /* How many other IODevice objects rely on this object being valid */

protected:
#if WINDOWS_OS
    static const int AlreadyOpen = ERROR_ALREADY_INITIALIZED;
    static const int CannotClose = ERROR_ACCESS_DENIED;
    static const int GenericError = ERROR_DEVICE_NOT_AVAILABLE;
    static const int NoMemory = ERROR_OUTOFMEMORY;
#else
    static const int AlreadyOpen = EPERM;
    static const int CannotClose = EPERM;
    static const int GenericError = EIO;
    static const int NoMemory = ENOMEM;
#endif

    IODevice() : references(0), m_io(NULL) {}

    void tryOpen(int err) {
        if (err)
            throw std::runtime_error("IODevice error when opening: " + errorDescription(err));
    }
    virtual void closing() = 0;

public:
    IODevice(IO device) : references(0), m_io(device) {}
    virtual ~IODevice() {if (m_io) io_close(m_io);}

    /* References prevent this device from closing using close() (but it will still be destroyed with the destructor) */
    void incrementRef() {++references;}
    void decrementRef() {if (references) --references;}

    /* Clears EOF and Error flags */
    void clearError() {if (m_io) io_clearerr(m_io);}

    /* Returns the underlying IO device */
    IO underlyingDevice() const {return m_io;}

    bool isOpen() const {return m_io;}

    /* Closes the device and returns any error
     * The device cannot be closed if another reference is held to this object
     */
    int close() {
        if (references || !isOpen())
            return CannotClose;

        closing();

        int result = io_close(m_io);
        m_io = NULL;

        return result;
    }

    /* Returns true if the device is readable */
    bool isReadable() const {return m_io? io_readable(m_io): false;}
    /* Returns true if the device is writable */
    bool isWritable() const {return m_io? io_writable(m_io): false;}
    /* Returns true if the device is in read mode */
    bool isInReadMode() const {return m_io? io_just_read(m_io): false;}
    /* Returns true if the device is in write mode */
    bool isInWriteMode() const {return m_io? io_just_wrote(m_io): false;}

    /* Returns true if the error flag is set (this flag is sticky until cleared) */
    int error() const {return m_io? io_error(m_io):
#if WINDOWS_OS
                                    ERROR_INVALID_HANDLE
#else
                                    ENODEV
#endif
                                    ;}

    /* Returns human-readable description of what went wrong */
    static std::string errorDescription(int error) {
        char *desc = io_error_description_alloc(error);
        std::string result;

        if (desc == NULL)
            return {};

        try {
            result = desc;
        } catch (...) {
            FREE(desc);
            throw;
        }
        FREE(desc);

        return result;
    }

    /* Returns human-readable description of what went wrong */
    std::string errorDescription() const {
        return errorDescription(error());
    }

    /* Returns true if the end of the file was reached */
    bool eof() const {return m_io? io_eof(m_io): true;}

    /* Attempts to flush the device and returns true on success, false on failure
     * For devices that have an input buffer, the buffer is cleared
     */
    bool flush() {return m_io? io_flush(m_io) == 0: false;}

    bool switchReadWrite() {return seek(0, SEEK_CUR) == 0;}

    /* Gets a character and returns true on success, false on failure. Check eof() or error() to determine cause of failure */
    bool getChar(char &chr) {
        int ch = m_io? io_getc(m_io): EOF;

        if (ch == EOF)
            return false;

        chr = ch;
        return true;
    }
    /* Gets a character and returns it on success, or EOF on failure. Check eof() or error() to determine cause of failure */
    int getChar() {
        return m_io? io_getc(m_io): EOF;
    }

    /* Attempts to read `max` characters into `buffer`, without NUL-terminating. Returns the number of characters read.
     * Check eof() or error() to determine cause of failure
     */
    size_t read(char *buffer, size_t max) {
        return m_io? io_read(buffer, 1, max, m_io): 0;
    }
    /* Reads until EOF or newline, whichever comes first */
    /* NUL-terminates the buffer, so max must be greater than 0. Returns the number of characters read, including the newline
     * The newline character is included in the output
     * Check eof() or error() to determine cause of failure
     */
    size_t readLine(char *buffer, size_t max) {
        char *newBuffer = buffer, ch;

        if (max == 0)
            throw std::runtime_error("Invalid size passed to readLine");

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
     * Check error() to determine cause of failure
     */
    bool readAll(std::string &result) {
        char buf[256];
        size_t size;
        result.clear();

        do {
            size = read(buf, 256);
            result.append(buf, size);
        } while (size == 256);

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
        return m_io? io_putc((unsigned char) chr, m_io) != EOF: false;
    }
    /* Writes a NUL-terminated string and returns true on success, false on failure */
    bool putString(const char *str) {
        return m_io? io_puts(str, m_io) == 0: false;
    }
    /* Writes a string and returns true on success, false on failure */
    bool putString(const char *str, size_t len) {
        return m_io? io_write(str, 1, len, m_io) == len: false;
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
    bool slowCopyTo(IODevice &&out) {return slowCopyTo(out);}
    bool slowCopyTo(IODevice &out) {
        if (m_io && out.m_io) {
            while (1) {
                int ch = io_getc(m_io);
                if (ch == EOF)
                    break;

                if (io_putc(ch, out.m_io) == EOF)
                    return false;
            }

            return true;
        }

        return false;
    }

    bool copyTo(IODevice &&out) {return copyTo(out);}
    bool copyTo(IODevice &out) {
        return m_io && out.m_io? io_copy(m_io, out.m_io) == 0: false;
    }
    /* Reads from `in` and pushes all the data to this device, returning true on success, false on either read or write failure
     * The respective error flag on the failing device will be set
     */
    bool slowCopyFrom(IODevice &&in) {return slowCopyTo(in);}
    bool slowCopyFrom(IODevice &in) {
        if (m_io && in.m_io) {
            while (1) {
                int ch = io_getc(in.m_io);
                if (ch == EOF)
                    break;

                if (io_putc(ch, m_io) == EOF)
                    return false;
            }

            return true;
        }

        return false;
    }

    bool copyFrom(IODevice &&in) {return copyFrom(in);}
    bool copyFrom(IODevice &in) {
        return m_io && in.m_io? io_copy(in.m_io, m_io) == 0: false;
    }

    bool getPosition(IOPosition &position) const {
        return m_io? io_getpos(m_io, &position) == 0: false;
    }
    bool setPosition(IOPosition position) {
        return m_io? io_setpos(m_io, &position) == 0: false;
    }

    /* printf functions return -1 if a writing error occured, or the number of characters written otherwise */
    int vprintf(const char *fmt, va_list args) {
        int res = m_io? io_vprintf(m_io, fmt, args): -1;

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
        long long res = m_io? io_tell64(m_io): -1;
        if (res < 0)
            return false;

        offset = res;
        return true;
    }
    long long tell() const {return m_io? io_tell64(m_io): -1;}

    bool seek(long long offset, int origin) {
        return m_io? io_seek64(m_io, offset, origin) == 0: -1;
    }

    void rewind() {if (m_io) io_rewind(m_io);}

    /* Returns a character back to the input stream, as the next character to return
     * Returns true if the character was put back, false otherwise (the buffer may be full)
     * At least 1 level of ungetChar is supported()
     */
    bool ungetChar(char chr) {
        return m_io? io_ungetc((unsigned char) chr, m_io) != EOF: false;
    }

    operator bool() const {return !error() && !eof();}

protected:
    IO m_io;
};

class FileIO : public IODevice {
    std::string name;

protected:
    void closing() {}

public:
    FileIO() {}
    FileIO(IO_NATIVE_FILE_HANDLE native, const char *mode = "rwb") {tryOpen(open(native, mode));}
    FileIO(FILE *file) {tryOpen(open(file));}
    FileIO(const char *filename, const char *mode = "rb", bool native = true) {tryOpen(open(filename, mode, native));}

    int open(IO_NATIVE_FILE_HANDLE native, const char *mode = "rwb") {
        if (isOpen())
            return AlreadyOpen;

        m_io = io_open_native_file(native, mode);

        return m_io? 0: GenericError;
    }
    int open(FILE *file) {
        if (isOpen())
            return AlreadyOpen;

        m_io = io_open_file(file);

        return m_io? 0: GenericError;
    }
    int open(const char *filename, const char *mode = "rb", bool native = true) {
        if (isOpen())
            return AlreadyOpen;

        m_io = native? io_open(filename, mode): io_open_native(filename, mode);

        return m_io? 0: GenericError;
    }

    std::string filename() const {return name;}

    IO_NATIVE_FILE_HANDLE handle() const {
        if (m_io && (io_type(m_io) == IO_NativeFile || io_type(m_io) == IO_OwnNativeFile))
            return (IO_NATIVE_FILE_HANDLE) (uintmax_t) io_userdata(m_io);

        return IO_INVALID_FILE_HANDLE;
    }

    void grab() {if (m_io) io_grab_file(m_io);}
    void ungrab() {if (m_io) io_ungrab_file(m_io);}
};

class StringIO : public IODevice {
    std::string str;
    enum IO_Type type;
    size_t size;

protected:
    void closing() {
        char *ptr = m_io? io_underlying_buffer(m_io): NULL;
        FREE(ptr);
    }

public:
    StringIO() : type(IO_Empty), size(0) {}
    StringIO(const char *cstring, const char *mode = "rb") : type(IO_Empty), size(0) {tryOpen(open(cstring, mode));}
    StringIO(const char *buffer, size_t size, const char *mode = "rb") : type(IO_Empty), size(0) {
        if (strchr(mode, '+') || strchr(mode, 'w'))
            throw std::runtime_error("StringIO cannot write to const buffer");

        tryOpen(open(buffer, size, mode));
    }
    StringIO(char *buffer, size_t size, const char *mode = "r+b") : type(IO_Empty), size(0) {tryOpen(open(buffer, size, mode));}
    StringIO(std::string str, const char *mode = "rb") : str(str), type(IO_Empty), size(0) {tryOpen(open(this->str.data(), this->str.size(), mode));}
    StringIO(bool minimal, const char *mode = "wb") : type(IO_Empty), size(0) {tryOpen(open(minimal, mode));}

    int open(const char *cstring, const char *mode = "rb") {
        if (isOpen())
            return AlreadyOpen;

        m_io = io_open_cstring(cstring, mode);
        size = strlen(cstring);
        type = IO_CString;

        return m_io? 0: GenericError;
    }
    int open(const char *buffer, size_t size, const char *mode = "rb") {
        if (isOpen())
            return AlreadyOpen;

        if (strchr(mode, '+') || strchr(mode, 'w'))
            throw std::runtime_error("StringIO cannot write to const buffer");

        m_io = io_open_buffer(const_cast<char *>(buffer), size, mode);
        this->size = size;
        type = IO_SizedBuffer;

        return m_io? 0: GenericError;
    }
    int open(char *buffer, size_t size, const char *mode = "r+b") {
        if (isOpen())
            return AlreadyOpen;

        m_io = io_open_buffer(const_cast<char *>(buffer), size, mode);
        this->size = size;
        type = IO_SizedBuffer;

        return m_io? 0: GenericError;
    }
    int open(bool minimal, const char *mode = "wb") {
        if (isOpen())
            return AlreadyOpen;

        m_io = minimal? io_open_minimal_buffer(mode): io_open_dynamic_buffer(mode);
        type = minimal? IO_MinimalBuffer: IO_DynamicBuffer;

        return m_io? 0: GenericError;
    }

    const char *rawData() {
        switch (type) {
            case IO_CString:
            case IO_SizedBuffer: return m_io? static_cast<const char *>(io_userdata(m_io)): "";
            case IO_MinimalBuffer:
            case IO_DynamicBuffer: return m_io? io_underlying_buffer(m_io): "";
            default: return "";
        }
    }
    size_t length() {
        switch (type) {
            case IO_CString: return strlen(rawData());
            case IO_SizedBuffer: return size;
            case IO_MinimalBuffer:
            case IO_DynamicBuffer: return m_io? io_underlying_buffer_size(m_io): 0;
            default: return 0;
        }
    }
    size_t capacity() {
        switch (type) {
            case IO_MinimalBuffer:
            case IO_DynamicBuffer: return m_io? io_underlying_buffer_capacity(m_io): 0;
            default: return length();
        }
    }

    std::string data() {
        return {rawData(), length()};
    }
};

#endif

#endif // IO_H
