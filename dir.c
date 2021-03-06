/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "dir.h"

#include "seaerror.h"
#include "ccio.h"

#include <string.h>
#include <stdlib.h>

#if LINUX_OS
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#elif WINDOWS_OS
#include <windows.h>
/* Windows device names/paths are really hard to work with, see https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
 * The current status:
 *
 *  - Fully qualified DOS paths (e.g. C:\example.txt) will pass through fine, and support long filenames (greater than MAX_PATH). Users will never see the leading '\\?\'
 *  - Device paths (e.g. '\\.\COM1') will pass through fine, and the user will see the leading '\\.\'.
 *  - Network paths (e.g. '\\SERVER\Share\') will pass through fine, but the user will not see long paths (greater than MAX_PATH) in this form. Instead, they'll see a long paths in the form '\\?\UNC\SERVER\Share'.
 *  - Any other path is passed through as is, whether beginning with '\\?\', '\\.\', or a relative path. Native reserved device names, such as 'COM1', are not modified to begin with '\\.\'.
 */

#define LONG_PATH_PREFIX "\\\\?\\"
#define LONG_PATH_UNC_PREFIX "\\\\?\\UNC\\"
#define LONG_PATH_PREFIX_LEN strlen(LONG_PATH_PREFIX)
#define LONG_PATH_UNC_PREFIX_LEN strlen(LONG_PATH_UNC_PREFIX);
#define LONG_PATH_EXTRA_SPACE 8
#else
#define NATIVE_DIR 1
#endif

#if !NATIVE_DIR
#define NATIVE_DIR 0
#endif

#include "utility.h"

struct PathStruct {
    char *external;
    char path[];
};

int path_check_separator(char c) {
    return c == '\\' || c == '/';
}

char path_separator() {
#if WINDOWS_OS
    return '\\';
#else
    return '/';
#endif
}

static char *path_data(Path path) {
    return path->external? path->external: path->path;
}

const char *path_str(Path path) {
    return path_data(path);
}

Path path_construct_ref(char *pathBuffer) {
    Path result = MALLOC(sizeof(*result));
    if (!result)
        return NULL;

    result->external = pathBuffer;

    return result;
}

Path path_construct_gather(const char *args[], size_t segments) {
    size_t totalLen = segments? segments - 1: 0;

    for (size_t i = 0; i < segments; ++i)
        totalLen += strlen(args[i]);

    Path result = MALLOC(sizeof(*result) + totalLen + 1);
    if (!result)
        return NULL;
    result->external = NULL;

    char *ptr = result->path;
    size_t lastLength = 0, lastIndex = 0;

    for (size_t i = 0; i < segments; ++i) {
        size_t len = strlen(args[i]);
        if (i && len &&
                !path_check_separator(args[lastIndex][lastLength-1]) &&
                !path_check_separator(*args[i]))
            *ptr++ = path_separator();

        if (i && path_check_separator(args[lastIndex][lastLength-1]) &&
                 path_check_separator(*args[i]))
            --ptr;

        if (len) {
            lastIndex = i;
            lastLength = len;
        }

        memcpy(ptr, args[i], len);
        ptr += len;
    }

    *ptr = 0;

    return result;
}

Path path_construct(const char *path, const char *name) {
    size_t pathLen = strlen(path);
    size_t nameLen = name? strlen(name): 0;
    int add_separator = pathLen && !path_check_separator(path[pathLen-1]);

    Path result = MALLOC(sizeof(*result) + pathLen + add_separator + nameLen + 1);
    if (!result)
        return NULL;

    memcpy(result->path, path, pathLen);

    if (add_separator)
        result->path[pathLen++] = path_separator();

    if (name)
        memcpy(result->path + pathLen, name, nameLen);

    result->path[pathLen + nameLen] = 0;
    result->external = NULL;

    return result;
}

Path path_append(Path path, const char *name) {
    Path result = path_copy(path, name);
    path_destroy(path);
    return result;
}

Path path_copy(Path path, const char *name) {
    return path_construct(path_str(path), name);
}

void path_destroy(Path path) {
    FREE(path);
}

char *path_up_cstr(char *pathStr) {
    char *save = pathStr;
    char *npath = pathStr;
    char *minAddr = pathStr;

#if WINDOWS_OS
    if (str_starts_with(pathStr, LONG_PATH_PREFIX)) {
        pathStr += LONG_PATH_PREFIX_LEN;
        npath = pathStr;
        minAddr = pathStr;
    }

    if (pathStr[0] == '\\' && pathStr[1] == '\\') {
        minAddr = strchr(pathStr + 2, '\\');
        if (minAddr == NULL)
            return save;
    }

    npath = pathStr + strlen(pathStr) - 1;
    while (npath != pathStr && *npath == '\\') /* Trailing '\' character(s), ignore as if not there */
        --npath;

    while (*npath && *npath != '\\')
        --npath;

    /* Allow for proper resource locators (don't take the trailing '\' after "\\resource\") */
    if (npath <= minAddr && minAddr != pathStr) {
        /* Don't strip the name of resources of the form "\\.\name" (resources of this type won't have a trailing slash either) */
        if (pathStr[2] != '.' || pathStr[3] != '\\')
            minAddr[1] = 0;
        return save;
    }

    if (npath <= pathStr + 2 && pathStr[0] && pathStr[1] == ':') /* Absolute drive path, cannot go up */
        pathStr[2 + (pathStr[2] == '\\')] = 0;
    else
        npath[0] = 0;
#else
    if (*pathStr != '/') /* Either a relative path, or a resource locator */ {
        minAddr = strchr(pathStr, '/');
        if (minAddr == NULL || minAddr[-1] != ':')
            minAddr = pathStr; /* Not a resource locator */
        else if (minAddr[1] == '/') /* Resource locator has two slashes (i.e. "smb://"), so look for the next slash */ {
            minAddr = strchr(minAddr + 2, '/');
            if (minAddr == NULL)
                return save;
        }
    }

    npath = pathStr + strlen(pathStr) - 1;
    while (npath != pathStr && *npath == '/') /* Trailing '/' character(s), ignore as if not there */
        --npath;

    while (*npath && *npath != '/')
        --npath;

    /* Allow for proper resource locators (don't take the trailing '\' after "\\resource\") */
    if (npath <= minAddr && minAddr != pathStr) {
        minAddr[1] = 0;
        return save;
    }

    if (npath == pathStr && *pathStr == '/') /* Traversed up to root, do not remove slash */
        npath[1] = 0;
    else /* Not root, so remove slash */
        npath[0] = 0;
#endif

    return save;
}

