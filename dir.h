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

/** @brief Edits `path` to point to its parent directory, or if no parent directory exists, doesn't edit the path.
 *
 * @param path The path to traverse up one level from.
 * @return Returns @p path. This function never fails.
 */
Path path_up(Path path);

/** @brief Edits `path` to point to its parent directory, or if no parent directory exists, doesn't edit the path.
 *
 * @param path The path to traverse up one level from.
 * @return Returns @p path. This function never fails.
 */
char *path_up_cstr(char *path);

/** @brief Normalizes `path` to remove '.' and '..' entries, remove consecutive separator characters, and replace erroneous separators (like '/' vs '\\').
 *
 * @param path The path to normalize.
 * @return Returns `path`. This function never fails.
 */
Path path_normalize(Path path);

/** @brief Normalizes `path` to remove '.' and '..' entries, remove consecutive separator characters, and replace erroneous separators (like '/' vs '\\').
 *
 * @param path The path to normalize.
 * @return Returns `path`. This function never fails.
 */
char *path_normalize_cstr(char *path);

/** @brief Get the folder or filename.
 *
 * @param path The path to get the name from.
 * @return Returns the name-only portion of the string, i.e. the deepest subfolder or the filename
 */
const char *path_name(Path path);

/** @brief Get the folder or filename.
 *
 * @param path The path to get the name from.
 * @return Returns the name-only portion of the string, i.e. the deepest subfolder or the filename
 */
const char *path_name_cstr(char *path);

/** @brief Get the extension of the filename.
 *
 * If no extension exists, the result will be the empty string ("").
 *
 * @param path The path to get the name from.
 * @return Returns the extension of the filename, i.e. the last portion of the filename after the final '.' character
 */
const char *path_ext(Path path);

/** @brief Get the extension of the filename.
 *
 * If no extension exists, the result will be the empty string ("").
 *
 * @param path The path to get the name from.
 * @return Returns the extension of the filename, i.e. the last portion of the filename after the final '.' character
 */
const char *path_ext_cstr(char *path);

/** @brief Returns a UTF-8 string containing the path.
 *
 * @param path The path to get the string from.
 * @return A UTF-8 string containing the path represented by @p path.
 */
const char *path_str(Path path);

/** @brief Constructs a new path pointing to a specified buffer.
 *
 * @param pathBuffer The buffer to point the new path to.
 * @return A new path object referencing @p pathBuffer for its storage.
 */
Path path_construct_ref(char *pathBuffer);

/** @brief Constructs a path from a series of path segments.
 *
 * Segments may contain path separators, or none at all.
 * Path separators will automatically be inserted between segments if they don't already exist in a segment
 * (e.g. both "path", "nextPath" and "path/", "nextPath" become "path/nextPath" on Linux),
 * and segments that contain redundant separators will be reduced to a single separator
 * (e.g. "path/", "/nextPath" becomes "path/nextPath" on Linux).
 * Consecutive separators inside segments will not be reduced; this function does not normalize the path.
 *
 * @param args A list of strings to concatenate into a path.
 * @param segments The number of strings in @p args.
 * @return A new path containing a concatenation of all the path segments in @p args.
 */
Path path_construct_gather(const char *args[], size_t segments);

/** @brief Constructs a new path from a path and a name.
 *
 * A path separator is automatically inserted between the path and the name if needed.
 *
 * @param path The directory path for the new Path object. The path may be terminated with a path separator, but is not required to be.
 * @param name The file/directory name for the new Path object. This parameter may be NULL.
 * @return A new path containing a concatenation of @p path and @p name.
 *     If @p name is NULL, this function essentially copies @p path and ensures it ends with a path separator.
 */
Path path_construct(const char *path, const char *name);

/** @brief Appends a name to a path, and returns the modified path, or NULL if out of memory.
 *
 * The original path object is invalidated, even if NULL is returned
 * Intended use case includes the following:
 *
 *     Path path_to_modify = ...;
 *
 *     ...
 *
 *     if ((path_to_modify = path_append(path_to_modify, "subdirectory")) == NULL)
 *         <error>;
 *
 * @param path The path to append to.
 * @param name The file/directory name to append to @p path.
 * @return A new path containing the concatenation of @p path and @p name.
 */
Path path_append(Path path, const char *name);

/** @brief Copies a path, with an optional name appended to it.
 *
 * @param path The path to copy.
 * @param name An optional file/directory name to append to @p path while copying. Can be set to NULL if unused.
 * @return A new path that contains a copy of path, with @p name appended.
 */
Path path_copy(Path path, const char *name);

