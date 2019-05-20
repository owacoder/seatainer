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

/* Removes the last directory from the (either relative or absolute) path and returns `path` */
char *path_up(char *path) {
    char *npath = path;
    char *minAddr = path;

#if LINUX_OS
    if (*path != '/') /* Either a relative path, or a resource locator */ {
        minAddr = strchr(path, '/');
        if (minAddr == NULL || minAddr[-1] != ':')
            minAddr = path; /* Not a resource locator */
        else if (minAddr[1] == '/') /* Resource locator has two slashes (i.e. "smb://"), so look for the next slash */ {
            minAddr = strchr(minAddr + 2, '/');
            if (minAddr == NULL)
                return path;
        }
    }

    npath = path + strlen(path) - 1;
    while (npath != path && *npath == '/') /* Trailing '/' character(s), ignore as if not there */
        --npath;

    while (*npath && *npath != '/')
        --npath;

    /* Allow for proper resource locators (don't take the trailing '\' after "\\resource\") */
    if (npath <= minAddr && minAddr != path) {
        minAddr[1] = 0;
        return path;
    }

    if (npath == path && *path == '/') /* Traversed up to root, do not remove slash */
        npath[1] = 0;
    else /* Not root, so remove slash */
        npath[0] = 0;

    return path;
#elif WINDOWS_OS
    if (path[0] == '\\' && path[1] == '\\') {
        minAddr = strchr(path + 2, '\\');
        if (minAddr == NULL)
            return path;
    }

    npath = path + strlen(path) - 1;
    while (npath != path && *npath == '\\') /* Trailing '\' character(s), ignore as if not there */
        --npath;

    while (*npath && *npath != '\\')
        --npath;

    /* Allow for proper resource locators (don't take the trailing '\' after "\\resource\") */
    if (npath <= minAddr && minAddr != path) {
        /* Don't strip the name of resources of the form "\\.\name" (resources of this type won't have a trailing slash either) */
        if (path[2] != '.' || path[3] != '\\')
            minAddr[1] = 0;
        return path;
    }

    if (npath <= path + 2 && path[0] && path[1] == ':') /* Absolute drive path, cannot go up */
        path[2 + (path[2] == '\\')] = 0;
    else
        npath[0] = 0;
#endif

    return path;
}

/* Normalizes `path` to remove consecutive '/' or '\' characters, remove './' references, and remove '../' references */
char *path_norm(char *path) {
    char *save = path;

#if WINDOWS_OS
    const int pathSep = '\\';
    const int altPathSep = '/';
#else
    const int pathSep = '/';
    const int altPathSep = '\\';
#endif

    char *npath = path;

    /* Convert all path separators to proper form */
    for (; *npath; ++npath)
        if (*npath == altPathSep)
            *npath = pathSep;

    int unknownParent = 1;

    /* Remove consecutive separators */
    for (npath = path; *path; ) {
        if (path[0] == pathSep && path[1] == pathSep) { /* Consecutive separators */
#if LINUX_OS
            /* Allow two after a scheme for Linux resources */
            if (path != save && path[-1] == ':') {
                *npath++ = *path++;
                *npath++ = *path++;
                continue;
            }
#elif WINDOWS_OS
            /* Allow two to start with for Windows resources */
            if (path == save) {
                npath = path += 2;

                /* Allow for resources of the form "\\.\" */
                if (path[0] == '.' && path[1] == pathSep)
                    npath = path += 2;

                continue;
            }
#endif

            while (path[0] == pathSep && path[1] == pathSep)
                ++path;
        } else if ((path[0] == pathSep && path[1] == '.' && path[2] == '.' && (path[3] == 0 || path[3] == pathSep)) ||
                   ((path == save || path[-1] == pathSep) && path[0] == '.' && path[1] == '.' && path[2] == pathSep)) { /* Parent directory specifier */
            path += path[0] == pathSep;

            if (unknownParent) {
                memmove(npath, path, 3);
                npath += 3;
                path += 3;
                continue;
            }

            char *next = path + 3;
            npath[0] = 0;
            npath = path_up(save);

            npath += strlen(npath);
            if (npath != save && npath[-1] != pathSep && *next)
                *npath++ = pathSep;

            path = next;
        } else if ((path == save || path[-1] == pathSep) && path[0] == '.' && path[1] == pathSep) { /* Current directory specifier */
            while (path[0] == '.' && path[1] == pathSep)
                path += 2;
            while (*path == pathSep) /* Remove consecutive separators after directory specifier, if any */
                ++path;
        } else if (path[0] == pathSep && path[1] == '.' && (path[2] == 0 || path[2] == pathSep)) { /* Remove trailing current directory specifiers */
            while (path[0] == pathSep && path[1] == '.' && (path[2] == 0 || path[2] == pathSep))
                path += 2;
        } else {
            *npath++ = *path++;
            unknownParent = 0; /* We must have part of a valid name here, so parent references after this should be able to be dereferenced */
        }
    }

    if (npath != save && npath[-1] == pathSep)
        --npath;
    *npath = 0;

    return save;
}