/* Removes the last directory from the (either relative or absolute) path and returns `path` */
Path path_up(Path path) {
    path_up_cstr(path_data(path));
    return path;
}

/* Normalizes `path` to remove consecutive '/' or '\' characters, remove './' references, and remove '../' references */
char *path_normalize_cstr(char *pathStr) {
    char *save = pathStr;

#if WINDOWS_OS
    const int pathSep = '\\';
    const int altPathSep = '/';
#else
    const int pathSep = '/';
    const int altPathSep = '\\';
#endif

    char *npath = pathStr;

    /* Convert all path separators to proper form */
    for (; *npath; ++npath)
        if (*npath == altPathSep)
            *npath = pathSep;

#if WINDOWS_OS
    if (str_starts_with(pathStr, LONG_PATH_PREFIX)) {
        pathStr += LONG_PATH_PREFIX_LEN;
    }
#endif

    int unknownParent = 1;

    /* Remove consecutive separators */
    for (npath = pathStr; *pathStr; ) {
        if (pathStr[0] == pathSep && pathStr[1] == pathSep) { /* Consecutive separators */
#if LINUX_OS
            /* Allow two after a scheme for Linux resources */
            if (pathStr != save && pathStr[-1] == ':') {
                *npath++ = *pathStr++;
                *npath++ = *pathStr++;
                continue;
            }
#elif WINDOWS_OS
            /* Allow two to start with for Windows resources */
            if (pathStr == save) {
                npath = pathStr += 2;

                /* Allow for resources of the form "\\.\" */
                if (pathStr[0] == '.' && pathStr[1] == pathSep)
                    npath = pathStr += 2;

                continue;
            }
#endif

            while (pathStr[0] == pathSep && pathStr[1] == pathSep)
                ++pathStr;
        } else if ((pathStr[0] == pathSep && pathStr[1] == '.' && pathStr[2] == '.' && (pathStr[3] == 0 || pathStr[3] == pathSep)) ||
                   ((pathStr == save || pathStr[-1] == pathSep) && pathStr[0] == '.' && pathStr[1] == '.' && pathStr[2] == pathSep)) { /* Parent directory specifier */
            pathStr += pathStr[0] == pathSep;

            if (unknownParent) {
                memmove(npath, pathStr, 3);
                npath += 3;
                pathStr += 3;
                continue;
            }

            char *next = pathStr + 3;
            npath[0] = 0;
            npath = path_up_cstr(pathStr);

            npath += strlen(npath);
            if (npath != save && npath[-1] != pathSep && *next)
                *npath++ = pathSep;

            pathStr = next;
        } else if ((pathStr == save || pathStr[-1] == pathSep) && pathStr[0] == '.' && pathStr[1] == pathSep) { /* Current directory specifier */
            while (pathStr[0] == '.' && pathStr[1] == pathSep)
                pathStr += 2;
            while (*pathStr == pathSep) /* Remove consecutive separators after directory specifier, if any */
                ++pathStr;
        } else if (pathStr[0] == pathSep && pathStr[1] == '.' && (pathStr[2] == 0 || pathStr[2] == pathSep)) { /* Remove trailing current directory specifiers */
            while (pathStr[0] == pathSep && pathStr[1] == '.' && (pathStr[2] == 0 || pathStr[2] == pathSep))
                pathStr += 2;
        } else {
            *npath++ = *pathStr++;
            unknownParent = 0; /* We must have part of a valid name here, so parent references after this should be able to be dereferenced */
        }
    }

    if (npath != save && npath[-1] == pathSep) {
#if WINDOWS_OS
        if (npath - save != 3 || npath[-2] != ':')
#endif
            --npath;
    }
    *npath = 0;

    return save;
}

/* Normalizes the absolute or relative path and returns `path` */
Path path_normalize(Path path) {
    path_normalize_cstr(path_data(path));
    return path;
}

const char *path_name_cstr(char *pathStr) {
    size_t len = strlen(pathStr);
    size_t endpos = len, pos;

    for (; endpos > 0; --endpos) {
        if (!path_check_separator(pathStr[endpos-1]))
            break;
    }

    pathStr[endpos] = 0;

    for (pos = endpos; pos > 0; --pos) {
        if (path_check_separator(pathStr[pos-1]))
            return pathStr + pos;
    }

#if WINDOWS_OS
    if (endpos == 2 && len > endpos)
        pathStr[endpos++] = '\\';
    pathStr[endpos] = 0;
#endif

    return "";
}

const char *path_name(Path path) {
    return path_name_cstr(path_data(path));
}

const char *path_ext_cstr(char *path) {
    const char *name = path_name_cstr(path);
    char *find = strrchr(name, '.');

    if (find == NULL)
        return "";

#if LINUX_OS
    if (find == name) /* If file starts with '.', it's hidden, not an extension */
        return "";
#endif

    return find + 1;
}

const char *path_ext(Path path) {
    return path_ext_cstr(path_data(path));
}

#if WINDOWS_OS
int path_is_device_cstr(const char *pathStr) {
    return path_check_separator(pathStr[0]) && path_check_separator(pathStr[1]) && (pathStr[2] == '.' || pathStr[2] == '?');
}

int path_is_unc_cstr(const char *pathStr) {
    return path_check_separator(pathStr[0]) && path_check_separator(pathStr[1]) && pathStr[2] != '.' && pathStr[2] != '?';
}

int path_is_dos_path_cstr(const char *pathStr) {
    return isalpha(pathStr[0]) && pathStr[1] == ':' && path_check_separator(pathStr[2]);
}
#endif

int path_is_relative_cstr(const char *pathStr) {
#if WINDOWS_OS
    return !(pathStr[0] == path_separator() ||
            (pathStr[0] && pathStr[1] == ':' && pathStr[2] == path_separator()));
#else
    if (pathStr[0] == '/')
        return 0;

    const char *minAddr = strchr(pathStr, '/');
    if (minAddr == NULL || minAddr[-1] != ':')
        return 1;
    else if (minAddr[1] == '/') /* Resource locator has two slashes (i.e. "smb://"), so look for the next slash */
        return 0;
    else
        return 1;
#endif
}