/** @brief Destroys a Path object and invalidates the handle
 *
 * @param path The path to destroy.
 */
void path_destroy(Path path);

/** @brief Returns the current working directory for this process.
 *
 * @return A new path that contains the current working directory for this process, or NULL on allocation failure.
 */
Path path_get_current_working_dir();

/** @brief Sets the current working directory for this process.
 *
 * @param path The path to set the current working directory to.
 * @return Returns 0 on success, error code on failure.
 */
int path_set_current_working_dir_cstr(const char *path);

/** @brief Sets the current working directory for this process.
 *
 * @param path The path to set the current working directory to.
 * @return Returns 0 on success, error code on failure.
 */
int path_set_current_working_dir(Path path);

#if 0
Directory dir_open(const char *dir);
Directory dir_open_with_mode(const char *dir, const char *mode);
DirectoryEntry dir_next(Directory dir);
const char *dir_path(Directory dir);
int dir_error(Directory dir);
void dir_clearerr(Directory dir);
void dir_close(Directory dir);

DirectoryEntry dirent_open(const char *entrypath);
DirectoryEntry dirent_open_with_mode(const char *entrypath, const char *mode);
const char *dirent_path(DirectoryEntry entry);
const char *dirent_name(DirectoryEntry entry);
const char *dirent_fullname(DirectoryEntry entry);
long long dirent_size(DirectoryEntry entry);
int dirent_exists(DirectoryEntry entry);
int dirent_error(DirectoryEntry entry);
void dirent_clearerr(DirectoryEntry entry);
void dirent_close(DirectoryEntry entry);
#endif

enum DirectorySort {
    DirSortNone = 0x00,
    DirSortByName = 0x01,
    DirSortByTime = 0x02,
    DirSortBySize = 0x04,
    DirSortReversed = 0x10,
    DirSortFoldersBeforeFiles = 0x20
};

enum DirectoryFilter {
    DirFilterNone = 0x00,
    DirFilterNoSymlinks = 0x008,
    DirFilterShowHidden = 0x100,
    DirFilterShowSystem = 0x200,
    DirFilterShowAll = DirFilterShowHidden | DirFilterShowSystem,
    DirFilterNoDot = 0x2000,
    DirFilterNoDotDot = 0x4000,
    DirFilterNoDotOrDotDot = DirFilterNoDot | DirFilterNoDotDot
};

#define DIR_SORT_TYPE_MASK 0xF

/** @brief Opens a directory for access.
 *
 * @param dir A UTF-8 string containing the name of the directory to open.
 * @param filter The specified filter to use when reading directory contents.
 * @param sort The specified sort flags to use when reading directory contents.
 * @return A new directory object, or NULL on allocation failure.
 */
Directory dir_open(const char *dir, enum DirectoryFilter filter, enum DirectorySort sort);

/** @brief Opens a directory for access.
 *
 * Same as dir_open(), but uses ANSI functions on Windows if mode contains "@ncp", for [n]ative [c]ode [p]age.
 *
 * @param dir A UTF-8 string containing the name of the directory to open.
 * @param filter The specified filter to use when reading directory contents.
 * @param sort The specified sort flags to use when reading directory contents.
 * @return A new directory object, or NULL on allocation failure.
 */
Directory dir_open_with_mode(const char *dir, const char *mode, enum DirectoryFilter filter, enum DirectorySort sort);

/** @brief Returns path that open directory points to.
 *
 * @param dir The opened directory to get the path from.
 * @return The UTF-8 path of @p dir, including the terminating path separator.
 */
const char *dir_path(Directory dir);
#if WINDOWS_OS
const char *dir_path_unc(Directory dir);
LPCWSTR dir_path_wide_unc(Directory dir);
LPCWSTR dir_path_wide(Directory dir);
#endif

/** @brief Returns zero if no error occured while opening the directory, or an IO error otherwise
 *
 * @param dir The opened directory to check the error status of.
 * @return The error that occured while opening @p dir, or 0 on success.
 */
int dir_error(Directory dir);

/** @brief Clears any error indicator in @p dir.
 *
 * @param dir The opened directory to clear the error in.
 */
void dir_clearerr(Directory dir);

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

/** @brief Clears any error indicator in @p entry.
 *
 * @param entry The opened directory entry to clear the error in.
 */
void dirent_clearerr(DirectoryEntry entry);

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
#if WINDOWS_OS
const char *dirent_path_unc(DirectoryEntry entry);
LPCWSTR dirent_path_wide(DirectoryEntry entry);
LPCWSTR dirent_path_wide_unc(DirectoryEntry entry);
#endif

