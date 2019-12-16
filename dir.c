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
#endif

#include "utility.h"

#define GLOB_MAX_POSITIONS 100

int glob(const char *str, const char *pattern) {
    /* Arbitrary stack depth limit (basically the number of '*' characters allowed in the pattern,
     * although multiple '*' characters will be batched together and therefore count as only one '*' toward this limit) */
    const size_t max_positions = GLOB_MAX_POSITIONS;
    struct position {
        const char *strpos; /* Where to start searching for the pattern */
        const char *patternpos;
    } positions[GLOB_MAX_POSITIONS];

    /* If a wildcard is found, a new entry is pushed onto the position stack
     *
     * If *strpos == 0, and *patternpos contains all '*' characters or *patternpos == 0, then the match was successful. Return immediately.
     *
     * If *strpos == 0, and *patternpos is not a valid match, then the match failed. More characters were expected. Return immediately.
     *
     * If *strpos != 0, but *patternpos == 0, then the match failed. Too much input was present. Backtrack to the previous position.
     */

    size_t current_position = 0;

    positions[0].strpos = str;
    positions[0].patternpos = pattern;

    while (1) {
try_new_glob:
        for (; *positions[current_position].patternpos; ++positions[current_position].patternpos) {
            switch (*positions[current_position].patternpos) {
                case '?':
                    if (*positions[current_position].strpos == 0)
                        goto stop_checking_glob_entry;

                    ++positions[current_position].strpos;
                    break;
                case '*': {
                    while (*positions[current_position].patternpos == '*')
                        ++positions[current_position].patternpos;

                    if (*positions[current_position].patternpos == 0) /* Anything matches if '*' is at end of pattern */
                        return 0;

                    /* Pattern has specific char to match after star, so search for it (this is optional as an optimization) */
                    if (*positions[current_position].patternpos != '[' && *positions[current_position].patternpos != '?') {
                        const char *str = (const char *) strchr(positions[current_position].strpos, *positions[current_position].patternpos);
                        if (str == NULL)
                            return -1; /* Since stars are minimal matchers, if the character afterward does not exist, the string must not match */
                        positions[current_position].strpos = str;
                    }

                    if (++current_position == max_positions)
                        return -2; /* Glob too complicated! */

                    positions[current_position] = positions[current_position-1];
                    goto try_new_glob;
                }
                case '[': {
                    if (positions[current_position].patternpos[1] == 0 || positions[current_position].patternpos[2] == 0)
                        return -2;

                    const char *lastCharInSet = positions[current_position].patternpos + 2;

                    while (*lastCharInSet && (lastCharInSet[-1] == '-' || (lastCharInSet == positions[current_position].patternpos + 2 && lastCharInSet[-1] == '^') || *lastCharInSet != ']'))
                        ++lastCharInSet;

                    if (*lastCharInSet != ']') /* Set not concluded properly */
                        return -2;

                    --lastCharInSet;
                    ++positions[current_position].patternpos;

                    int negateSet = *positions[current_position].patternpos == '^';
                    if (negateSet) {
                        if (positions[current_position].patternpos == lastCharInSet) /* Negated set with nothing in it isn't valid */
                            return -2;
                        ++positions[current_position].patternpos;
                    }

                    /* positions[current_position].patternpos now points to first char in set and lastCharInSet points to the last char in set */
                    /* They may be pointing to the same char if it's a one-character set */
                    if (positions[current_position].patternpos == lastCharInSet) {
                        if ((negateSet? *positions[current_position].strpos == *positions[current_position].patternpos:
                                        *positions[current_position].strpos != *positions[current_position].patternpos))
                            goto stop_checking_glob_entry;

                        ++positions[current_position].strpos;
                        positions[current_position].patternpos = lastCharInSet + 1;
                    } else { /* Complex set, possibly negated */
                        int matched = negateSet; /* If matched is non-zero, the set matches */
                        unsigned char strChr = (unsigned char) *positions[current_position].strpos;

                        for (; positions[current_position].patternpos <= lastCharInSet; ++positions[current_position].patternpos) {
                            if (positions[current_position].patternpos[1] == '-') { /* Compute range */
                                int rangeLow = (unsigned char) positions[current_position].patternpos[0];
                                int rangeHigh = (unsigned char) positions[current_position].patternpos[2];

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

                                positions[current_position].patternpos += 2;
                            } else if (*positions[current_position].strpos == *positions[current_position].patternpos) {
                                matched = !negateSet;
                                break;
                            }
                        }

                        if (!matched)
                            goto stop_checking_glob_entry;

                        ++positions[current_position].strpos;
                        positions[current_position].patternpos = lastCharInSet + 1;
                    }
                    break;
                }
                default:
                    if (*positions[current_position].strpos != *positions[current_position].patternpos)
                        goto stop_checking_glob_entry;

                    ++positions[current_position].strpos;
                    break;
            }
        }
stop_checking_glob_entry:

        if (*positions[current_position].strpos == 0) {
            while (*positions[current_position].patternpos == '*')
                ++positions[current_position].patternpos;

            return *positions[current_position].patternpos == 0? 0: -1;
        }

        if (current_position == 0) {
            return -1;

        } else { /* Nested glob, restart current glob at next string position */
            /* Pattern has specific char to match after star, so search for it (this is optional as an optimization) */
            if (*positions[current_position-1].patternpos != '[' && *positions[current_position-1].patternpos != '?') {
                const char *str = strchr(positions[current_position-1].strpos+1, *positions[current_position-1].patternpos);
                if (str == NULL)
                    return -1; /* Since stars are minimal matchers, if the character afterward does not exist, the string must not match */
                positions[current_position-1].strpos = str;
            } else { /* This is not optional :) */
                ++positions[current_position-1].strpos;
            }

            positions[current_position] = positions[current_position-1];
        }
    };
}