int path_is_relative(Path path) {
    return path_is_relative_cstr(path_data(path));
}

Path path_get_current_working_dir() {
#if WINDOWS_OS
    DWORD size = GetCurrentDirectoryW(0, NULL);
    LPWSTR wbuf = MALLOC(size * sizeof(*wbuf));
    if (wbuf == NULL)
        return NULL;

    GetCurrentDirectoryW(size, wbuf);

    char *wd = wide_to_utf8_alloc(wbuf);
    FREE(wbuf);
    if (wd == NULL)
        return NULL;
#elif LINUX_OS
    char *wd = getcwd(NULL, 0);
    if (wd == NULL)
        return NULL;
#else
    size_t wd_size = 256;
    char *wd = MALLOC(wd_size);

    do {
        if (getcwd(wd, wd_size) != NULL)
            break;

        if (errno != ERANGE) {
            FREE(wd);
            return NULL;
        }

        wd_size *= 2;
        char *new_wd = REALLOC(wd, wd_size);
        if (new_wd == NULL) {
            FREE(wd);
            return NULL;
        }

        wd = new_wd;
    } while (1);
#endif

    Path p = path_construct(wd, NULL);
    FREE(wd);

    return p;
}

int path_set_current_working_dir_cstr(const char *path) {
#if WINDOWS_OS
    WCHAR *wide = utf8_to_wide_alloc(path);
    if (wide == NULL)
        return CC_ENOMEM;

    if (SetCurrentDirectoryW(wide) == 0)
        return GetLastError();

    FREE(wide);
    return 0;
#else
    return chdir(path);
#endif
}

int path_set_current_working_dir(Path path) {
    return path_set_current_working_dir_cstr(path_data(path));
}

struct SortedDirStruct {
    DirectoryEntry *entries;
    size_t next;
    size_t count;
    size_t capacity;
    size_t in_use;
};

static int sorted_dir_add_entry(struct SortedDirStruct *s, DirectoryEntry entry) {
    if (s->count == s->capacity) {
        size_t amount = MAX(16, s->count + (s->count >> 1));
        DirectoryEntry *d = REALLOC(s->entries, amount * sizeof(*d));
        if (d == NULL)
            return CC_ENOMEM;

        s->entries = d;
        s->capacity = amount;
    }

    s->entries[s->count++] = entry;
    return 0;
}

#if !NATIVE_DIR
/* See https://stackoverflow.com/a/46042467 */
#if WINDOWS_OS
#define DIR_QSORT(base, count, size, comparator, thunk) qsort_s((base), (count), (size), (comparator), (thunk))
#else
#define DIR_QSORT(base, count, size, comparator, thunk) qsort_r((base), (count), (size), (comparator), (thunk))
#endif

