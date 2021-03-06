/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#ifndef IO_CORE_H
#define IO_CORE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h> /* For io_ftime */

#include "../platforms.h"

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
 * Use of an IO device that references a FILE * will pass some calls on to the C Standard Library. Invariants held in the rest of the library may not apply.
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
 *   - `<` - Disable hardware acceleration. Currently only implemented for AES and SHA IO.
 *   - `g` - Grab ownership of handle when opening a native file descriptor.
 *   - `@ncp` - If specified in this order, the [n]ative [c]ode [p]age is used on Windows to open files instead of UTF-8.
 *
 * The C Standard Library seek() rules apply to this library as well. You must have a call to `io_seek()`, `io_seek64()`, or `io_rewind()` before switching between reading and writing.
 * The call `io_seek(device, 0, SEEK_CUR)` will only ever fail if an internal buffer failed to flush, even if the stream does not support seeking.
 * A call to `io_rewind()` also works for non-seekable streams, but has no way of reporting buffer-flush errors directly, although the error flag on the device will still be set.
 */
typedef struct InputOutputDevice *IO;

enum IO_Type
{
    IO_Empty,
    IO_File, /* External FILE * object */
    IO_OwnFile, /* Internally-created FILE * object */
    IO_NativeFile, /* External native file descriptor */
    IO_OwnNativeFile, /* Internally-created native file descriptor */
    IO_SizedBuffer, /* Size-limited buffer for reading and/or writing */
    IO_ThreadBuffer, /* Thread-safe dynamically sized (or statically sized, if set with setbuf()/setvbuf()) send/receive buffer, all data written will to it will be read later */
    IO_DynamicBuffer, /* Dynamic buffer with time-efficient buffer allocation, can release ownership of owned pointer */
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
 *         .state_switch = NULL,
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
     * The callback may set the `io` parameter's error code with `io_set_error()`, but this is not required. If no error is specified, a generic read error is set if `SIZE_MAX` is returned.
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
    IO_SimpleCallback state_switch;

    /** @brief Requests a clearing of all embedded IO objects.
     *
     * This callback must only call `io_clearerr()` on any embedded IO objects.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being acted on. Reads from or writes to the device are not allowed.
     */
    void (*clearerr)(void *userdata, IO io);

    /** @brief Requests shutdown of one or more device channels.
     *
     * This callback is a simple passthrough of `io_shutdown()` and any calls to `io_shutdown()` will fail with `CC_ENOTSUP` if this function is not defined.
     *
     * The callback must set the `io` parameter's error code with `io_set_error()`.
     *
     * @param The userdata stored in @p io.
     * @param io The IO device being acted on. Reads from or writes to the device are allowed to finalize the stream.
     * @param how Which channels to shutdown on the device, one of IO_SHUTDOWN_READ, IO_SHUTDOWN_WRITE, or IO_SHUTDOWN_READWRITE.
     * @return Zero on success, or non-zero on error.
     */
    int (*shutdown)(void *userdata, IO io, int how);

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
     * The seek change should be from the perspective of this device, not any underlying device.
     * For example, a hex encoder should seek on the underlying stream by twice the provided offset,
     * whereas a hex decoder should seek on the underlying stream by half the offset.
     *
     * If only one of `seek` or `seek64` are non-NULL, that overload will be used exclusively, with appropriate range handling.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device that the position is being changed on.
     * @return Zero on success, or non-zero if seeking is not available or an error occurred.
     */
    int (*seek)(void *userdata, long int offset, int origin, IO io);
    int (*seek64)(void *userdata, long long int offset, int origin, IO io);

    /** @brief Returns a constant integer specifying flags that this IO device type supports
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device that flags are being requested for.
     * @return An unsigned integer specifying which operations this IO device type supports
     */
    unsigned long (*flags)(void *userdata, IO io);

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
     * By convention, the returned string is normally lowercase.
     *
     * @param userdata The userdata stored in @p io.
     * @param io The IO device being closed. Reads from or writes to the device are allowed.
     * @return A machine-friendly string identifying the type of the IO device.
     */
    const char *(*what)(void *userdata, IO io);
};