int utf8glob(const char *str, const char *pattern) {
    /* Arbitrary stack depth limit (basically the number of '*' characters allowed in the pattern,
     * although multiple '*' characters will be batched together and therefore count as only one '*' toward this limit) */
    const size_t max_positions = GLOB_MAX_POSITIONS;
    struct position {
        const char *strpos; /* Where to start searching for the pattern */
        const char *patternpos;
    } positions[GLOB_MAX_POSITIONS];

    /* If a wildcard is found, a new entry is pushed onto the position stack
     *
     * If *strpos == 0, and *patternpos contains all '*' characters or *patternpos == 0, then the match was successful. Return immediately.
     *
     * If *strpos == 0, and *patternpos is not a valid match, then the match failed. More characters were expected. Return immediately.
     *
     * If *strpos != 0, but *patternpos == 0, then the match failed. Too much input was present. Backtrack to the previous position.
     */

    size_t current_position = 0;

    positions[0].strpos = str;
    positions[0].patternpos = pattern;

    while (1) {
try_new_glob:
        for (; *positions[current_position].patternpos; utf8next(positions[current_position].patternpos, &positions[current_position].patternpos)) {
            switch (*positions[current_position].patternpos) {
                case '?':
                    if (*positions[current_position].strpos == 0)
                        goto stop_checking_glob_entry;

                    utf8next(positions[current_position].strpos, &positions[current_position].strpos);
                    break;
                case '*': {
                    while (*positions[current_position].patternpos == '*')
                        utf8next(positions[current_position].patternpos, &positions[current_position].patternpos);

                    if (*positions[current_position].patternpos == 0) /* Anything matches if '*' is at end of pattern */
                        return 0;

                    /* Pattern has specific char to match after star, so search for it (this is optional as an optimization) */
                    if (*positions[current_position].patternpos != '[' && *positions[current_position].patternpos != '?') {
                        const char *str = utf8chr(positions[current_position].strpos, utf8next(positions[current_position].patternpos, NULL));
                        if (str == NULL)
                            return -1; /* Since stars are minimal matchers, if the character afterward does not exist, the string must not match */
                        positions[current_position].strpos = str;
                    }

                    if (++current_position == max_positions)
                        return -2; /* Glob too complicated! */

                    positions[current_position] = positions[current_position-1];
                    goto try_new_glob;
                }
                case '[': {
                    /* TODO: doesn't support UTF-8 yet. */
                    if (positions[current_position].patternpos[1] == 0 || positions[current_position].patternpos[2] == 0)
                        return -2;

                    const char *lastCharInSet = NULL, *endOfSet = NULL, *initialEndOfSet = NULL;
                    utf8next(positions[current_position].patternpos + 1, &endOfSet);

                    lastCharInSet = positions[current_position].patternpos + 1;
                    initialEndOfSet = endOfSet;
                    while (*endOfSet && (endOfSet[-1] == '-' || (endOfSet == initialEndOfSet && endOfSet[-1] == '^') || *endOfSet != ']')) {
                        lastCharInSet = endOfSet;
                        utf8next(endOfSet, &endOfSet);
                    }

                    if (*endOfSet != ']') /* Set not concluded properly */
                        return -2;

                    utf8next(positions[current_position].patternpos, &positions[current_position].patternpos);

                    int negateSet = *positions[current_position].patternpos == '^';
                    if (negateSet) {
                        if (positions[current_position].patternpos == lastCharInSet) /* Negated set with nothing in it isn't valid */
                            return -2;
                        ++positions[current_position].patternpos;
                    }

                    /* positions[current_position].patternpos now points to first char in set and lastCharInSet points to the last char in set */
                    /* They may be pointing to the same char if it's a one-character set */
                    if (positions[current_position].patternpos == lastCharInSet) {
                        const char *nextStr = NULL, *nextPattern = NULL;
                        if (negateSet? utf8next(positions[current_position].strpos, &nextStr) == utf8next(positions[current_position].patternpos, &nextPattern):
                                       utf8next(positions[current_position].strpos, &nextStr) != utf8next(positions[current_position].patternpos, &nextPattern))
                            goto stop_checking_glob_entry;

                        positions[current_position].strpos = nextStr;
                        positions[current_position].patternpos = nextPattern;
                    } else { /* Complex set, possibly negated */
                        int matched = negateSet; /* If matched is non-zero, the set matches */
                        const char *nextStr = NULL;
                        uint32_t strChr = utf8next(positions[current_position].strpos, &nextStr);

                        while (positions[current_position].patternpos <= lastCharInSet) {
                            const char *nextPattern = NULL;
                            uint32_t patternCodepoint = utf8next(positions[current_position].patternpos, &nextPattern);

                            if (*nextPattern == '-') { /* Compute range */
                                uint32_t rangeLow = patternCodepoint;
                                uint32_t rangeHigh = utf8next(nextPattern + 1, &nextPattern);

                                /* Swap range if backwards */
                                if (rangeHigh < rangeLow) {
                                    uint32_t temp = rangeHigh;
                                    rangeHigh = rangeLow;
                                    rangeLow = temp;
                                }

                                if (rangeLow <= strChr && strChr <= rangeHigh) {
                                    matched = !negateSet; /* Set to 1 if normal set, and 0 if negated */
                                    break;
                                }
                            } else if (strChr == patternCodepoint) {
                                matched = !negateSet;
                                break;
                            }

                            positions[current_position].patternpos = nextPattern;
                        }

                        if (!matched)
                            goto stop_checking_glob_entry;

                        positions[current_position].strpos = nextStr;
                        positions[current_position].patternpos = endOfSet;
                    }
                    break;
                }
                default: {
                    const char *first = NULL;

                    if (utf8next(positions[current_position].strpos, &first) != utf8next(positions[current_position].patternpos, NULL))
                        goto stop_checking_glob_entry;

                    positions[current_position].strpos = first;
                    break;
                }
            }
        }
stop_checking_glob_entry:

        if (*positions[current_position].strpos == 0) {
            while (*positions[current_position].patternpos == '*')
                utf8next(positions[current_position].patternpos, &positions[current_position].patternpos);

            return *positions[current_position].patternpos == 0? 0: -1;
        }

        if (current_position == 0) {
            return -1;

        } else { /* Nested glob, restart current glob at next string position */
            /* Pattern has specific char to match after star, so search for it (this is optional as an optimization) */
            if (*positions[current_position-1].patternpos != '[' && *positions[current_position-1].patternpos != '?') {
                const char *strPlus1 = NULL;
                utf8next(positions[current_position-1].strpos, &strPlus1);

                const char *str = utf8chr(strPlus1, utf8next(positions[current_position-1].patternpos, NULL));
                if (str == NULL)
                    return -1; /* Since stars are minimal matchers, if the character afterward does not exist, the string must not match */
                positions[current_position-1].strpos = str;
            } else { /* This is not optional :) */
                utf8next(positions[current_position-1].strpos, &positions[current_position-1].strpos);
            }

            positions[current_position] = positions[current_position-1];
        }
    };
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
    unsigned int next;
    unsigned int count;
    unsigned int capacity;
    unsigned int in_use;
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
};

