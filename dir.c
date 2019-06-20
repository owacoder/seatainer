/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2019
 */

#include "dir.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#if LINUX_OS
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#elif WINDOWS_OS
#include <windows.h>
#endif

#include "utility.h"

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

char *path_up_cstr(char *pathStr) {
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
            return pathStr;
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
        return pathStr;
    }

    if (npath <= pathStr + 2 && pathStr[0] && pathStr[1] == ':') /* Absolute drive path, cannot go up */
        pathStr[2 + (pathStr[2] == '\\')] = 0;
    else
        npath[0] = 0;
#endif

    return pathStr;
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

#if LINUX_OS
struct DirEntryStruct {
    char *path; /* Points to path in DirStruct, doesn't have ownership */
    size_t path_len; /* Length of directory path in DirStruct */
    struct dirent data; /* Only use name field if ownedDir is non-zero */
    struct stat extData;
    char hasExtData; /* non-zero if extData is valid, zero if extData is not valid */
    Directory ownedDir;  /* non-zero if this structure should be free()d.
                          * This will only be non-zero if the entry was created
                          * independently of a dir_next() call, i.e. using dirent_open()
                          * This is because a Directory object is always needed for a DirectoryEntry to exist
                          * If ownedDir is NULL, do not use `struct dirent` member `data` (except for the name field), only use `extData` using `dirFillExtData` */
};

struct DirStruct {
    DIR *handle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    size_t path_len;
    char path[]; /* Owned copy of path, plus capacity for directory separator, 256-byte name, and NUL-terminator */
};

int dirFillExtData(DirectoryEntry entry) {
    if (entry->hasExtData)
        return 0;

    if (lstat(dirent_fullname(entry), &entry->extData))
        return -1;

    entry->hasExtData = 1;
    return 0;
}
#elif WINDOWS_OS
struct DirEntryStruct {
    char *path; /* Points to path in DirStruct, doesn't have ownership */
    size_t path_len; /* Length of directory path in DirStruct */
    int is_wide; /* Whether to use `data` (0) or `wdata` (1) */
    union {
        WIN32_FIND_DATAA data; /* Only use name field if ownedDir is non-zero */
        WIN32_FIND_DATAW wdata; /* Only use name field if ownedDir is non-zero */
    } fdata;
    Directory ownedDir; /* non-zero if this structure should be free()d.
                         * This will only be non-zero if the entry was created
                         * independently of a dir_next() call, i.e. using dirent_open()
                         * This is because a Directory object is always needed for a DirectoryEntry to exist */
    char name[MAX_PATH * 6 + 1]; /* contains UTF-8 encoding of name if is_wide is true */
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
    return dir_open_with_mode(dir, "");
}

Directory dir_open_with_mode(const char *dir, const char *mode) {
#if LINUX_OS
    UNUSED(mode)

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
    result->findData.ownedDir = result->lastFoundData.ownedDir = NULL;
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
    result->findData.ownedDir = result->lastFoundData.ownedDir = NULL;

    /* Finish adding glob to end of path */
    name[len++] = '*';
    name[len] = 0;

    if (strstr(mode, "@ncp") != NULL) {
        result->findFirstHandle = FindFirstFileA(name, &result->findData.fdata.data);

        result->findData.is_wide = result->lastFoundData.is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc(name);
        if (!wide) {
            free(result);
            return NULL;
        }

        result->findFirstHandle = FindFirstFileW(wide, &result->findData.fdata.wdata);

        free(wide);

        result->findData.is_wide = result->lastFoundData.is_wide = 1;
    }

    if (result->findFirstHandle == INVALID_HANDLE_VALUE)
        result->error = GetLastError();

    return result;
#else
    UNUSED(dir)
    UNUSED(mode)

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

DirectoryEntry dirent_open(const char *path) {
    return dirent_open_with_mode(path, "");
}

DirectoryEntry dirent_open_with_mode(const char *path, const char *mode) {
    UNUSED(mode)

    size_t pathLen = strlen(path);
    DirectoryEntry entry = calloc(1, sizeof(*entry));
    Directory dir = NULL;
    if (!entry)
        return NULL;

#if LINUX_OS
    dir = calloc(1, sizeof(*dir) + pathLen + 1);
    if (!dir)
        goto cleanup;
#else
    if (pathLen > MAX_PATH)
        goto cleanup;

    dir = calloc(1, sizeof(*dir));
    if (!dir)
        goto cleanup;
#endif

    strcpy(dir->path, path);

    const char *name = path_name_cstr(dir->path);

    /* Extract file/directory name from path and insert into status field */
    size_t name_len = strlen(name);

#if LINUX_OS
    if (name_len > 255)
        goto cleanup;
    strcpy(entry->data.d_name, name);
#elif WINDOWS_OS
    if (strstr(mode, "@ncp") != NULL) {
        if (name_len > MAX_PATH - 1)
            goto cleanup;

        strcpy(entry->fdata.data.cFileName, name);
        dir->findFirstHandle = FindFirstFileA(dirent_fullname(entry), &entry->fdata.data);

        entry->is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc(dir->path);
        if (!wide)
            goto cleanup;

        LPWSTR wideName = utf8_to_wide_alloc(name);
        if (!wideName || wcslen(wideName) > MAX_PATH - 1) {
            free(wide);
            free(wideName);
            goto cleanup;
        }

        wcscpy(entry->fdata.wdata.cFileName, wideName);
        dir->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);

        free(wide);
        free(wideName);

        entry->is_wide = 1;
    }

    if (dir->findFirstHandle == INVALID_HANDLE_VALUE)
        dir->error = GetLastError();
#endif

    /* Then remove filename from path */
    path_up_cstr(dir->path);
    dir->path_len = strlen(dir->path);
    dir->path[dir->path_len++] = path_separator();
    dir->path[dir->path_len] = 0;

    entry->ownedDir = dir;
    entry->path = dir->path;
    entry->path_len = dir->path_len;

    return entry;

cleanup:
    free(entry);
    free(dir);
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
    if (entry->ownedDir) {
        dir_close(entry->ownedDir);
        free(entry);
    }
}

