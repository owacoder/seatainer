#ifndef IO_H
#define IO_H

#include <stdio.h>

struct InputOutputDevice;

typedef struct InputOutputDevice *IO;

typedef enum
{
    IO_Empty,
    IO_File, /* External FILE * object */
    IO_OwnFile, /* Internally-created FILE * object */
    IO_CString, /* NUL-terminated C-style string */
    IO_SizedBuffer, /* Size-limited buffer for reading and/or writing */
    IO_Custom /* Custom callback for reading and/or writing */
} IOType;

typedef struct
{
    fpos_t _fpos;
    long long _pos;
} IO_pos;

/* A read callback should return SIZE_MAX if an error was encountered while reading, or a value less than size*count if EOF was reached */
typedef size_t (*IO_ReadCallback)(void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef size_t (*IO_WriteCallback)(const void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef int (*IO_SimpleCallback)(void *userdata, IO io);

struct InputOutputDeviceCallbacks {
    IO_ReadCallback read;
    IO_WriteCallback write;

    /* Userdata given to io_open_custom is passed as an argument to `open`, then the userdata in the IO object is set to the return value of this function.
     * If this function returns NULL, the call to open will fail and no IO device will be opened
     * If `open` itself is NULL, the userdata in the IO object is set to the userdata parameter of io_open_custom, and there is no way to prevent opening the IO device here
     */
    void *(*open)(void *userdata, IO io);
    IO_SimpleCallback close, flush;

    long int (*tell)(void *userdata, IO io);
    long long int (*tell64)(void *userdata, IO io);

    int (*seek)(void *userdata, long int offset, int origin, IO io);
    int (*seek64)(void *userdata, long long int offset, int origin, IO io);
};

/* For Large File Support on Linux, the compile flag -D_FILE_OFFSET_BITS=64 must be used for io_[seek/tell]64() functions to work with 64-bit offsets */

void io_clearerr(IO io);
int io_close(IO io);
int io_readable(IO io);
int io_writable(IO io);
/* Returns pointer to under-the-hood data
 * This pointer should *never* be freed
 */
void *io_userdata(IO io);
/* Returns pointer to object-specific raw buffer (the size is given by io_tempdata_size(), but is guaranteed to be >= 4 for Custom-type objects)
 * This buffer can be used for anything
 * If this function returns NULL, no buffer is available
 */
unsigned char *io_tempdata(IO io);
size_t io_tempdata_size(IO io);
int io_error(IO io);
int io_eof(IO io);
int io_flush(IO io);
int io_getc(IO io);
int io_getpos(IO io, IO_pos *pos);
char *io_gets(char *str, int num, IO io);
IO io_open(const char *filename, const char *mode);
IO io_open_file(FILE *file);
IO io_open_empty(void);
IO io_open_cstring(const char *str);
IO io_open_buffer(char *buf, size_t size, const char *mode);
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
int io_scanf(IO io, const char *fmt, ...);
int io_seek(IO io, long int offset, int origin);
int io_seek64(IO io, long long int offset, int origin);
int io_setpos(IO io, const IO_pos *pos);
long int io_tell(IO io);
long long int io_tell64(IO io);
size_t io_write(const void *ptr, size_t size, size_t count, IO io);
void io_rewind(IO io);
void io_setbuf(IO io, char *buf);
int io_setvbuf(IO io, char *buf, int mode, size_t size);
IO io_tmpfile(void);
int io_ungetc(int chr, IO io);

#endif // IO_H