/* Whether IO device is readable or not */
#define IO_FLAG_READABLE ((unsigned long) 0x01)
/* Whether IO device is writable or not */
#define IO_FLAG_WRITABLE ((unsigned long) 0x02)
/* Whether IO device is opened for update or not */
#define IO_FLAG_UPDATE ((unsigned long) 0x04)
/* Whether IO device is opened for append or not */
#define IO_FLAG_APPEND ((unsigned long) 0x08)
/* Whether IO device had an error */
#define IO_FLAG_ERROR ((unsigned long) 0x10)
/* Whether IO device reached the end of input */
#define IO_FLAG_EOF ((unsigned long) 0x20)
/* Whether IO device should fail if the file exists already */
#define IO_FLAG_FAIL_IF_EXISTS ((unsigned long) 0x40)
/* Whether IO device is in use (used for static-storage IO device allocation) */
#define IO_FLAG_IN_USE ((unsigned long) 0x100)
/* Whether IO device is dynamically allocated and should be freed */
#define IO_FLAG_DYNAMIC ((unsigned long) 0x200)
/* Whether IO device owns its vbuf */
#define IO_FLAG_OWNS_BUFFER ((unsigned long) 0x400)
/* Whether IO device was just read from */
#define IO_FLAG_HAS_JUST_READ ((unsigned long) 0x800)
/* Whether IO device was just written to */
#define IO_FLAG_HAS_JUST_WRITTEN ((unsigned long) 0x1000)
/* Whether IO device is opened for text or binary */
#define IO_FLAG_BINARY ((unsigned long) 0x2000)

#define IO_FLAG_RESET (IO_FLAG_READABLE | IO_FLAG_WRITABLE | IO_FLAG_UPDATE | IO_FLAG_APPEND | IO_FLAG_ERROR | IO_FLAG_EOF | IO_FLAG_HAS_JUST_READ | IO_FLAG_HAS_JUST_WRITTEN | IO_FLAG_BINARY)

/* Flags for devices to specify support for various modes of operation */
/* Whether IO device supports switching between reading and writing without an explicit state switch (e.g. `io_seek(io, 0, SEEK_CUR)`) */
#define IO_FLAG_SUPPORTS_NO_STATE_SWITCH ((unsigned long) 0x10000)

/* For Large File Support on Linux, the compile flag -D_FILE_OFFSET_BITS=64 must be used for
 * io_[seek/tell]64() functions to work with 64-bit offsets. io.c specifies these defines */

/** Define IO_DEFAULT_TEXT_MODE to default to text-mode reading and writing, and define
 *  IO_DEFAULT_BINARY_MODE to default to binary-mode reading and writing. The default if neither is specified
 *  is text mode, just like the C standard library.
 */
#if !defined(IO_DEFAULT_TEXT_MODE) && !defined(IO_DEFAULT_BINARY_MODE)
# define IO_DEFAULT_TEXT_MODE
#endif

/** @brief Returns an IO device that reads from stdin
 *
 * This IO device is a simple translation layer over the FILE *stdin
 * This IO device must not be closed by the user.
 *
 * @return An IO device that reads from stdin, or NULL if an error occurred.
 */
IO io_get_stdin(void);
#define io_stdin io_get_stdin()

/** @brief Returns an IO device that writes to stdout
 *
 * This IO device is a simple translation layer over the FILE *stdout
 * This IO device must not be closed by the user.
 *
 * @return An IO device that writes to stdout, or NULL if an error occurred.
 */
IO io_get_stdout(void);
#define io_stdout io_get_stdout()

/** @brief Returns an IO device that writes to stderr
 *
 * This IO device is a simple translation layer over the FILE *stderr
 * This IO device must not be closed by the user.
 *
 * @return An IO device that writes to stderr, or NULL if an error occurred.
 */