struct DirStruct {
    HANDLE findFirstHandle;
    struct DirEntryStruct findData, lastFoundData;
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    enum DirectoryFilter filter;
    struct SortedDirStruct sorted;
    size_t path_len;
    char path[MAX_PATH + 1]; /* Owned copy of path */
};

void filetime_to_time_t(FILETIME *time, time_t *t) {
    ULARGE_INTEGER large;

    large.LowPart = time->dwLowDateTime;
    large.HighPart = time->dwHighDateTime;

    large.QuadPart /= 10000000; /* Convert from 100 nanosecond intervals to seconds */
    large.QuadPart -= 11644473600LL; /* Number of seconds between Jan 1, 1601 and Jan 1, 1970 */

    *t = (time_t) large.QuadPart;
}
#else
struct DirStruct {
    int error; /* Zero if no error occured while opening the directory, platform specific otherwise */
    enum DirectoryFilter filter;
    struct SortedDirStruct sorted;
}
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
    memcpy(result->path, dir, len);
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
    char name[MAX_PATH + 3];
    size_t len = strlen(dir);

    if (len > MAX_PATH - 2)
        return NULL;

    memcpy(name, dir, len);

    /* Ensure path ends with directory separator */
    if (len && name[len-1] != '\\' && name[len-1] != '/')
        name[len++] = '\\';
    name[len] = 0;

    result = CALLOC(1, sizeof(*result));
    if (result == NULL)
        return NULL;

    /* Copy to new Directory object */
    strcpy(result->path, name);
    result->path_len = len;
    result->findData.parent = result->lastFoundData.parent = result;

    /* Finish adding glob to end of path */
    name[len++] = '*';
    name[len] = 0;

    if (strstr(mode, "@ncp") != NULL) {
        result->findFirstHandle = FindFirstFileA(name, &result->findData.fdata.data);

        result->findData.is_wide = result->lastFoundData.is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc(name);
        if (!wide) {
            FREE(result);
            return NULL;
        }

        result->findFirstHandle = FindFirstFileW(wide, &result->findData.fdata.wdata);

        FREE(wide);

        result->findData.is_wide = result->lastFoundData.is_wide = 1;
    }

    if (result->findFirstHandle == INVALID_HANDLE_VALUE)
        result->error = GetLastError();
