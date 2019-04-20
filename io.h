#ifndef IO_H
#define IO_H

#include <stdio.h>

/* **WARNING** - Only define CC_IO_STATIC_INSTANCES if you don't need thread safety */

struct InputOutputDevice;

typedef struct InputOutputDevice *IO;

enum IO_Type
{
    IO_Empty,
    IO_File, /* External FILE * object */
    IO_OwnFile, /* Internally-created FILE * object */
    IO_CString, /* NUL-terminated C-style string */
    IO_SizedBuffer, /* Size-limited buffer for reading and/or writing */
    IO_Custom /* Custom callback for reading and/or writing */
};

typedef struct
{
    fpos_t _fpos;
    long long _pos;
} IO_Pos;

enum IO_OpenHint {
    IO_HintStatic,
    IO_HintDynamic,

    IO_HintShortLived = IO_HintStatic,
    IO_HintLongLived = IO_HintDynamic
};

/* Hint the life-duration of the next open
 * (or if permanentHint is set, where to allocate all subsequent IO devices if possible, until the next call to this function)
 */
void io_hint_next_open(enum IO_OpenHint hint, int permanentHint);

/* A read callback should return SIZE_MAX if an error was encountered while reading, or a value less than size*count if EOF was reached */
typedef size_t (*IO_ReadCallback)(void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef size_t (*IO_WriteCallback)(const void *ptr, size_t size, size_t count, void *userdata, IO io);
typedef int (*IO_SimpleCallback)(void *userdata, IO io);

struct InputOutputDeviceCallbacks {
    IO_ReadCallback read;
    IO_WriteCallback write;

    void *(*open)(void *userdata, IO io);
    IO_SimpleCallback close, flush;

    long int (*tell)(void *userdata, IO io);
    long long int (*tell64)(void *userdata, IO io);

    int (*seek)(void *userdata, long int offset, int origin, IO io);
    int (*seek64)(void *userdata, long long int offset, int origin, IO io);
};

/* For Large File Support on Linux, the compile flag -D_FILE_OFFSET_BITS=64 must be used for
 * io_[seek/tell]64() functions to work with 64-bit offsets */

void io_clearerr(IO io);
int io_close(IO io);
int io_readable(IO io);
int io_writable(IO io);
/* Returns pointer to under-the-hood data
 * This pointer should *never* be freed
 */
void *io_userdata(IO io);
int io_error(IO io);
int io_eof(IO io);
int io_flush(IO io);
int io_getc(IO io);
int io_getpos(IO io, IO_Pos *pos);
char *io_gets(char *str, int num, IO io);
IO io_open(const char *filename, const char *mode);
IO io_open_file(FILE *file);
IO io_open_empty(void); /* No input is read (i.e. the first read returns EOF) and all writes fail to this device */
IO io_open_cstring(const char *str);
IO io_open_buffer(char *buf, size_t size, const char *mode);
IO io_open_custom(const struct InputOutputDeviceCallbacks *custom, void *userdata, const char *mode);
int io_vprintf(IO io, const char *fmt, va_list args);
int io_printf(IO io, const char *fmt, ...);
int io_putc(int ch, IO io);
int io_puts(const char *str, IO io);
size_t io_read(void *ptr, size_t size, size_t count, IO io);
IO io_reopen(const char *filename, const char *mode, IO io);
int io_scanf(IO io, const char *fmt, ...);
int io_seek(IO io, long int offset, int origin);
int io_seek64(IO io, long long int offset, int origin);
int io_setpos(IO io, const IO_Pos *pos);
long int io_tell(IO io);
long long int io_tell64(IO io);
size_t io_write(const void *ptr, size_t size, size_t count, IO io);
void io_rewind(IO io);
void io_setbuf(IO io, char *buf);
int io_setvbuf(IO io, char *buf, int mode, size_t size);
IO io_tmpfile(void);
int io_ungetc(int chr, IO io);

#endif // IO_H
