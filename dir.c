#include "dir.h"

#include <string.h>
#include <errno.h>

#if LINUX_OS
#include <sys/stat.h>
#include <dirent.h>
#elif WINDOWS_OS
#include <windows.h>
#endif

/* TODO: currently recursive. Change to iterative version later? */
int glob(const char *str, const char *pattern) {
    for (; *pattern; ++pattern) {
        switch (*pattern) {
            case '?':
                if (*str++ == 0)
                    return -1;
                break;
            case '*': {
                while (*pattern == '*')
                    ++pattern;

                if (*pattern == 0) /* Anything matches if '*' is at end of pattern */
                    return 0;

                for (; *str; ++str) {
                    if (*pattern != '[' && *pattern != '?') { /* Pattern has specific char to match after star, so search for it */
                        str = strchr((char *) str, *pattern);
                        if (str == NULL)
                            return -1;
                        ++str;
                        ++pattern;
                    }

                    if (glob(str, pattern) == 0)
                        return 0;
                }

                while (*pattern == '*')
                    ++pattern;

                return *str == 0 && *pattern == 0? 0: -1;
            }
            case '[': {
                if (pattern[1] == 0 || pattern[2] == 0)
                    return -2;

                const char *lastCharInSet = pattern + 2;

                while (*lastCharInSet && (lastCharInSet[-1] == '-' || (lastCharInSet == pattern + 2 && lastCharInSet[-1] == '^') || *lastCharInSet != ']'))
                    ++lastCharInSet;

                if (*lastCharInSet != ']') /* Set not concluded properly */
                    return -2;

                --lastCharInSet;
                ++pattern;

                int negateSet = *pattern == '^';
                if (negateSet) {
                    if (pattern == lastCharInSet) /* Negated set with nothing in it isn't valid */
                        return -2;
                    ++pattern;
                }

                /* pattern now points to first char in set and lastCharInSet points to the last char in set */
                /* They may be pointing to the same char if it's a one-character set */
                if (pattern == lastCharInSet) {
                    if ((negateSet? *str++ == *pattern: *str++ != *pattern))
                        return -1;
                    pattern = lastCharInSet + 1;
                } else { /* Complex set, possibly negated */
                    int matched = negateSet; /* If matched is non-zero, the set matches */
                    unsigned char strChr = *str;

                    for (; pattern <= lastCharInSet; ++pattern) {
                        if (pattern[1] == '-') { /* Compute range */
                            int rangeLow = (unsigned char) pattern[0];
                            int rangeHigh = (unsigned char) pattern[2];

                            /* Swap range if backwards */
                            if (rangeHigh < rangeLow) {
                                int temp = rangeHigh;
                                rangeHigh = rangeLow;
                                rangeLow = temp;
                            }

                            if (rangeLow <= strChr && strChr <= rangeHigh) {
                                matched = !negateSet; /* Set to 1 if normal set, and 0 if negated */
                                break;
                            }

                            pattern += 2;
                        } else if (*str == *pattern) {
                            matched = !negateSet;
                            break;
                        }
                    }

                    if (!matched)
                        return -1;

                    ++str;
                    pattern = lastCharInSet + 1;
                }
                break;
            }
            default:
                if (*str++ != *pattern)
                    return -1;
                break;
        }
    }

    return *str == 0? 0: -1;
}

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
    Path result = malloc(sizeof(*result));
    if (!result)
        return NULL;

    result->external = pathBuffer;

    return result;
}

Path path_construct_gather(const char *args[], size_t segments) {
    size_t totalLen = segments? segments - 1: 0;

    for (size_t i = 0; i < segments; ++i)
        totalLen += strlen(args[i]);

    Path result = malloc(sizeof(*result) + totalLen + 1);
    if (!result)
        return NULL;
    result->external = NULL;

    char *ptr = result->path;
    size_t lastLength = 0;

    for (size_t i = 0; i < segments; ++i) {
        size_t len = strlen(args[i]);
        if (i && len &&
                !path_check_separator(args[i-1][lastLength-1]) &&
                !path_check_separator(*args[i]))
            *ptr++ = path_separator();
        memcpy(ptr, args[i], len);
        ptr += len;
        lastLength = len;
    }

    *ptr = 0;

    return result;
}