IO io_get_stderr(void);
#define io_stderr io_get_stderr()

/** @brief Clears any error or EOF flag present in an IO device.
 *
 * @param io The device to clear error flags on.
 */
void io_clearerr(IO io);

#if LINUX_OS
# include <sys/socket.h>
# define IO_SHUTDOWN_READ SHUT_RD
# define IO_SHUTDOWN_WRITE SHUT_WR
# define IO_SHUTDOWN_READWRITE SHUT_RDWR
#elif WINDOWS_OS
# define IO_SHUTDOWN_READ SD_RECEIVE
# define IO_SHUTDOWN_WRITE SD_SEND
# define IO_SHUTDOWN_READWRITE SD_BOTH
#else
# define IO_SHUTDOWN_READ 0
# define IO_SHUTDOWN_WRITE 1
# define IO_SHUTDOWN_READWRITE 2
#endif
/** @brief Shuts down read and/or write channels on a device.
 *
 * @param io The device to shutdown.
 * @param how Which channels to shutdown. One of IO_SHUTDOWN_READ, IO_SHUTDOWN_WRITE, or IO_SHUTDOWN_READWRITE
 * @return 0 on success, non-zero on failure. Call io_error() to get the actual error.
 */
int io_shutdown(IO io, int how);

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
unsigned long io_readable(IO io);

/** @brief Detect whether an IO device is writable or not.
 *
 * @param io The IO device on which to check write ability.
 * @return Non-zero if @p io is writable, zero otherwise.
 */
unsigned long io_writable(IO io);

/** @brief Returns the IO flags specified for a device. The flags are ORed together.
 *
 * @param io The IO device on which to get the flags.
 * @return A bit vector containing the flags currently set on the device. There is no way to set the flags on an IO device after it's opened.
 */
unsigned long io_flags(IO io);

/** @brief Detect whether the last operation performed on an IO device was a read operation.
 *
 * @param io The IO device on which to check the last operation.
 * @return Non-zero if @p io was just read from, zero otherwise.
 */
unsigned long io_just_read(IO io);

/** @brief Detect whether the last operation performed on an IO device was a write operation.
 *
 * @param io The IO device on which to check the last operation.
 * @return Non-zero if @p io was just written to, zero otherwise.
 */
unsigned long io_just_wrote(IO io);

/** @brief Detect whether an IO device is opened for update.
 *
 * Opening a stream for update does not clear the data that is already present, but allows writing.
 *
 * @param io The IO device to check the update status of.
 * @return Non-zero if @p io is opened for update.
 */
unsigned long io_opened_for_update(IO io);

/** @brief Detect whether an IO device is opened for append.
 *
 * Opening a stream for append does not clear the data that is already present, and positions the write point at the end.
 *
 * @param io The IO device to check the append status of.
 * @return Non-zero if @p io is opened for append.
 */
unsigned long io_opened_for_append(IO io);

/** @brief Detect whether an IO device is opened as a binary device.
 *
 * Binary devices pass all data through unmodified.
 *
 * @param io The IO device to check the binary/text status of.
 * @return Non-zero if @p io is a binary device.
 */
unsigned long io_binary(IO io);

/** @brief Detect whether an IO device is opened as a text device.
 *
 * Text devices convert the native line ending into a single '\\n' character when reading,
 * and convert a single '\\n' character into the native line ending when writing.
 *
 * @param io The IO device to check the binary/text status of.
 * @return Non-zero if @p io is a binary device.
 */
unsigned long io_text(IO io);

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
 */
void io_set_error(IO io, int err);

/** @brief Returns the state of the EOF flag on an IO device.
 *
 * @param io The IO device to get the EOF flag from.
 * @return Non-zero if the EOF flag is set, zero otherwise.
 */
unsigned long io_eof(IO io);

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