int dirent_error(DirectoryEntry entry) {
    if (entry->ownedDir)
        return entry->ownedDir->error;

    return 0;
}

int dirent_refresh(DirectoryEntry entry) {
#if LINUX_OS
    entry->hasExtData = 0; /* Clear cache flag */
    return 0;
#elif WINDOWS_OS
    if (entry->ownedDir) { /* Can only refresh independently-created entries, not those tied to a Directory */
        if (entry->ownedDir->findFirstHandle != INVALID_HANDLE_VALUE)
            FindClose(entry->ownedDir->findFirstHandle);

        if (!entry->is_wide) {
            entry->ownedDir->findFirstHandle = FindFirstFileA(dirent_fullname(entry), &entry->fdata.data);
        } else {
            LPWSTR wide = utf8_to_wide_alloc(dirent_fullname(entry));
            if (!wide)
                return -1;

            entry->ownedDir->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);

            free(wide);
        }

        if (entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
            entry->ownedDir->error = GetLastError();

        return 0;
    } else
        return -1;
#else
    return 0;
#endif
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
    if (entry->is_wide) {
        WideCharToMultiByte(CP_UTF8, 0, entry->fdata.wdata.cFileName, -1, entry->name, sizeof(entry->name), NULL, NULL);
        return entry->name;
    }

    return entry->fdata.data.cFileName;
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

    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
        return -1LL;
    else if (entry->is_wide) {
        li.LowPart = entry->fdata.wdata.nFileSizeLow;
        li.HighPart = entry->fdata.wdata.nFileSizeHigh;
    } else {
        li.LowPart = entry->fdata.data.nFileSizeLow;
        li.HighPart = entry->fdata.data.nFileSizeHigh;
    }

    return li.QuadPart;
#else
    UNUSED(entry)

    return -1LL;
#endif
}

int dirent_exists(DirectoryEntry entry) {
#if LINUX_OS
    return !dirFillExtData(entry);
#elif WINDOWS_OS
    if (entry->ownedDir)
        return entry->ownedDir->findFirstHandle != INVALID_HANDLE_VALUE;

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

int dirent_is_archive(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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

int dirent_is_subdirectory(DirectoryEntry entry) {
    if (!strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), ".."))
        return 0;

    return dirent_is_directory(entry);
}

int dirent_is_directory(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;
    else if (entry->is_wide) {
        return entry->fdata.wdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    } else {
        return entry->fdata.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    }
#else
    if (dirFillExtData(entry))
        return 0;

    return S_ISDIR(entry->extData.st_mode);
#endif
}

int dirent_is_encrypted(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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

int dirent_is_system(DirectoryEntry entry) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
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

int dirent_created_time_utc(DirectoryEntry entry, struct tm *t) {
#if WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;

    SYSTEMTIME time;
    if (!FileTimeToSystemTime(entry->is_wide? &entry->fdata.wdata.ftCreationTime: &entry->fdata.data.ftCreationTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_access_time_utc(DirectoryEntry entry, struct tm *t) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1;

    gmtime_r(&entry->extData.st_atime, t);

    return 0;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;

    SYSTEMTIME time;
    if (!FileTimeToSystemTime(entry->is_wide? &entry->fdata.wdata.ftLastAccessTime: &entry->fdata.data.ftLastAccessTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_modification_time_utc(DirectoryEntry entry, struct tm *t) {
#if LINUX_OS
    if (dirFillExtData(entry))
        return -1;

    gmtime_r(&entry->extData.st_mtime, t);

    return 0;
#elif WINDOWS_OS
    if (entry->ownedDir && entry->ownedDir->findFirstHandle == INVALID_HANDLE_VALUE)
        return 0;

    SYSTEMTIME time;
    if (!FileTimeToSystemTime(entry->is_wide? &entry->fdata.wdata.ftLastWriteTime: &entry->fdata.data.ftLastWriteTime, &time))
        return -1;

    systemTimeToTm(&time, t);

    return 0;
#else
    UNUSED(entry)
    UNUSED(t)

    return -1;
#endif
}

int dirent_last_status_update_time_utc(DirectoryEntry entry, struct tm *t) {
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