#ifdef WINDOWS_OS
static int sorted_dir_compare_names(void *thunk, const void *a, const void *b) {
#else
static int sorted_dir_compare_names(const void *a, const void *b, void *thunk) {
#endif
    DirectoryEntry lhs = *((DirectoryEntry *) a);
    DirectoryEntry rhs = *((DirectoryEntry *) b);

    enum DirectorySort sort = *((enum DirectorySort *) thunk);

    if (sort & DirSortFoldersBeforeFiles) {
        int cmp = !!dirent_is_directory(rhs) - !!dirent_is_directory(lhs);

        if (cmp)
            return cmp;
    }

#if WINDOWS_OS
    int result = strcmp_no_case(dirent_name(lhs), dirent_name(rhs));
#else
    int result = strcmp(dirent_name(lhs), dirent_name(rhs));
#endif

    return (sort & DirSortReversed)? -result: result;
}

#ifdef WINDOWS_OS
static int sorted_dir_compare_sizes(void *thunk, const void *a, const void *b) {
#else
static int sorted_dir_compare_sizes(const void *a, const void *b, void *thunk) {
#endif
    DirectoryEntry lhs = *((DirectoryEntry *) a);
    DirectoryEntry rhs = *((DirectoryEntry *) b);

    enum DirectorySort sort = *((enum DirectorySort *) thunk);

    if (sort & DirSortFoldersBeforeFiles) {
        int cmp = !!dirent_is_directory(rhs) - !!dirent_is_directory(lhs);

        if (cmp)
            return cmp;
    }

    long long result = dirent_size(lhs) - dirent_size(rhs);

    return (sort & DirSortReversed)? -SIGN(result): SIGN(result);
}

#if WINDOWS_OS
static int sorted_dir_compare_times(void *thunk, const void *a, const void *b) {
#else
static int sorted_dir_compare_times(const void *a, const void *b, void *thunk) {
#endif
    DirectoryEntry lhs = *((DirectoryEntry *) a);
    DirectoryEntry rhs = *((DirectoryEntry *) b);

    enum DirectorySort sort = *((enum DirectorySort *) thunk);

    if (sort & DirSortFoldersBeforeFiles) {
        int cmp = !!dirent_is_directory(rhs) - !!dirent_is_directory(lhs);

        if (cmp)
            return cmp;
    }

    time_t lhsTime = dirent_last_modification_time(lhs, NULL);
    time_t rhsTime = dirent_last_modification_time(rhs, NULL);
    int result = 0;

    if (lhsTime < rhsTime)
        result = -1;
    else if (rhsTime < lhsTime)
        result = 1;

    return (sort & DirSortReversed)? -SIGN(result): SIGN(result);
}

static void sorted_dir_sort(struct SortedDirStruct *s, enum DirectorySort sort) {
    switch (sort & DIR_SORT_TYPE_MASK) {
        default: break;
        case DirSortByName: DIR_QSORT(s->entries, s->count, sizeof(*s->entries), sorted_dir_compare_names, &sort); break;
        case DirSortBySize: DIR_QSORT(s->entries, s->count, sizeof(*s->entries), sorted_dir_compare_sizes, &sort); break;
        case DirSortByTime: DIR_QSORT(s->entries, s->count, sizeof(*s->entries), sorted_dir_compare_times, &sort); break;
    }
}

static void sorted_dir_free(struct SortedDirStruct *s) {
    if (s->entries) {
        for (size_t i = 0; i < s->count; ++i)
            dirent_close(s->entries[i]);
    }

    FREE(s->entries);
}
#endif /* NATIVE_DIR */

#if LINUX_OS
struct DirEntryStruct {
    Directory parent; /* Points to parent DirStruct, doesn't have ownership */
    struct dirent data; /* Only use name field if ownedDir is non-zero */
    struct stat extData;
    char hasExtData; /* non-zero if extData is valid, zero if extData is not valid */
    char ownedDir;   /* non-zero if this structure should be free()d.
                      * This will only be non-zero if the entry was created
                      * independently of a dir_next() call, i.e. using dirent_open()
                      * This is because a Directory object is always needed for a DirectoryEntry to exist
                      * If ownedDir is 0, do not use `struct dirent` member `data` (except for the name field), only use `extData` using `dirFillExtData` */
};

struct DirStruct {
    DIR *handle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    enum DirectoryFilter filter;
    struct SortedDirStruct sorted;
    size_t path_len;
    char path[]; /* Owned copy of path, plus capacity for directory separator, 256-byte name, and NUL-terminator */
};

int dirFillExtData(DirectoryEntry entry) {
    if (entry->hasExtData)
        return 0;

    if (lstat(dirent_fullname(entry), &entry->extData)) {
        entry->parent->error = errno;
        return -1;
    }

    entry->hasExtData = 1;
    return 0;
}
#elif WINDOWS_OS
struct DirEntryStruct {
    Directory parent; /* Points to parent DirStruct, doesn't have ownership */
    union {
        WIN32_FIND_DATAA data;
        WIN32_FIND_DATAW wdata;
    } fdata;
    char is_wide; /* Whether to use `data` (0) or `wdata` (1) */
    char ownedDir; /* non-zero if this structure should be free()d.
                    * This will only be non-zero if the entry was created
                    * independently of a dir_next() call, i.e. using dirent_open()
                    * This is because a Directory object is always needed for a DirectoryEntry to exist */
    char name[MAX_PATH * 4]; /* contains UTF-8 encoding of name if is_wide is true */
    LPWSTR wname; /* Contains wide encoding of name if non-NULL, owns copy of name */
};

struct DirStruct {
    HANDLE findFirstHandle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    enum DirectoryFilter filter;
    struct SortedDirStruct sorted;
    size_t path_len;
    size_t wpath_len;
    LPWSTR wpath_unc; /* Owned wide copy of path, prefixed with '\\?\', plus MAX_PATH space at end for name of file */
    char path[]; /* Owned copy of path */
};

void filetime_to_time_t(FILETIME *time, time_t *t) {
    ULARGE_INTEGER large;

    large.LowPart = time->dwLowDateTime;
    large.HighPart = time->dwHighDateTime;

    large.QuadPart /= 10000000; /* Convert from 100 nanosecond intervals to seconds */
    large.QuadPart -= 11644473600LL; /* Number of seconds between Jan 1, 1601 and Jan 1, 1970 */

    *t = (time_t) large.QuadPart;
}

void filetime_to_nsecs_since_epoch(FILETIME *time, long long *t) {
    ULARGE_INTEGER large;

    large.LowPart = time->dwLowDateTime;
    large.HighPart = time->dwHighDateTime;

    large.QuadPart -= 116444736000000000LL; /* Number of 100-nanosecond intervals between Jan 1, 1601 and Jan 1, 1970 */
    large.QuadPart *= 100; /* Convert from 100 nanosecond intervals to nanosecond intervals */

    *t = (long long) large.QuadPart;
}

#else
struct DirEntryStruct {
    Directory parent; /* Points to parent DirStruct, doesn't have ownership */
    char ownedDir; /* non-zero if this structure should be free()d.
                      * This will only be non-zero if the entry was created
                      * independently of a dir_next() call, i.e. using dirent_open()
                      * This is because a Directory object is always needed for a DirectoryEntry to exist */
    char name[256]; /* Name part only of filename */
};

struct DirStruct {
    int error; /* What error occurred last */
    size_t path_len; /* Length of folder structure, not including file name or deepest subfolder name */
    char path[]; /* Path name of directory entry */
};
#endif

static int dir_entry_present_with_filter(DirectoryEntry entry, enum DirectoryFilter filter) {
    if (((filter & DirFilterNoDot) && !strcmp(dirent_name(entry), ".")) ||
        ((filter & DirFilterNoDotDot) && !strcmp(dirent_name(entry), "..")) ||
        ((filter & DirFilterNoSymlinks) && dirent_is_symlink(entry)) ||
        (!(filter & DirFilterShowHidden) && dirent_is_hidden(entry)) ||
        (!(filter & DirFilterShowSystem) && dirent_is_system(entry)))
        return 0;

    return 1;
}

Directory dir_open(const char *dir, enum DirectoryFilter filter, enum DirectorySort sort) {
    return dir_open_with_mode(dir, "", filter, sort);
}

Directory dir_open_with_mode(const char *dir, const char *mode, enum DirectoryFilter filter, enum DirectorySort sort) {
    Directory result = NULL;

#if LINUX_OS
    UNUSED(mode)

    size_t len = strlen(dir);
    result = CALLOC(1, sizeof(*result) + len + 258);
    if (result == NULL)
        return NULL;

    /* Copy to new Directory object and ensure path ends with directory separator */
    memcpy(result->path, dir, len+1);
    len = strlen(path_normalize_cstr(result->path));

    if (len && result->path[len-1] != '/')
        result->path[len++] = '/';
    result->path[len] = 0;
    result->path_len = len;

    result->findData.parent = result->lastFoundData.parent = result;
    result->handle = opendir(dir);

    struct dirent *dEntry;
    if (result->handle) {
        result->error = 0;

        if ((dEntry = readdir(result->handle)) == NULL) {
            closedir(result->handle);
            result->handle = NULL;
        } else {
            result->findData.data = *dEntry;
            result->findData.hasExtData = 0;
        }
    } else {
        result->error = errno;
    }
#elif WINDOWS_OS
    const char *prefix = LONG_PATH_PREFIX;
    size_t prefix_len = 0;
    size_t len = strlen(dir);

    if (!path_is_device_cstr(dir)) {
        if (path_is_unc_cstr(dir) && len >= MAX_PATH) {
            prefix = LONG_PATH_UNC_PREFIX;
            prefix_len = LONG_PATH_UNC_PREFIX_LEN;
            dir += 2;
            len -= 2;
        } else if (path_is_dos_path_cstr(dir)) {
            prefix_len = LONG_PATH_PREFIX_LEN;
        }
    }

    result = CALLOC(1, sizeof(*result) + len + MAX_PATH + LONG_PATH_EXTRA_SPACE);
    if (result == NULL)
        return NULL;

    /* Copy to new Directory object and ensure path ends with directory separator */
    memcpy(result->path, prefix, prefix_len);
    memcpy(result->path + prefix_len, dir, len+1);
    len = strlen(path_normalize_cstr(result->path + prefix_len)) + prefix_len;

    /* Ensure path ends with directory separator */
    if (len && result->path[len-1] != '\\' && result->path[len-1] != '/')
        result->path[len++] = '\\';
    result->path[len] = 0;
    result->path_len = len;

    result->findData.parent = result->lastFoundData.parent = result;

    /* Finish adding glob to end of path */
    result->path[len++] = '*';
    result->path[len] = 0;

    if (strstr(mode, "@ncp") != NULL) {
        result->findFirstHandle = FindFirstFileA(result->path, &result->findData.fdata.data);

        result->findData.is_wide = result->lastFoundData.is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc_additional(result->path, MAX_PATH);
        if (!wide) {
            FREE(result);
            return NULL;
        }

        result->findFirstHandle = FindFirstFileW(wide, &result->findData.fdata.wdata);

        result->wpath_unc = wide;
        result->wpath_len = wcslen(wide) - 1; /* Remove '*' at end of path */
        result->findData.is_wide = result->lastFoundData.is_wide = 1;
    }

    if (result->findFirstHandle == INVALID_HANDLE_VALUE)
        result->error = GetLastError();
#else
    UNUSED(dir)
    UNUSED(mode)

    return NULL;
#endif

#if !NATIVE_DIR
    result->filter = filter;
    if (sort != DirSortNone && result && result->error == 0) {
        DirectoryEntry entry;
        struct SortedDirStruct sorted;

        memset(&sorted, 0, sizeof(sorted));
        sorted.in_use = 1;

        while ((entry = dir_next(result)) != NULL) {
            DirectoryEntry copy = dirent_copy(entry);
            if (!copy || sorted_dir_add_entry(&sorted, copy)) {
                sorted_dir_free(&sorted);
                dirent_close(copy);
                result->error = CC_ENOMEM;
                return result;
            }
        }

        sorted_dir_sort(&sorted, sort);

        result->sorted = sorted;
        result->filter = DirFilterNone; /* Since we already filtered, don't do it again */
    }

    return result;
#endif
}

int dir_error(Directory dir) {
    return dir->error;
}

void dir_clearerr(Directory dir) {
    dir->error = 0;
}

static DirectoryEntry dir_next_internal(Directory dir) {
#if !NATIVE_DIR
    if (dir->sorted.in_use) {
        if (dir->sorted.entries == NULL || dir->sorted.next == dir->sorted.count)
            return NULL;

        return dir->sorted.entries[dir->sorted.next++];
    }
#endif

#if LINUX_OS
    if (dir->handle == NULL)
        return NULL;

    dir->lastFoundData = dir->findData;
    DirectoryEntry entry = &dir->lastFoundData;

    struct dirent *dEntry;
    if ((dEntry = readdir(dir->handle)) == NULL) {
        closedir(dir->handle);
        dir->handle = NULL;
    } else {
        dir->findData.data = *dEntry;
        dir->findData.hasExtData = 0;
    }

    return entry;
#elif WINDOWS_OS
    if (dir->findFirstHandle == INVALID_HANDLE_VALUE)
        return NULL;

    dir->lastFoundData = dir->findData;
    DirectoryEntry entry = &dir->lastFoundData;

    if (dir->findData.is_wide) {
        if (!FindNextFileW(dir->findFirstHandle, &dir->findData.fdata.wdata)) {
            FindClose(dir->findFirstHandle);
            dir->findFirstHandle = INVALID_HANDLE_VALUE;
        }
    } else {
        if (!FindNextFileA(dir->findFirstHandle, &dir->findData.fdata.data)) {
            FindClose(dir->findFirstHandle);
            dir->findFirstHandle = INVALID_HANDLE_VALUE;
        }
    }

    return entry;
#else
    UNUSED(dir);

    return NULL;
#endif
}

DirectoryEntry dir_next(Directory dir) {
#if NATIVE_DIR
    UNUSED(dir)

    return NULL;
#else
    DirectoryEntry entry;

    while ((entry = dir_next_internal(dir)) != NULL) {
        if (!dir_entry_present_with_filter(entry, dir->filter))
            continue;

        return entry;
    }

    return entry;
#endif
}

const char *dir_path(Directory dir) {
#if WINDOWS_OS
    const char *path = dir_path_unc(dir);
    if (str_starts_with(path, LONG_PATH_PREFIX) && path_is_dos_path_cstr(path + LONG_PATH_PREFIX_LEN))
        path += LONG_PATH_PREFIX_LEN;

    return path;
#else
    dir->path[dir->path_len] = 0;
    return dir->path;
#endif
}

#if WINDOWS_OS
const char *dir_path_unc(Directory dir) {
    dir->path[dir->path_len] = 0;
    return dir->path;
}

LPCWSTR dir_path_wide(Directory dir) {
    LPCWSTR str = dir_path_wide_unc(dir);
    if (str && str_starts_with(dir->path, LONG_PATH_PREFIX) && path_is_dos_path_cstr(dir->path + LONG_PATH_PREFIX_LEN))
        str += LONG_PATH_PREFIX_LEN;

    return str;
}

LPCWSTR dir_path_wide_unc(Directory dir) {
    if (dir->wpath_unc) {
        dir->wpath_unc[dir->wpath_len] = 0;
        return dir->wpath_unc;
    }

    dir->wpath_unc = utf8_to_wide_alloc_additional(dir_path_unc(dir), MAX_PATH);
    if (dir->wpath_unc == NULL)
        return NULL;

    dir->wpath_len = wcslen(dir->wpath_unc);
    return dir->wpath_unc;
}
#endif

void dir_close(Directory dir) {
    if (dir == NULL)
        return;

#if !NATIVE_DIR
    sorted_dir_free(&dir->sorted);
#endif

#if LINUX_OS
    if (dir->handle != NULL)
        closedir(dir->handle);
#elif WINDOWS_OS
    if (dir->lastFoundData.wname)
        FREE(dir->lastFoundData.wname);

    if (dir->wpath_unc)
        FREE(dir->wpath_unc);

    if (dir->findFirstHandle != INVALID_HANDLE_VALUE)
        FindClose(dir->findFirstHandle);
#endif

    FREE(dir);
}

DirectoryEntry dirent_open(const char *path) {
    return dirent_open_with_mode(path, "");
}

DirectoryEntry dirent_open_with_mode(const char *path, const char *mode) {
    UNUSED(mode)

    size_t pathLen = strlen(path);
    DirectoryEntry entry = CALLOC(1, sizeof(*entry));
    Directory dir = NULL;
    if (!entry)
        return NULL;

#if LINUX_OS
    dir = CALLOC(1, sizeof(*dir) + pathLen + 1);
    if (!dir)
        goto cleanup;

    strcpy(dir->path, path);
    path_normalize_cstr(dir->path);

    const char *name = path_name_cstr(dir->path);

    /* Extract file/directory name from path and insert into status field */
    size_t name_len = strlen(name);

    if (name_len > 255)
        goto cleanup;
    strcpy(entry->data.d_name, name);
#elif WINDOWS_OS
    dir = CALLOC(1, sizeof(*dir) + pathLen + LONG_PATH_EXTRA_SPACE);
    if (!dir)
        goto cleanup;

    const char *prefix = LONG_PATH_PREFIX;
    size_t prefix_len = 0;
    size_t len = strlen(path);

    if (!path_is_device_cstr(path)) {
        if (path_is_unc_cstr(path) && len >= MAX_PATH) {
            prefix = LONG_PATH_UNC_PREFIX;
            prefix_len = LONG_PATH_UNC_PREFIX_LEN;
            path += 2;
            len -= 2;
        } else if (path_is_dos_path_cstr(path)) {
            prefix_len = LONG_PATH_PREFIX_LEN;
        }
    }

    memcpy(dir->path, prefix, prefix_len);
    memcpy(dir->path + prefix_len, path, len);
    path_normalize_cstr(dir->path);

    const char *name = path_name_cstr(dir->path);
    dir->path_len = name - dir->path;

    if (strstr(mode, "@ncp") != NULL) {
        dir->findFirstHandle = FindFirstFileA(dir->path, &entry->fdata.data);
        strcpy(entry->fdata.data.cFileName, name);

        entry->is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc_additional(dir->path, MAX_PATH);
        if (!wide)
            goto cleanup;

        LPWSTR wideName = utf8_to_wide_alloc(name);
        size_t wideNameLen = wcslen(wideName);
        if (!wideName || wideNameLen >= MAX_PATH) {
            FREE(wide);
            FREE(wideName);
            goto cleanup;
        }

        dir->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);
        dir->wpath_unc = wide;
        dir->wpath_len = wcslen(wide) - wideNameLen;
        if (dir->wpath_unc[dir->wpath_len-1] != path_separator()) {
            dir->wpath_unc[dir->wpath_len-1] = path_separator();
            dir->wpath_unc[dir->wpath_len++] = 0;
        } else
            dir->wpath_unc[dir->wpath_len] = 0;
        wcscpy(entry->fdata.wdata.cFileName, wideName);

        entry->wname = wideName;
        entry->is_wide = 1;
    }

    if (dir->findFirstHandle == INVALID_HANDLE_VALUE)
        dir->error = GetLastError();
#else
    strcpy(dir->path, path);
    path_normalize_cstr(dir->path);

    const char *name = path_name_cstr(dir->path);

    /* Extract file/directory name from path and insert into status field */
    size_t name_len = strlen(name);

    if (name_len > 255)
        goto cleanup;
    strcpy(entry->name, name);
#endif

    /* Then remove filename from path */
    path_up_cstr(dir->path);
    dir->path_len = strlen(dir->path);
    dir->path[dir->path_len++] = path_separator();
    dir->path[dir->path_len] = 0;

    entry->parent = dir;
    entry->ownedDir = 1;

    return entry;

cleanup:
    FREE(entry);
    FREE(dir);
    return NULL;
}

DirectoryEntry dirent_copy(DirectoryEntry entry) {
#if WINDOWS_OS
    return dirent_open_with_mode(dirent_fullname(entry), entry->is_wide? "": "@ncp");
#else
    return dirent_open(dirent_fullname(entry));
#endif
}

void dirent_close(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->wname)
        FREE(entry->wname);
#endif

    if (entry && entry->ownedDir) {
        dir_close(entry->parent);
        FREE(entry);
    }
}

int dirent_error(DirectoryEntry entry) {
    return entry->parent->error;
}

void dirent_clearerr(DirectoryEntry entry) {
    entry->parent->error = 0;
}

int dirent_refresh(DirectoryEntry entry) {
#if LINUX_OS
    entry->hasExtData = 0; /* Clear cache flag */
    return 0;
#elif WINDOWS_OS
    if (entry->ownedDir) { /* Can only refresh independently-created entries, not those tied to a Directory */
        entry->parent->error = 0;

        if (entry->parent->findFirstHandle != INVALID_HANDLE_VALUE)
            FindClose(entry->parent->findFirstHandle);

        if (!entry->is_wide) {
            entry->parent->findFirstHandle = FindFirstFileA(dirent_fullname(entry), &entry->fdata.data);
        } else {
            LPCWSTR wide = dirent_fullname_wide(entry);
            if (!wide)
                return entry->parent->error = CC_ENOMEM;

            entry->parent->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);
        }

        if (entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
            entry->parent->error = GetLastError();

        return entry->parent->error;
    } else
        return entry->parent->error = CC_EPERM;
#else
    UNUSED(entry)

    return 0;
#endif
}

const char *dirent_path(DirectoryEntry entry) {
    return dir_path(entry->parent);
}

#if WINDOWS_OS
const char *dirent_path_unc(DirectoryEntry entry) {
    return dir_path_unc(entry->parent);
}

LPCWSTR dirent_path_wide(DirectoryEntry entry) {
    return dir_path_wide(entry->parent);
}

LPCWSTR dirent_path_wide_unc(DirectoryEntry entry) {
    return dir_path_wide_unc(entry->parent);
}
#endif

const char *dirent_fullname(DirectoryEntry entry) {
#if WINDOWS_OS
    const char *path = dirent_fullname_unc(entry);
    if (str_starts_with(path, LONG_PATH_PREFIX) && path_is_dos_path_cstr(path + LONG_PATH_PREFIX_LEN))
        path += LONG_PATH_PREFIX_LEN;

    return path;
#else
    strcpy(entry->parent->path + entry->parent->path_len, dirent_name(entry));
    return entry->parent->path;
#endif
}

#if WINDOWS_OS
const char *dirent_fullname_unc(DirectoryEntry entry) {
    strcpy(entry->parent->path + entry->parent->path_len, dirent_name(entry));
    return entry->parent->path;
}

LPCWSTR dirent_fullname_wide(DirectoryEntry entry) {
    LPCWSTR str = dirent_fullname_wide_unc(entry);
    if (str && str_starts_with(dirent_fullname_unc(entry), LONG_PATH_PREFIX) && path_is_dos_path_cstr(dirent_fullname_unc(entry) + LONG_PATH_PREFIX_LEN))
        str += LONG_PATH_PREFIX_LEN;

    return str;
}

LPCWSTR dirent_fullname_wide_unc(DirectoryEntry entry) {
    LPWSTR wpath = (LPWSTR) dirent_path_wide_unc(entry);
    wcscpy(wpath + entry->parent->wpath_len, dirent_name_wide(entry));
    return wpath;
}
#endif

const char *dirent_name(DirectoryEntry entry) {
#if LINUX_OS
    return entry->data.d_name;
#elif WINDOWS_OS
    if (entry->is_wide) {
        WideCharToMultiByte(CP_UTF8, 0, entry->fdata.wdata.cFileName, -1, entry->name, sizeof(entry->name), NULL, NULL);
        return entry->name;
    }

    return entry->fdata.data.cFileName;
#else
    return entry->name;
#endif
}

#if WINDOWS_OS
LPCWSTR dirent_name_wide(DirectoryEntry entry) {
    if (entry->wname)
        return entry->wname;

    if (entry->is_wide)
        return entry->fdata.wdata.cFileName;

    return entry->wname = utf8_to_wide_alloc(dirent_name(entry));
}
#endif

long long dirent_size(DirectoryEntry entry) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1LL;

    return entry->extData.st_size;
#elif WINDOWS_OS
    LARGE_INTEGER li;

    entry->parent->error = 0;

    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        return -1LL;
    } else if (entry->is_wide) {
        li.LowPart = entry->fdata.wdata.nFileSizeLow;
        li.HighPart = entry->fdata.wdata.nFileSizeHigh;
    } else {
        li.LowPart = entry->fdata.data.nFileSizeLow;
        li.HighPart = entry->fdata.data.nFileSizeHigh;
    }

    return li.QuadPart;