#else
    UNUSED(dir)
    UNUSED(mode)

    return NULL;
#endif

    result->filter = filter;
    if (sort != DirSortNone && result && result->error == 0) {
        DirectoryEntry entry;
        struct SortedDirStruct sorted;

        memset(&sorted, 0, sizeof(sorted));
        sorted.in_use = 1;

        while ((entry = dir_next(result)) != NULL) {
            if (!dir_entry_present_with_filter(entry, filter))
                continue;

            DirectoryEntry copy = dirent_copy(entry);
            if (!copy || sorted_dir_add_entry(&sorted, copy)) {
                sorted_dir_free(&sorted);
                dirent_close(copy);
                result->error = CC_ENOMEM;
                return result;
            }
        }

        for (size_t i = 0; i < sorted.count; ++i)
            printf("Unsorted %s\n", dirent_name(sorted.entries[i]));

        sorted_dir_sort(&sorted, sort);

        for (size_t i = 0; i < sorted.count; ++i)
            printf("Sorted %s\n", dirent_name(sorted.entries[i]));

        result->sorted = sorted;
        result->filter = DirFilterNone; /* Since we already filtered, don't do it again */
    }

    return result;
}

int dir_error(Directory dir) {
    return dir->error;
}

void dir_clearerr(Directory dir) {
    dir->error = 0;
}