/** @brief Matches a string that should appear next in the input stream.
 *
 *  io_match() matches a NUL-terminated string and io_match_n() matches a sized string that can contain NUL characters.
 *
 *  @param io The IO device to match the next characters from.
 *  @param str The string to match to the next characters to be read from @p io.
 *  @return Returns 0 if the next characters in the input stream match @p str exactly. If they don't match, CC_EBADMSG is returned.
 *          If an input error occurs, the error that occurred is returned.
 */
int io_match(IO io, const char *str);
int io_match_n(IO io, const char *str, size_t len);

/** @brief Gets the current position of the IO device, if available.
 *
 * @param io The IO device to get the current read/write position from.
 * @param pos The location to save the current position in. This value must not be modified by the end user.
 * @return Zero on success, non-zero if an error occurred.
 */
int io_getpos(IO io, IO_Pos *pos);

/** @brief Reads up to @p num characters from @p io, stopping if a newline or EOF is reached.
 *
 * @param str The buffer to read into. This only needs to be as large as @p num, since only num-1 characters are written at most. The final character is a terminating NUL.
 * @param num The maximum number of characters to read. The maximum number of characters that will be consumed is equal to num-1.
 * @param io The IO device to read from.
 * @return Equal to @p str on success, NULL if an error occurred while reading or if no data was read.
 */
char *io_gets(char *str, int num, IO io);

/** @brief Opens a file with the provided mode using the C FILE object.
 *
 * @param filename The name of the local file to open.
 * @param mode The mode with which to open the file.
 * @return A new IO device referencing the opened file, or NULL if an error occurred.
 */
IO io_open(const char *filename, const char *mode);

/** @brief Opens a file with the provided mode natively, using OS-specific file descriptors
 *
 * @param filename The name of the local file to open.
 * @param mode The mode with which to open the file. If `mode` contains "@ncp" on Windows, the [n]ative [c]ode [p]age is used, instead of UTF-8
 * @return A new IO device referencing the opened native file, or NULL if an error occurred.
 */
IO io_open_native(const char *filename, const char *mode);

#if WINDOWS_OS
typedef HANDLE IONativeFileHandle;
#define IO_INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
typedef int IONativeFileHandle;
#define IO_INVALID_FILE_HANDLE (-1)
#endif

/** @brief Creates an IO device on top of a native OS file descriptor
 *
 * @param descriptor The native OS file descriptor to use for underlying IO.
 * @param mode The mode with which to open the file.
 * @return A new IO device referencing the opened native file IO device, or NULL if an error occurred.
 */
IO io_open_native_file(IONativeFileHandle descriptor, const char *mode);

/** @brief Creates an IO device on top of the C FILE object
 *
 * No mode can be specified because there is no portable way to determine what mode a FILE device was opened with. "rwb" is assumed.
 *
 * @param file The C FILE object to use for underlying IO
 * @return A new IO device referencing the opened native file IO device, or NULL if an error occurred.
 */
IO io_open_file(FILE *file);
IO io_open_empty(void); /* No input is read (i.e. the first read returns EOF) and all writes fail to this device */

/** @brief Creates an IO device on top of a C-style string.
 *
 * Writing to a C-style string device is not allowed and attempts to create a writable IO device will this function will fail.
 *
 * @param str The C-style string to use as the underlying buffer to read from. This buffer must be valid for the lifetime of the IO device as the data is not copied.
 * @param mode The mode with which to open the device. The mode cannot include writable attributes.
 * @return A new IO device referencing the opened C-style string device, or NULL if an error occurred.
 */
IO io_open_cstring(const char *str, const char *mode);

/** @brief Creates an IO device on top of a const buffer.
 *
 * Writing to a const buffer device is not allowed and attempts to create a writable IO device will this function will fail.
 *
 * @param str The const buffer to use as the underlying buffer to read from. This buffer must be valid for the lifetime of the IO device as the data is not copied.
 * @param mode The mode with which to open the device. The mode cannot include writable attributes.
 * @return A new IO device referencing the opened const buffer device, or NULL if an error occurred.
 */
IO io_open_const_buffer(const char *buf, size_t size, const char *mode);