#else
    errno = 0;
    FILE *file = fopen(dirent_fullname(entry), "r");
    long size = -1LL;

    if (file) {
        if (fseek(file, 0, SEEK_END) == 0)
            size = ftell(file);

        fclose(file);
    }

    if (size < 0)
        entry->parent->error = errno;
    return size;
#endif
}

int dirent_exists(DirectoryEntry entry) {
#if LINUX_OS
    return !dirFillExtData(entry);
#elif WINDOWS_OS
    if (entry->ownedDir)
        return entry->parent->findFirstHandle != INVALID_HANDLE_VALUE;

    return 1;
#else
    FILE *file = fopen(dirent_fullname(entry), "r");
    if (file) {
        fclose(file);
        return 1;
    }

    return 0;
#endif
}

int dirent_remove(DirectoryEntry entry, int recursive) {
#if LINUX_OS

#elif WINDOWS_OS
    if (dirent_is_directory(entry)) {
        int err = 0;

        if (!dirent_is_subdirectory(entry))
            return entry->parent->error = CC_ENOTSUP;

        if (recursive) {
            Directory dir = dir_open_with_mode(dirent_fullname(entry), entry->is_wide? "": "@ncp", DirFilterNone, DirSortNone);
            if (dir == NULL)
                return entry->parent->error = CC_ENOMEM;

            if (dir_error(dir)) {
                entry->parent->error = dir_error(dir);
                dir_close(dir);

                return entry->parent->error;
            }

            for (DirectoryEntry entry = dir_next(dir); entry; entry = dir_next(dir)) {
                if (dirent_is_directory(entry) && !dirent_is_subdirectory(entry))
                    continue;

                err = dirent_remove(entry, 1);
                if (err)
                    return entry->parent->error = err;
            }

            dir_close(dir);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilea
     *
     * To delete a read-only file, you must remove the read-only attribute.
     */
    if (dirent_is_readonly(entry)) {
        unsigned long attributes = dirent_get_attributes(entry);

        int err = dirent_set_attributes(entry, attributes & ~DIRENT_ATTRIBUTE_READONLY);
        if (err)
            return err;
    }

    if (entry->is_wide) {
        LPCWSTR wide = dirent_fullname_wide(entry);
        if (wide == NULL)
            return entry->parent->error = CC_ENOMEM;

        if (dirent_is_directory(entry)? !RemoveDirectoryW(wide): !DeleteFileW(wide))
            return entry->parent->error = GetLastError();

        return 0;
    } else {
        if (dirent_is_directory(entry)? !RemoveDirectoryA(dirent_fullname(entry)): !DeleteFileA(dirent_fullname(entry)))
            return entry->parent->error = GetLastError();

        return 0;
    }
#else
    UNUSED(recursive)
    if (remove(dirent_fullname(entry)))
        return errno;

    return 0;
#endif
}

int dirent_is_archive(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_compressed(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_actual_entry(DirectoryEntry entry) {
    return !dirent_is_directory(entry) || dirent_is_subdirectory(entry);
}

int dirent_is_subdirectory(DirectoryEntry entry) {
    if (!strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), ".."))
        return 0;

    return dirent_is_directory(entry);
}

int dirent_is_directory(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return !strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), "..");
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    }
#elif LINUX_OS
    if (dirFillExtData(entry))
        return !strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), "..");

    return S_ISDIR(entry->extData.st_mode);
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_encrypted(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_hidden(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
    }
#elif LINUX_OS
    return entry->data.d_name[0] == '.' && strcmp(entry->data.d_name, ".") && strcmp(entry->data.d_name, "..");
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_normal(DirectoryEntry entry) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return 0;

    return S_ISREG(entry->extData.st_mode);
#elif WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes == FILE_ATTRIBUTE_NORMAL;
    } else {
        return entry->fdata.data.dwFileAttributes == FILE_ATTRIBUTE_NORMAL;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_not_indexed(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_offline(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_readonly(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_READONLY;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_sparse(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_symlink(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
    }
#else
    UNUSED(entry)

    /* TODO: handle Unix/Linux symlinks */
    return 0;
#endif
}

int dirent_is_system(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_temporary(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

unsigned long dirent_get_attributes(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes;
    } else {
        return entry->fdata.data.dwFileAttributes;
    }
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_set_attributes(DirectoryEntry entry, unsigned long attributes) {
#if WINDOWS_OS
    LPCWSTR wide = NULL;

    if (entry->is_wide) {
        wide = dirent_fullname_wide(entry);
        if (wide == NULL)
            return entry->parent->error = CC_ENOMEM;
    }

    /* Change to directory */
    if (!!(attributes & DIRENT_ATTRIBUTE_DIRECTORY) != dirent_is_directory(entry))
        return entry->parent->error = CC_ENOTSUP;

    /* Change to symlink/reparse point */
    if (!!(attributes & DIRENT_ATTRIBUTE_SYMLINK) != dirent_is_symlink(entry))
        return entry->parent->error = CC_ENOTSUP;

    /* Change to compression or sparsity */
    if ((    attributes                   & (DIRENT_ATTRIBUTE_COMPRESSED | DIRENT_ATTRIBUTE_SPARSE)) !=
            (dirent_get_attributes(entry) & (DIRENT_ATTRIBUTE_COMPRESSED | DIRENT_ATTRIBUTE_SPARSE))) {
        HANDLE handle = entry->is_wide?
                    CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL):
                    CreateFileA(dirent_fullname(entry), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (handle == INVALID_HANDLE_VALUE)
            goto cleanup;

        /* Handle compression attribute */
        /* https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/77f650a3-e3a2-4a25-baac-4bf9b36bcc46 */
        DWORD bytesReturned;
        WORD type = !!(attributes & DIRENT_ATTRIBUTE_COMPRESSED);
        if (DeviceIoControl(handle, FSCTL_SET_COMPRESSION, &type, 2, NULL, 0, &bytesReturned, NULL) == 0) {
            CloseHandle(handle);
            goto cleanup;
        }

        /* Handle sparse attribute */
        /* https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/6a884fe5-3da1-4abb-84c4-f419d349d878 */
        BYTE sparse = !!(attributes & DIRENT_ATTRIBUTE_SPARSE);
        if (DeviceIoControl(handle, FSCTL_SET_SPARSE, &sparse, 1, NULL, 0, &bytesReturned, NULL) == 0) {
            CloseHandle(handle);
            goto cleanup;
        }

        if (CloseHandle(handle) == 0)
            goto cleanup;
    }

    /* Change to encryption */
    if (!!(attributes & DIRENT_ATTRIBUTE_ENCRYPTED) != dirent_is_encrypted(entry)) {
        if (attributes & DIRENT_ATTRIBUTE_ENCRYPTED) {
            if (0 == (entry->is_wide? EncryptFileW(wide): EncryptFileA(dirent_fullname(entry))))
                goto cleanup;
        } else {
            if (0 == (entry->is_wide? DecryptFileW(wide, 0): DecryptFileA(dirent_fullname(entry), 0)))
                goto cleanup;
        }
    }

    attributes &=
            DIRENT_ATTRIBUTE_ARCHIVE |
            DIRENT_ATTRIBUTE_HIDDEN |
            DIRENT_ATTRIBUTE_NORMAL |
            DIRENT_ATTRIBUTE_NOT_INDEXED |
            DIRENT_ATTRIBUTE_OFFLINE |
            DIRENT_ATTRIBUTE_READONLY |
            DIRENT_ATTRIBUTE_SYSTEM |
            DIRENT_ATTRIBUTE_TEMPORARY;

    if (0 == (entry->is_wide? SetFileAttributesW(wide, attributes): SetFileAttributesA(dirent_fullname(entry), attributes)))
        goto cleanup;

    if (entry->is_wide)
        entry->fdata.wdata.dwFileAttributes = attributes;
    else
        entry->fdata.data.dwFileAttributes = attributes;

    return 0;

cleanup:
    return entry->parent->error = GetLastError();
#else
    UNUSED(attributes)

    /* TODO: Linux attributes */
    return entry->parent->error = CC_ENOTSUP;
#endif
}

time_t dirent_created_time(DirectoryEntry entry, int *err) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftCreationTime: entry->fdata.data.ftCreationTime;
    time_t t;

    filetime_to_time_t(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

long long dirent_created_time_ns(DirectoryEntry entry, int *err) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftCreationTime: entry->fdata.data.ftCreationTime;
    long long t;

    filetime_to_nsecs_since_epoch(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

time_t dirent_last_access_time(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_atime;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftLastAccessTime: entry->fdata.data.ftLastAccessTime;
    time_t t;

    filetime_to_time_t(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

long long dirent_last_access_time_ns(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_atim.tv_sec * 1000000000 + entry->extData.st_atim.tv_nsec;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftLastAccessTime: entry->fdata.data.ftLastAccessTime;
    long long t;

    filetime_to_nsecs_since_epoch(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

time_t dirent_last_modification_time(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_mtime;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftLastWriteTime: entry->fdata.data.ftLastWriteTime;
    time_t t;

    filetime_to_time_t(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

long long dirent_last_modification_time_ns(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_mtim.tv_sec * 1000000000 + entry->extData.st_mtim.tv_nsec;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        if (err) *err = -1;
        return 0;
    }

    FILETIME time = entry->is_wide? entry->fdata.wdata.ftLastWriteTime: entry->fdata.data.ftLastWriteTime;
    long long t;

    filetime_to_nsecs_since_epoch(&time, &t);
    if (err) *err = 0;

    return t;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

time_t dirent_last_status_update_time(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_ctime;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}

long long dirent_last_status_update_time_ns(DirectoryEntry entry, int *err) {
#if LINUX_OS
    if (dirFillExtData(entry)) {
        if (err) *err = -1;
        return 0;
    }

    if (err) *err = 0;
    return entry->extData.st_ctim.tv_sec * 1000000000 + entry->extData.st_ctim.tv_nsec;
#else
    UNUSED(entry)
    UNUSED(err)

    entry->parent->error = CC_ENOTSUP;
    if (err) *err = -1;
    return 0;
#endif
}