#if LINUX_OS
struct DirEntryStruct {
    char *path; /* Points to path in DirStruct, doesn't have ownership */
    struct dirent data;
    struct stat extData;
    unsigned hasExtData; /* non-zero if extData is valid, zero if extData is not valid */
};

struct DirStruct {
    DIR *handle;
    char *path; /* Owned copy of path, plus capacity for directory separator, 256-byte name, and NUL-terminator */
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
};

int dirFillExtData(DirectoryEntry entry) {
    if (entry->hasExtData)
        return 0;

    size_t pathLen = strlen(entry->path);
    strcpy(entry->path + pathLen, dirent_name(entry));

    if (lstat(entry->path, &entry->extData)) {
        entry->path[pathLen] = 0;
        return -1;
    }

    entry->hasExtData = 1;
    entry->path[pathLen] = 0;
    return 0;
}
#elif WINDOWS_OS
struct DirStruct {
    HANDLE findFirstHandle;
    WIN32_FIND_DATAA findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
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
    Directory result = calloc(1, sizeof(*result));
    if (result == NULL)
        return NULL;

    size_t len = strlen(dir);
    char *path = malloc(len + 258);
    if (path == NULL) {
        free(result);
        return NULL;
    }

    memcpy(path, dir, len);
    if (len && path[len-1] != '/')
        path[len++] = '/';
    path[len] = 0;

    result->path = result->findData.path = result->lastFoundData.path = path;
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

    if (len > MAX_PATH)
        return NULL;

    memcpy(name, dir, len);

    if (len && name[len-1] != '\\' && name[len-1] != '/')
        name[len++] = '\\';

    name[len++] = '*';
    name[len] = 0;

    if (len > MAX_PATH)
        return NULL;

    Directory result = calloc(1, sizeof(*result));
    if (result == NULL)
        return NULL;

    result->findFirstHandle = FindFirstFileA(name, &result->findData);
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

    if (!FindNextFileA(dir->findFirstHandle, &dir->findData)) {
        FindClose(dir->findFirstHandle);
        dir->findFirstHandle = INVALID_HANDLE_VALUE;
    }

    return entry;
#else
    return NULL;
#endif
}

void dir_close(Directory dir) {
#if LINUX_OS
    if (dir->handle != NULL)
        closedir(dir->handle);

    free(dir->path);
#elif WINDOWS_OS
    if (dir->findFirstHandle != INVALID_HANDLE_VALUE)
        FindClose(dir->findFirstHandle);
#endif

    free(dir);
}

const char *dirent_name(DirectoryEntry entry) {
#if LINUX_OS
    return entry->data.d_name;
#elif WINDOWS_OS
    return entry->cFileName;
#else
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

    li.LowPart = entry->nFileSizeLow;
    li.HighPart = entry->nFileSizeHigh;

    return li.QuadPart;
#else
    return -1LL;
#endif
}

int dirent_is_archive(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE;
#else
    return 0;
#endif
}

int dirent_is_compressed(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED;
#else
    return 0;
#endif
}

int dirent_is_directory(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else
    if (dirFillExtData(entry))
        return 0;

    return S_ISDIR(entry->extData.st_mode);
#endif
}

int dirent_is_encrypted(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED;
#else
    return 0;
#endif
}

int dirent_is_hidden(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;
#elif LINUX_OS
    return entry->data.d_name[0] == '.' && strcmp(entry->data.d_name, ".") && strcmp(entry->data.d_name, "..");
#else
    return 0;
#endif
}

int dirent_is_normal(DirectoryEntry entry) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return 0;

    return S_ISREG(entry->extData.st_mode);
#elif WINDOWS_OS
    return entry->dwFileAttributes == FILE_ATTRIBUTE_NORMAL;
#else
    return 0;
#endif
}

int dirent_is_not_indexed(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
#else
    return 0;
#endif
}

int dirent_is_offline(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_OFFLINE;
#else
    return 0;
#endif
}

int dirent_is_readonly(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_READONLY;
#else
    return 0;
#endif
}

int dirent_is_sparse(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE;
#else
    return 0;
#endif
}

int dirent_is_system(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM;
#else
    return 0;
#endif
}

int dirent_is_temporary(DirectoryEntry entry) {
#if WINDOWS_OS
    return entry->dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY;
#else
    return 0;
#endif
}

int dirent_created_time(DirectoryEntry entry, struct tm *t) {
#if WINDOWS_OS
    SYSTEMTIME time;
    if (!FileTimeToSystemTime(&entry->ftCreationTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
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
    if (!FileTimeToSystemTime(&entry->ftLastAccessTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
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
    if (!FileTimeToSystemTime(&entry->ftLastWriteTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
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
    return -1;
#endif
}