/** @brief Creates an IO device on top of a writable statically-sized buffer.
 *
 * Writing to a statically-sized buffer succeeds unless the write would overrun the end of the buffer.
 *
 * @param str The buffer to use as the underlying buffer to read from. This buffer must be valid for the lifetime of the IO device as the data is not copied.
 * @param mode The mode with which to open the device.
 * @return A new IO device referencing the opened buffer device, or NULL if an error occurred.
 */
IO io_open_buffer(char *buf, size_t size, const char *mode);

/** @brief Creates a thread-buffer IO device.
 *
 * This is a input -> output buffer that is thread safe and can be used from multiple producers and consumers at once.
 * The size of the buffer can be set on construction, or allowed to grow unbounded dynamically.
 * Growing dynamically could be useful if you don't want to ever block on writes and it's not likely that the consumer would get behind.
 *
 * If the thread_buffer has a fixed size, writes will block until there is enough space to write the rest of the data.
 * If the thread_buffer has no size set, writes will never block and will grow the internal buffer accordingly.
 *
 * Reads will block until the data is available, or until EOF, which occurs when all the producers have closed their write connection to the thread_buffer with io_shutdown.
 *
 * @param buffer_size The fixed size of the underlying thread buffer. If set to 0, the size is allowed to grow dynamically.
 * @param initial_producers The initial number of producers (usually 1 per write thread) for this IO device.
 * @param initial_consumers The initial number of consumers (usually 1 per read thread) for this IO device.
 * @return A new IO device referencing the opened thread buffer device, or NULL if an error occurred.
 */
IO io_open_thread_buffer(size_t buffer_size, size_t initial_producers, size_t initial_consumers /* TODO: new parameter: int close_on_last_shutdown */);
IO io_open_dynamic_buffer(const char *mode);
IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode);
/** @brief Reads all data from `in` and pushes it to `out`, one character at a time.
 *
 * This function is designed to be used for testing purposes only.
 *
 * @param in The input stream to read data from
 * @param out The output stream to write data to
 * @return Returns 0 on success, any error that occured on failure. Detection of which stream failed is left up to the caller
 */
int io_slow_copy(IO in, IO out);
/** @brief Reads all data from `in` and pushes it to `out`.
 *
 * This function reads and writes more than one character at a time.
 *
 * @param in The input stream to read data from
 * @param out The output stream to write data to
 * @return Returns 0 on success, any error that occured on failure. Detection of which stream failed is left up to the caller
 */
int io_copy(IO in, IO out);
int io_copy_and_close(IO in, IO out);

/** @brief Print a number of arguments to an IO device using a specific format string.
 *
 * With the exception of wide strings, this function should perform identically to the standard
 * library functions in the `printf()` family. Wide strings are not supported.
 *
 * An extension of "%{}" is available to serialize a custom datatype too:
 *
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
 *
 * @param io The IO device to write to.
 * @param fmt The format string specifying what arguments to print.
 * @param args The va_list containing the arguments to print.
 * @return Returns the number of characters successfully written, -1 if a write error occurred, or -2 if the format string was formatted improperly.
 */
int io_vprintf(IO io, const char *fmt, va_list args);

/** @brief Print a number of arguments to an IO device using a specific format string.
 *
 * This function forwards the argument list on to io_vprintf().
 *
 * @param io The IO device to write to.
 * @param fmt The format string specifying what arguments to print.
 * @param ... A list of arguments to print.
 * @return Returns the number of characters successfully written, -1 if a write error occurred, or -2 if the format string was formatted improperly.
 */
int io_printf(IO io, const char *fmt, ...)
#if !WINDOWS_OS && (GCC_COMPILER | CLANG_COMPILER)
__attribute__((format(printf, 2, 3)))
#endif
;

/** @brief Formats a tm struct and writes it to an IO device using a specific format string.
 *
 * This function supports all of the same print specifiers that the C standard library function `strftime` supports in C99.
 *
 * @param io The IO device to write to.
 * @param fmt The format string specifying what to print.
 * @param timeptr A pointer to a tm struct that contains the date and/or time to print.
 * @return Returns the number of characters written on success, or a negative number on failure.
 */
