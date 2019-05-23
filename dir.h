#ifndef DIR_H
#define DIR_H

#include "platforms.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DirStruct;
struct DirEntryStruct;
struct PathStruct;

typedef struct DirStruct *Directory;
typedef struct DirEntryStruct *DirectoryEntry;
typedef struct PathStruct *Path;

/* Checks whether `c` is a valid path separator or not, returns non-zero if `c` is a separator, zero otherwise */
int path_check_separator(char c);
/* Returns the system-specific path separator */
char path_separator();
/* Edits `path` to point to parent directory, or if no parent directory exists, doesn't edit the path
 * Returns `path`; this function never fails
 */
Path path_up(Path path);
/* Normalizes `path` to remove '.' and '..' entries
 * Returns `path`; this function never fails
 */
Path path_normalize(Path path);
/* Returns the C-string representation of the path */
const char *path_str(Path path);
/* Constructs a new path pointing to a specified buffer */
Path path_construct_ref(char *pathBuffer);
/* Constructs a path from a series of path segments */
Path path_construct_gather(const char *args[], size_t segments);
/* Constructs a new path from a path and a name */
/* `name` may be NULL, in which case the path is duplicated */
Path path_construct(const char *path, const char *name);
/* Appends a name to a path, and returns the modified path, or NULL if out of memory */
/* The original path object is invalidated, even if NULL is returned */
/* Intended use case includes the following:
 *
 *  Path path_to_modify = ...;
 *
 *  ...
 *
 *  if ((path_to_modify = path_append(path_to_modify, "subdirectory")) == NULL)
 *      <error>;
 *
 */
Path path_append(Path path, const char *name);
/* Copies a path (with an optional name appended to it) and returns the copy, or NULL if out of memory */
Path path_copy(Path path, const char *name);
/* Destroys a Path object */
void path_destroy(Path path);

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
/* Returns path that open directory points to */
const char *dir_path(Directory dir);
/* Returns zero if no error occured while opening the directory, platform-specific non-zero value otherwise (errno for Linux, GetLastError for Windows) */
int dir_error(Directory dir);
/* Returns a pointer to the next available directory entry, or NULL if no more entries exist
 * This pointer MUST NOT be free()d!
 */
DirectoryEntry dir_next(Directory dir);
/* Closes a directory and invalidates the handle */
void dir_close(Directory dir);

/* Returns the full path of the directory entry, without the entry name */
const char *dirent_path(DirectoryEntry entry);
/* Returns the full path of the directory entry, including the entry name */
const char *dirent_fullname(DirectoryEntry entry);
/* Returns the NUL-terminated name of the directory entry */
const char *dirent_name(DirectoryEntry entry);
/* Returns the size (in bytes) of the directory entry, or -1 if no size could be determined */
long long dirent_size(DirectoryEntry entry);

/* Returns 0 if trait does not exist, non-zero if trait exists (some functions may always return 0 on some platforms) */
int dirent_is_archive(DirectoryEntry entry);
int dirent_is_compressed(DirectoryEntry entry);
int dirent_is_subdirectory(DirectoryEntry entry);
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