Path path_construct(const char *path, const char *name) {
    size_t pathLen = strlen(path);
    size_t nameLen = name? strlen(name): 0;
    int add_separator = pathLen && !path_check_separator(path[pathLen-1]);

    Path result = malloc(sizeof(*result) + pathLen + add_separator + nameLen + 1);
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
    free(path);
}

/* Removes the last directory from the (either relative or absolute) path and returns `path` */
Path path_up(Path path) {
    char *pathStr = path_data(path);
    char *npath = pathStr;
    char *minAddr = pathStr;

#if LINUX_OS
    if (*pathStr != '/') /* Either a relative path, or a resource locator */ {
        minAddr = strchr(pathStr, '/');
        if (minAddr == NULL || minAddr[-1] != ':')
            minAddr = pathStr; /* Not a resource locator */
        else if (minAddr[1] == '/') /* Resource locator has two slashes (i.e. "smb://"), so look for the next slash */ {
            minAddr = strchr(minAddr + 2, '/');
            if (minAddr == NULL)
                return pathStr;
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
        return pathStr;
    }

    if (npath == pathStr && *pathStr == '/') /* Traversed up to root, do not remove slash */
        npath[1] = 0;
    else /* Not root, so remove slash */
        npath[0] = 0;
#elif WINDOWS_OS
    if (pathStr[0] == '\\' && pathStr[1] == '\\') {
        minAddr = strchr(pathStr + 2, '\\');
        if (minAddr == NULL)
            return path;
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
        return path;
    }

    if (npath <= pathStr + 2 && pathStr[0] && pathStr[1] == ':') /* Absolute drive path, cannot go up */
        pathStr[2 + (pathStr[2] == '\\')] = 0;
    else
        npath[0] = 0;
#endif

    return path;
}

/* Normalizes `path` to remove consecutive '/' or '\' characters, remove './' references, and remove '../' references */
Path path_normalize(Path path) {
    char *pathStr = path_data(path);
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
            npath = path_data(path_up(path));

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

    return path;
}

#if LINUX_OS
struct DirEntryStruct {
    char *path; /* Points to path in DirStruct, doesn't have ownership */
    size_t path_len; /* Length of directory path in DirStruct */
    struct dirent data;
    struct stat extData;
    char hasExtData; /* non-zero if extData is valid, zero if extData is not valid */
    char shouldBeFreed; /* non-zero if this structure should be free()d */
};

struct DirStruct {
    DIR *handle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    size_t pathLen;
    char path[]; /* Owned copy of path, plus capacity for directory separator, 256-byte name, and NUL-terminator */
};

int dirFillExtData(DirectoryEntry entry) {
    if (entry->hasExtData)
        return 0;

    strcpy(entry->path + entry->path_len, dirent_name(entry));

    if (lstat(entry->path, &entry->extData)) {
        entry->path[entry->path_len] = 0;
        return -1;
    }

    entry->hasExtData = 1;
    entry->path[entry->path_len] = 0;
    return 0;
}
#elif WINDOWS_OS
struct DirEntryStruct {
    char *path; /* Points to path in DirStruct, doesn't have ownership */
    size_t path_len; /* Length of directory path in DirStruct */
    WIN32_FIND_DATAA data;
    char shouldBeFreed; /* non-zero if this structure should be free()d */
};

struct DirStruct {
    HANDLE findFirstHandle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    size_t path_len;
    char path[MAX_PATH + 1]; /* Owned copy of path */
};

void systemTimeToTm(LPSYSTEMTIME time, struct tm *t) {
    t->tm_isdst = 0;
    t->tm_year = time->wYear;
    t->tm_mon = time->wMonth - 1;
    t->tm_mday = time->wDay;
    t->tm_wday = time->wDayOfWeek;

    t->tm_hour = time->wHour;
    t->tm_min = time->wMinute;
    t->tm_sec = time->wSecond;
}
#endif

Directory dir_open(const char *dir) {
#if LINUX_OS
    size_t len = strlen(dir);
    Directory result = calloc(1, sizeof(*result) + len + 258);
    if (result == NULL)
        return NULL;

    /* Copy to new Directory object and ensure path ends with directory separator */
    memcpy(result->path, dir, len);
    if (len && result->path[len-1] != '/')
        result->path[len++] = '/';
    result->path[len] = 0;
    result->path_len = len;

    result->findData.path = result->lastFoundData.path = result->path;
    result->findData.path_len = result->lastFoundData.path_len = result->path_len;
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

    return result;
#elif WINDOWS_OS
    char name[MAX_PATH + 3];
    size_t len = strlen(dir);

    if (len > MAX_PATH - 2)
        return NULL;

    memcpy(name, dir, len);

    /* Ensure path ends with directory separator */
    if (len && name[len-1] != '\\' && name[len-1] != '/')
        name[len++] = '\\';
    name[len] = 0;

    Directory result = calloc(1, sizeof(*result));
    if (result == NULL)
        return NULL;

    /* Copy to new Directory object */
    strcpy(result->path, name);
    result->path_len = len;
    result->findData.path = result->lastFoundData.path = result->path;
    result->findData.path_len = result->lastFoundData.path_len = result->path_len;

    /* Finish adding glob to end of path */
    name[len++] = '*';
    name[len] = 0;

    result->findFirstHandle = FindFirstFileA(name, &result->findData.data);
    if (result->findFirstHandle == INVALID_HANDLE_VALUE)
        result->error = GetLastError();

    return result;
#else
    return NULL;
#endif
}

int dir_error(Directory dir) {
    return dir->error;
}

DirectoryEntry dir_next(Directory dir) {
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

    if (!FindNextFileA(dir->findFirstHandle, &dir->findData.data)) {
        FindClose(dir->findFirstHandle);
        dir->findFirstHandle = INVALID_HANDLE_VALUE;
    }

    return entry;
#else
    return NULL;
#endif
}

const char *dir_path(Directory dir) {
    dir->path[dir->path_len] = 0;
    return dir->path;
}

void dir_close(Directory dir) {
#if LINUX_OS
    if (dir->handle != NULL)
        closedir(dir->handle);
#elif WINDOWS_OS
    if (dir->findFirstHandle != INVALID_HANDLE_VALUE)
        FindClose(dir->findFirstHandle);
#endif

    free(dir);
}

const char *dirent_path(DirectoryEntry entry) {
    entry->path[entry->path_len] = 0;
    return entry->path;
}

const char *dirent_fullname(DirectoryEntry entry) {
    strcpy(entry->path + entry->path_len, dirent_name(entry));
    return entry->path;
}

const char *dirent_name(DirectoryEntry entry) {
#if LINUX_OS
    return entry->data.d_name;
#elif WINDOWS_OS
    return entry->data.cFileName;
#else
    UNUSED(entry)

    return "";
#endif
}

long long dirent_size(DirectoryEntry entry) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1LL;

    return entry->extData.st_size;
#elif WINDOWS_OS
    LARGE_INTEGER li;

    li.LowPart = entry->data.nFileSizeLow;
    li.HighPart = entry->data.nFileSizeHigh;

    return li.QuadPart;
#else
    UNUSED(entry)

    return -1LL;
#endif
}