static DirectoryEntry dir_next_internal(Directory dir) {
    if (dir->sorted.in_use) {
        if (dir->sorted.entries == NULL || dir->sorted.next == dir->sorted.count)
            return NULL;

        return dir->sorted.entries[dir->sorted.next++];
    }

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

DirectoryEntry dir_next(Directory dir) {
    DirectoryEntry entry;

    while ((entry = dir_next_internal(dir)) != NULL) {
        if (!dir_entry_present_with_filter(entry, dir->filter))
            continue;

        return entry;
    }

    return entry;
}

const char *dir_path(Directory dir) {
    dir->path[dir->path_len] = 0;
    return dir->path;
}

void dir_close(Directory dir) {
    if (dir == NULL)
        return;

    sorted_dir_free(&dir->sorted);

#if LINUX_OS
    if (dir->handle != NULL)
        closedir(dir->handle);
#elif WINDOWS_OS
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
#else
    if (pathLen > MAX_PATH)
        goto cleanup;

    dir = CALLOC(1, sizeof(*dir));
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

        dir->findFirstHandle = FindFirstFileA(dir->path, &entry->fdata.data);
        strcpy(entry->fdata.data.cFileName, name);

        entry->is_wide = 0;
    } else {
        LPWSTR wide = utf8_to_wide_alloc(dir->path);
        if (!wide)
            goto cleanup;

        LPWSTR wideName = utf8_to_wide_alloc(name);
        if (!wideName || wcslen(wideName) > MAX_PATH - 1) {
            FREE(wide);
            FREE(wideName);
            goto cleanup;
        }

        dir->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);
        wcscpy(entry->fdata.wdata.cFileName, wideName);

        FREE(wide);
        FREE(wideName);

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
            LPWSTR wide = utf8_to_wide_alloc(dirent_fullname(entry));
            if (!wide)
                return entry->parent->error = CC_ENOMEM;

            entry->parent->findFirstHandle = FindFirstFileW(wide, &entry->fdata.wdata);

            FREE(wide);
        }

        if (entry->parent->findFirstHandle == INVALID_HANDLE_VALUE)
            entry->parent->error = GetLastError();

        return entry->parent->error;
    } else
        return entry->parent->error = CC_EPERM;
#else
    entry->parent->error = CC_ENOTSUP;
    return -1;
#endif
}

const char *dirent_path(DirectoryEntry entry) {
    entry->parent->path[entry->parent->path_len] = 0;
    return entry->parent->path;
}

const char *dirent_fullname(DirectoryEntry entry) {
    strcpy(entry->parent->path + entry->parent->path_len, dirent_name(entry));
    return entry->parent->path;
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

    entry->parent->error = 0;

    if (entry->ownedDir && entry->parent->findFirstHandle == INVALID_HANDLE_VALUE) {
        entry->parent->error = CC_EBADF;
        return -1LL;
    }
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
#else
    if (dirFillExtData(entry))
        return !strcmp(dirent_name(entry), ".") || !strcmp(dirent_name(entry), "..");

    return S_ISDIR(entry->extData.st_mode);
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
    LPWSTR wide = utf8_to_wide_alloc(dirent_fullname(entry));
    if (wide == NULL)
        return entry->parent->error = CC_ENOMEM;

    /* Change to directory */
    if (!!(attributes & DIRENT_ATTRIBUTE_DIRECTORY) != dirent_is_directory(entry))
        return entry->parent->error = CC_ENOTSUP;

    /* Change to symlink/reparse point */
    if (!!(attributes & DIRENT_ATTRIBUTE_SYMLINK) != dirent_is_symlink(entry))
        return entry->parent->error = CC_ENOTSUP;

    /* Change to compression or sparsity */
    if ((    attributes                   & (DIRENT_ATTRIBUTE_COMPRESSED | DIRENT_ATTRIBUTE_SPARSE)) !=
            (dirent_get_attributes(entry) & (DIRENT_ATTRIBUTE_COMPRESSED | DIRENT_ATTRIBUTE_SPARSE))) {
        HANDLE handle = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
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
            if (EncryptFileW(wide) == 0)
                goto cleanup;
        } else {
            if (DecryptFileW(wide, 0) == 0)
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

    if (SetFileAttributesW(wide, attributes) == 0)
        goto cleanup;

    FREE(wide);
    return dirent_refresh(entry); /* TODO: if entry is returned from dir_next(), it will return an error and will not reflect changes to the directory entry, since such entries cannot be refreshed. */

cleanup:
    FREE(wide);
    return entry->parent->error = GetLastError();
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
