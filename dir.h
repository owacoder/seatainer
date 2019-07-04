/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

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

/** @brief The root object type to perform all directory operations on.
 *
 * A Directory object must be created to iterate over directories using `dir_next()`.
 *
 * This is an opaque handle to an object that does not need (or want) any member access.
 */
typedef struct DirStruct *Directory;
/** @brief The root object type to perform all file information operations on.
 *
 * A DirectoryEntry object is created for each call to `dir_next()`, but may also be created independently with `dirent_open()`.
 *
 * This is an opaque handle to an object that does not need (or want) any member access.
 */
typedef struct DirEntryStruct *DirectoryEntry;
/** @brief The root object type to perform all path operations on.
 *
 * A path object allows some convenient path manipulation functions that are difficult to accomplish with raw strings.
 *
 * This is an opaque handle to an object that does not need (or want) any member access.
 */
typedef struct PathStruct *Path;

/** @brief Checks whether @p c is a valid path separator.
 *
 *  @param c The character to check.
 *  @return 1 if @p c is a valid path separator, 0 otherwise.
 */
int path_check_separator(char c);

/** @brief Gives system-specific path separator.
 *
 * The system-specific separator defaults to '\' on Windows and '/' everywhere else.
 *
 * @return The system-specific path separator.
 */
char path_separator();
/* Edits `path` to point to parent directory, or if no parent directory exists, doesn't edit the path
 * Returns `path`; this function never fails
 */
Path path_up(Path path);
char *path_up_cstr(char *path);
/* Normalizes `path` to remove '.' and '..' entries
 * Returns `path`; this function never fails
 */
Path path_normalize(Path path);
char *path_normalize_cstr(char *path);
/* Returns the name-only portion of the string, i.e. the deepest subfolder or the filename */
const char *path_name(Path path);
const char *path_name_cstr(char *path);
/* Returns the extension of the filename, i.e. the last portion of the filename after the final '.' character */
const char *path_ext(Path path);
const char *path_ext_cstr(char *path);
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
 * Returns 0 if `str` matches `pattern`, -1 if no match, and -2 if bad pattern or pattern too complex
 */
int glob(const char *str, const char *pattern);

/** @brief Opens a directory for access.
 *
 * @param dir A UTF-8 string containing the name of the directory to open.
 * @return A new directory object, or NULL on allocation failure.
 */
Directory dir_open(const char *dir);

/** @brief Opens a directory for access.
 *
 * Same as dir_open(), but uses ANSI functions on Windows if mode contains "@ncp", for [n]ative [c]ode [p]age.
 *
 * @param dir A UTF-8 string containing the name of the directory to open.
 * @return A new directory object, or NULL on allocation failure.
 */
Directory dir_open_with_mode(const char *dir, const char *mode);

/** @brief Returns path that open directory points to.
 *
 * @param dir The opened directory to get the path from.
 * @return The UTF-8 path of @p dir, including the terminating path separator.
 */
const char *dir_path(Directory dir);

/** @brief Returns zero if no error occured while opening the directory, or an IO error otherwise
 *
 * @param dir The opened directory to check the error status of.
 * @return The error that occured while opening @p dir, or 0 on success.
 */
int dir_error(Directory dir);

/** @brief Returns a pointer to the next available directory entry.
 *
 * This pointer MUST NOT be stored permanently, as it references a temporary object contained in @p dir. Calling dirent_destroy() on it is valid but has no effect.
 *
 * @param dir The opened directory to get the next entry from.
 * @return The next available directory entry, or NULL if no more entries exist.
 */
DirectoryEntry dir_next(Directory dir);

/** @brief Closes a directory and invalidates the handle.
 *
 * @param dir The opened directory to be released.
 */
void dir_close(Directory dir);

/** @brief Opens a directory entry for a specific UTF-8 path.
 *
 * A Directory object is created automatically under the hood, since a DirectoryEntry cannot exist without a Directory.
 *
 * @param path The UTF-8 path of a file or directory to get the information of.
 * @return A new DirectoryEntry object that references the specified object, or NULL on allocation failure.
 */