int io_ftime(IO io, const char *fmt, const struct tm *timeptr);

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

/** @brief Efficiently duplicates a character to an IO device.
 *
 * If the stream is opened as a text stream, any newline (10) characters written to the stream will be converted to the native
 * newline as needed.
 *
 * @param ch The character to write, which will be casted to `unsigned char` internally.
 * @param count The number of characters to write. The number of bytes written may be greater than count if there are newlines that are converted.
 * @param io The IO device to write to.
 * @return Zero on success, or EOF if an error occurred.
 */
int io_putc_n(int ch, size_t count, IO io);

/** @brief Writes a value in binary format to an IO device.
 *
 * If the stream is opened as a text stream, the behavior is undefined.
 *
 * @param io The IO device to write to.
 * @param value The value to write to @p io.
 * @return 1 on success, 0 if an error occurred.
 */
size_t io_put_uint16_le(IO io, unsigned short value);
size_t io_put_uint16_be(IO io, unsigned short value);
size_t io_put_uint32_le(IO io, unsigned long value);
size_t io_put_uint32_be(IO io, unsigned long value);
size_t io_put_uint64_le(IO io, unsigned long long value);
size_t io_put_uint64_be(IO io, unsigned long long value);

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

/** @brief Sets a read timeout for an IO device
 *
 * This function only applies to platform-defined networking sockets.
 *
 * @param io The IO device that is being operated on.
 * @param usecs The number of microseconds to timeout after attempting a read.
 * @return 0 on success, an error code if something went wrong.
 */
int io_set_read_timeout(IO io, long long usecs);

/** @brief Sets a write timeout for an IO device
 *
 * This function only applies to platform-defined networking sockets.
 *
 * @param io The IO device that is being operated on.
 * @param usecs The number of microseconds to timeout after attempting a write.
 * @return 0 on success, an error code if something went wrong.
 */
int io_set_write_timeout(IO io, long long usecs);

/** @brief Returns the read timeout for the IO device
 *
 * This function only applies to platform-defined networking sockets.
 *
 * @param io The IO device that is being operated on.
 * @return The number of microseconds to timeout after attempting a read.
 */
long long io_read_timeout(IO io);

/** @brief Returns the read timeout for the IO device
 *
 * This function only applies to platform-defined networking sockets.
 *
 * @param io The IO device that is being operated on.
 * @return The number of microseconds to timeout after attempting a read.
 */
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

/** @brief Format a file size as a string and output it to an IO device
 *
 * @param io the IO device to output the human-readable file size to
 * @param size the file size to write
 * @return Zero on success, non-zero on failure. The error code will be stored in @p io
 */
int io_format_file_size(IO io, long long size);

#ifdef __cplusplus
} // extern "C"

#include <stdexcept>
#include <string>
#include <cstring>
#include <memory>
#include "seaerror.h"

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
    static const int AlreadyOpen = CC_EPERM;
    static const int CannotClose = CC_EPERM;
    static const int GenericError = CC_EIO;
    static const int NoMemory = CC_ENOMEM;

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
    int error() const {return m_io? io_error(m_io): CC_ENODEV;}

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
    FileIO(IONativeFileHandle native, const char *mode = "rwb") {tryOpen(open(native, mode));}
    FileIO(FILE *file) {tryOpen(open(file));}
    FileIO(const char *filename, const char *mode = "rb", bool native = true) {tryOpen(open(filename, mode, native));}

    int open(IONativeFileHandle native, const char *mode = "rwb") {
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

    IONativeFileHandle handle() const {
        if (m_io && (io_type(m_io) == IO_NativeFile || io_type(m_io) == IO_OwnNativeFile))
            return (IONativeFileHandle) (uintmax_t) io_userdata(m_io);

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

#endif // IO_CORE_H
