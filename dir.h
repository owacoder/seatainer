#ifndef DIR_H
#define DIR_H

#include "platforms.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX_OS
struct DirStruct;
struct DirEntryStruct;

typedef struct DirStruct *Directory;
typedef struct DirEntryStruct *DirectoryEntry;
#else
struct DirStruct;

typedef struct DirStruct *Directory;
typedef WIN32_FIND_DATAA *DirectoryEntry;
#endif

/* Edits `path` to point to parent directory, or if no parent directory exists, doesn't edit the path
 * Returns `path`
 */
char *path_up(char *path);
/* Normalizes `path` to remove '.' and '..' entries
 * Returns `path`
 */
char *path_norm(char *path);

/* Glob to see if string matches pattern
 *
 * Pattern can contain the following:
 *
 *     '?' - Matches any single character. The character must be present, even if the '?' is at the end of the pattern.
 *     '*' - Matches any sequence of zero or more characters. If pattern starts and ends with '*', a substring is searched for.
 *     '[' - Begins a character set to search for. Sets can be ranges '[0-9a-z]' or distinct sets '[CBV]', and can be negated if the first character is '^'
 *           You can search for a literal ']' by including that as the first character in a set (i.e. to search for ']', use '[]]'; to search for '[', use '[[]'; and to search for either '[' or ']', use '[][]')
 *           This is because searching for the empty set is not allowed.
 *     xxx - Any other character matches itself. It must be present.
 *
 * Returns 0 if `str` matches `pattern`, -1 if no match, and -2 if bad pattern
 */
int glob(const char *str, const char *pattern);

/* Opens a directory for access, or returns NULL on allocation failure */
Directory dir_open(const char *dir);
/* Returns zero if no error occured while opening the directory, platform-specific non-zero value otherwise (errno for Linux, GetLastError for Windows) */
int dir_error(Directory dir);
/* Returns a pointer to the next available directory entry, or NULL if no more entries exist
 * This pointer MUST NOT be free()d!
 */
DirectoryEntry dir_next(Directory dir);
/* Closes a directory and invalidates the handle */
void dir_close(Directory dir);

/* Returns the NUL-terminated name of the directory entry */
const char *dirent_name(DirectoryEntry entry);
/* Returns the size (in bytes) of the directory entry, or -1 if no size could be determined */
long long dirent_size(DirectoryEntry entry);

/* Returns 0 if trait does not exist, non-zero if trait exists (some functions may always return 0 on some platforms) */
int dirent_is_archive(DirectoryEntry entry);
int dirent_is_compressed(DirectoryEntry entry);
int dirent_is_directory(DirectoryEntry entry);
int dirent_is_encrypted(DirectoryEntry entry);
int dirent_is_hidden(DirectoryEntry entry);
int dirent_is_normal(DirectoryEntry entry);
int dirent_is_not_indexed(DirectoryEntry entry);
int dirent_is_offline(DirectoryEntry entry);
int dirent_is_readonly(DirectoryEntry entry);
int dirent_is_sparse(DirectoryEntry entry);
int dirent_is_system(DirectoryEntry entry);
int dirent_is_temporary(DirectoryEntry entry);

/* Obtain the UTC time of file traits */
/* Returns 0 on success, -1 on failure (failure may occur because the requested trait doesn't exist for the platform) */
int dirent_created_time(DirectoryEntry entry, struct tm *t);
int dirent_last_access_time(DirectoryEntry entry, struct tm *t);
int dirent_last_modification_time(DirectoryEntry entry, struct tm *t);
int dirent_last_status_update_time(DirectoryEntry entry, struct tm *t);

#ifdef __cplusplus
}
#endif

#endif // DIR_H