DirectoryEntry dirent_open(const char *path);

/** @brief Opens a directory entry for a specific UTF-8 path.
 *
 * Same as dirent_open(), but uses ANSI functions on Windows if mode contains "@ncp", for [n]ative [c]ode [p]age.
 *
 * @param path The UTF-8 path of a file or directory to get the information of.
 * @return A new DirectoryEntry object that references the specified object, or NULL on allocation failure.
 */
DirectoryEntry dirent_open_with_mode(const char *path, const char *mode);

/** @brief Copies a directory entry so it can be saved easily.
 *
 * This function may be necessary if a user wants to store DirectoryEntry objects, but only have temporary objects returned from dir_next().
 * Copying the entry with this function allows it to be stored indefinitely.
 *
 * @param entry The directory entry to copy.
 * @return A new DirectoryEntry object, or NULL on allocation failure.
 */
DirectoryEntry dirent_copy(DirectoryEntry entry);

/** @brief Returns zero if no error occured while opening the directory entry, or an IO error otherwise
 *
 * @param dir The opened directory entry to check the error status of.
 * @return The error that occured while opening @p dir, or 0 on success.
 */
int dirent_error(DirectoryEntry entry);

/** @brief Closes a directory and invalidates the handle.
 *
 * @param dir The opened directory entry to be released.
 */
void dirent_close(DirectoryEntry entry);

/** @brief Returns the full UTF-8 path of the directory entry, without the entry name.
 *
 * @param entry The entry to get the path of.
 * @return The UTF-8 path of @p entry, including the trailing path separator.
 */
const char *dirent_path(DirectoryEntry entry);

/** @brief Returns the full UTF-8 path of the directory entry, including the entry name.
 *
 * @param entry The entry to get the path of.
 * @return The complete UTF-8 path of @p entry, including the name of the entry.
 */
const char *dirent_fullname(DirectoryEntry entry);

/** @brief Returns the UTF-8 name of the directory entry, without any part of the path.
 *
 * @param entry The entry to get the name of.
 * @return The UTF-8 name of @p entry. No part of the path is returned.
 */
const char *dirent_name(DirectoryEntry entry);

/** @brief Returns the size of the directory entry in bytes.
 *
 * @param entry The entry to get the size of.
 * @return The size of @p entry in bytes, or -1 if no size could be determined.
 */
long long dirent_size(DirectoryEntry entry);

/* Returns 0 if trait does not exist, non-zero if trait exists (some functions may always return 0 on some platforms) */
int dirent_is_actual_entry(DirectoryEntry entry);
int dirent_is_subdirectory(DirectoryEntry entry);
int dirent_is_directory(DirectoryEntry entry);
int dirent_is_normal(DirectoryEntry entry);

int dirent_is_archive(DirectoryEntry entry);
int dirent_is_compressed(DirectoryEntry entry);
int dirent_is_encrypted(DirectoryEntry entry);
int dirent_is_hidden(DirectoryEntry entry);
int dirent_is_not_indexed(DirectoryEntry entry);
int dirent_is_offline(DirectoryEntry entry);
int dirent_is_readonly(DirectoryEntry entry);
int dirent_is_sparse(DirectoryEntry entry);
int dirent_is_system(DirectoryEntry entry);
int dirent_is_temporary(DirectoryEntry entry);

/* Obtain the UTC time of file traits */
/* Returns 0 on success, -1 on failure (failure may occur because the requested trait doesn't exist for the platform) */
int dirent_created_time_utc(DirectoryEntry entry, struct tm *t);
int dirent_last_access_time_utc(DirectoryEntry entry, struct tm *t);
int dirent_last_modification_time_utc(DirectoryEntry entry, struct tm *t);
int dirent_last_status_update_time_utc(DirectoryEntry entry, struct tm *t);

#ifdef __cplusplus
}
#endif

#endif // DIR_H