int dirent_is_archive(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_compressed(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_subdirectory(DirectoryEntry entry) {
    if (!strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), ".."))
        return 0;

    return dirent_is_directory(entry);
}

int dirent_is_directory(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else
    if (dirFillExtData(entry))
        return 0;

    return S_ISDIR(entry->extData.st_mode);
#endif
}

int dirent_is_encrypted(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_hidden(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
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
    return entry->data.dwFileAttributes == FILE_ATTRIBUTE_NORMAL;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_not_indexed(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_offline(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_readonly(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_READONLY;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_sparse(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_system(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_is_temporary(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->data.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY;
#else
    UNUSED(entry)

    return 0;
#endif
}

int dirent_created_time(DirectoryEntry entry, struct tm *t) {
#if WINDOWS_OS
    SYSTEMTIME time;
    if (!FileTimeToSystemTime(&entry->data.ftCreationTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_access_time(DirectoryEntry entry, struct tm *t) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1;

    gmtime_r(&entry->extData.st_atime, t);

    return 0;
#elif WINDOWS_OS
    SYSTEMTIME time;
    if (!FileTimeToSystemTime(&entry->data.ftLastAccessTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_modification_time(DirectoryEntry entry, struct tm *t) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1;

    gmtime_r(&entry->extData.st_mtime, t);

    return 0;
#elif WINDOWS_OS
    SYSTEMTIME time;
    if (!FileTimeToSystemTime(&entry->data.ftLastWriteTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_status_update_time(DirectoryEntry entry, struct tm *t) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1;

    gmtime_r(&entry->extData.st_ctime, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}