/** @brief Returns the full UTF-8 path of the directory entry, including the entry name.
 *
 * @param entry The entry to get the path of.
 * @return The complete UTF-8 path of @p entry, including the name of the entry.
 */
const char *dirent_fullname(DirectoryEntry entry);
#if WINDOWS_OS
const char *dirent_fullname_unc(DirectoryEntry entry);
LPCWSTR dirent_fullname_wide(DirectoryEntry entry);
LPCWSTR dirent_fullname_wide_unc(DirectoryEntry entry);
#endif

/** @brief Returns the UTF-8 name of the directory entry, without any part of the path.
 *
 * @param entry The entry to get the name of.
 * @return The UTF-8 name of @p entry. No part of the path is returned.
 */
const char *dirent_name(DirectoryEntry entry);
#if WINDOWS_OS
LPCWSTR dirent_name_wide(DirectoryEntry entry);
#endif

/** @brief Returns the size of the directory entry in bytes.
 *
 * @param entry The entry to get the size of.
 * @return The size of @p entry in bytes, or -1 if no size could be determined.
 */
long long dirent_size(DirectoryEntry entry);

/** @brief Returns whether the directory entry exists or not.
 *
 * @param entry The entry to check the existence of.
 * @return 1 if the entry exists, 0 if it does not.
 */
int dirent_exists(DirectoryEntry entry);

/** @brief Attempts to refresh the internal cache of information about the entry.
 *
 * @param entry The entry to refresh the cache of.
 * @return 0 on success, or what error occured on failure.
 */
int dirent_refresh(DirectoryEntry entry);

/** @brief Attempts to delete the directory entry from disk.
 *
 * Attempts to delete directories with something in them will fail unless @p recursive is non-zero.
 *
 * @param entry The entry to delete, either a directory or a file on disk
 * @param recursive Whether to force deleting of a directory that still has contents.
 * @return 0 on success, or what error occured on failure.
 */
int dirent_remove(DirectoryEntry entry, int recursive);

/* Flags to describe the directory entry attributes */
#if WINDOWS_OS
#define DIRENT_ATTRIBUTE_ARCHIVE FILE_ATTRIBUTE_ARCHIVE
#define DIRENT_ATTRIBUTE_COMPRESSED FILE_ATTRIBUTE_COMPRESSED
#define DIRENT_ATTRIBUTE_DIRECTORY FILE_ATTRIBUTE_DIRECTORY
#define DIRENT_ATTRIBUTE_ENCRYPTED FILE_ATTRIBUTE_ENCRYPTED
#define DIRENT_ATTRIBUTE_HIDDEN FILE_ATTRIBUTE_HIDDEN
#define DIRENT_ATTRIBUTE_NOT_INDEXED FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
#define DIRENT_ATTRIBUTE_NORMAL FILE_ATTRIBUTE_NORMAL
#define DIRENT_ATTRIBUTE_OFFLINE FILE_ATTRIBUTE_OFFLINE
#define DIRENT_ATTRIBUTE_READONLY FILE_ATTRIBUTE_READONLY
#define DIRENT_ATTRIBUTE_SYMLINK FILE_ATTRIBUTE_REPARSE_POINT
#define DIRENT_ATTRIBUTE_SPARSE FILE_ATTRIBUTE_SPARSE_FILE
#define DIRENT_ATTRIBUTE_SYSTEM FILE_ATTRIBUTE_SYSTEM
#define DIRENT_ATTRIBUTE_TEMPORARY FILE_ATTRIBUTE_TEMPORARY
#endif

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
int dirent_is_symlink(DirectoryEntry entry);
int dirent_is_system(DirectoryEntry entry);
int dirent_is_temporary(DirectoryEntry entry);

unsigned long dirent_get_attributes(DirectoryEntry entry);
int dirent_set_attributes(DirectoryEntry entry, unsigned long attributes);

/* Obtain the local time of file traits */
/* err is 0 on success, -1 on failure (failure may occur because the requested trait doesn't exist for the platform) */
time_t dirent_created_time(DirectoryEntry entry, int *err);
long long dirent_created_time_ns(DirectoryEntry entry, int *err);
time_t dirent_last_access_time(DirectoryEntry entry, int *err);
long long dirent_last_access_time_ns(DirectoryEntry entry, int *err);
time_t dirent_last_modification_time(DirectoryEntry entry, int *err);
long long dirent_last_modification_time_ns(DirectoryEntry entry, int *err);
time_t dirent_last_status_update_time(DirectoryEntry entry, int *err);
long long dirent_last_status_update_time_ns(DirectoryEntry entry, int *err);

#ifdef __cplusplus
}
#endif

#endif // DIR_H
