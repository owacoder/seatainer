#include <stdio.h>

#include "ccio.h"
#include "platforms.h"
#include "utility.h"
#include "seaerror.h"

#include <assert.h>

#include <stdlib.h>

#include "IO/aes.h"
#include "IO/crypto_rand.h"
#include "IO/hex.h"
#include "IO/md5.h"
#include "IO/sha1.h"
#include "IO/sha256.h"
#include "IO/net.h"
#include "IO/tee.h"
#include "IO/concat.h"
#include "IO/zlib_io.h"
#include "IO/padding/bit.h"
#include "IO/padding/pkcs7.h"
#include "IO/repeat.h"
#include "IO/limiter.h"
#include "IO/tbuffer.h"
#include "IO/base64.h"

void test_io() {
    test_hex();

    IO tmp = io_open_dynamic_buffer("wt");
    IO hex = io_open_hex_encode(tmp, "w");
    fflush(stdout);
    io_puts("Some data", tmp);
    io_puts(" and some more data", tmp);
    printf("Written: %d\n", io_printf(tmp, " and something more, with a number (%d) and a string (%10s)\n", 1443, "str"));
    io_putc(0, tmp);
    puts(io_underlying_buffer(tmp));
    printf("Size: %lu\n", (unsigned long) io_underlying_buffer_size(tmp));
    printf("Capacity: %lu\n", (unsigned long) io_underlying_buffer_capacity(tmp));
    return;

#if WINDOWS_OS
    IO io = io_open_native("F:/Test_Data/test.txt", "r");
#elif LINUX_OS
    IO io = io_open_native("/shared/Test_Data/test.txt", "wx");
#endif
    if (!io)
        return;

    IO out = io_open_file(stdout);
    io_setvbuf(io, NULL, _IOFBF, 4);
    while (1) {
        int ch = io_getc(io);
        if (ch == EOF) break;

        io_putc(ch, out);
    }
    puts("");

    // io_seek64(io, 1LL << 16, SEEK_SET);
    io_setvbuf(io, NULL, _IOFBF, 4);
    //io_puts("This is some text", io);
    io_seek(io, 1, SEEK_SET);
    io_putc('?', io);
    io_puts("That is your name", io);

    io_close(io);

    char buffer[100];
    io = io_open_buffer(buffer, sizeof(buffer), "w");
    if (!io)
        return;
    /* assert(io_puts("Some text", io) == 0); */
    printf(".%012d.\n", 545);
    double dvalue = 0.2525252525252525;
    printf("printed %d\n", io_printf(io, "\n.%%%.19f.\n", dvalue));
    assert(io_seek(io, 0, SEEK_END) == 0);
    assert(io_putc(' ', io) == EOF);
    printf("buffer size = %ld\n", io_tell(io));
    io_close(io);

    for (size_t i = 0; i < sizeof(buffer); ++i)
        printf("%d ", buffer[i]);

    printf("\n%.100s\n", buffer);

    io = io_open_crypto_rand();
    assert(io_read(buffer, 1, sizeof(buffer), io) == sizeof(buffer));
    io_close(io);

    for (size_t i = 0; i < sizeof(buffer); ++i)
        printf("%d ", buffer[i]);

    io = io_open_cstring("", "r");
    IO io2 = io_open_sha1(io, "r");

    size_t read = io_read(buffer, 1, 32, io2);
    printf("Read %d\n", (int) read);
    io_rewind(io2);
    assert(io_read(buffer, 1, 32, io2) != 0);

    printf(".%%%.19f.\n", dvalue);

    for (size_t i = 0; i < read; ++i)
        printf("%02x", (uint8_t) buffer[i]);
    puts("");

    io_close(io2);
    io_close(io);
}

#include "dir.h"

static void search_helper(IO out, Directory directory, const char *fileglob, int searchSubDirs) {
    if (dir_error(directory)) {
        char *reason = error_description_alloc(dir_error(directory));
        printf("Cannot search %s: %s\n", dir_path(directory), reason);
        FREE(reason);
        return;
    }

    DirectoryEntry entry;
    while ((entry = dir_next(directory)) != NULL) {
        if (!dirent_is_actual_entry(entry))
            continue;

        if (glob(dirent_name(entry), fileglob) == 0)
            io_printf(out, "%s\n", dirent_fullname(entry));

        if (dirent_is_subdirectory(entry) && searchSubDirs) {
            Directory nextDirectory = dir_open(dirent_fullname(entry), DirFilterShowAll, DirSortNone);
            if (nextDirectory) {
                search_helper(out, nextDirectory, fileglob, searchSubDirs);
                dir_close(nextDirectory);
            }
        }
    }
}

void search(IO out, const char *path, const char *fileglob, int searchSubDirs) {
    search_helper(out, dir_open(path, DirFilterShowAll, DirSortNone), fileglob, searchSubDirs);
}

void walk(Directory directory, unsigned long long *items, unsigned long long *size) {
    if (dir_error(directory)) {
        printf("Error while scanning %s\n", dir_path(directory));
        return;
    }

    DirectoryEntry entry;
    while ((entry = dir_next(directory)) != NULL) {
        if (!dirent_is_actual_entry(entry))
            continue;

        printf("path: %s\n", dirent_fullname(entry));

        if (dirent_is_subdirectory(entry)) {
            Directory nextDirectory = dir_open(dirent_fullname(entry), DirFilterNone, DirSortNone);
            if (nextDirectory) {
                walk(nextDirectory, items, size);
                dir_close(nextDirectory);
            }
        } else {
            ++*items;
            long long dsize = dirent_size(entry);
            if (dsize >= 0)
                *size += dsize;
        }
    }
}

void walk_dir(IO out, Directory directory) {
    if (dir_error(directory)) {
        printf("Error while iterating %s: %s", dir_path(directory), error_description(dir_error(directory)));
        return;
    }

    Path p = path_get_current_working_dir();
    io_printf(out, "cwd: %s\n", path_str(p));
    path_up(p);
    if (path_set_current_working_dir(p) == 0) {
        path_destroy(p);
        p = path_get_current_working_dir();
        io_printf(out, "cwd: %s\n", path_str(p));
    }
    path_destroy(p);

    DirectoryEntry entry;
    while ((entry = dir_next(directory)) != NULL) {
        if (!dirent_is_actual_entry(entry))
            continue;

        long long ntime;
        time_t time;
        size_t name_len = strlen(dirent_name(entry));
        io_printf(out, "%s%-*c  ", dirent_name(entry), 40 - (int) name_len, dirent_is_directory(entry)? path_separator(): ' ');
        io_format_file_size(out, dirent_size(entry));
        io_putc('\n', out);

        int err;
        if ((void)(ntime = dirent_created_time_ns(entry, &err)), err == 0) {
            time = ntime / 1000000000;
            io_printf(out, "  Created: %s\b, %09lld", asctime(localtime(&time)), ntime % 1000000000);
        }

        if ((void)(ntime = dirent_last_modification_time_ns(entry, &err)), err == 0) {
            time = ntime / 1000000000;
            io_printf(out, "  Modified: %s\b, %09lld", asctime(localtime(&time)), ntime % 1000000000);
        }

        if ((void)(ntime = dirent_last_status_update_time_ns(entry, &err)), err == 0) {
            time = ntime / 1000000000;
            io_printf(out, "  Updated: %s\b, %09lld", asctime(localtime(&time)), ntime % 1000000000);
        }

        if ((void)(ntime = dirent_last_access_time_ns(entry, &err)), err == 0) {
            time = ntime / 1000000000;
            io_printf(out, "  Accessed: %s\b, %09lld", asctime(localtime(&time)), ntime % 1000000000);
        }
    }
}

#include "process.h"
#include <stdlib.h>

#include "containers.h"
#include "container_io.h"

int printer(void *arg) {
    int value = *((int *) arg);
    thread_sleep(5000 - value * 1000);
    printf("Thread number %d\n", value);
    return value;
}

#include <math.h>

void json() {
    GenericMap map = genericmap_create(binary_compare, (Compare) variant_compare, (Copier) variant_copy, (Deleter) variant_destroy);
    Variant v = variant_create_custom_move(map, (Compare) genericmap_compare, (Copier) genericmap_copy, (Deleter) genericmap_destroy);

    genericmap_insert_move(map, "test1", 5, variant_create_boolean(1));
    genericmap_insert_move(map, "test2", 5, variant_create_string("This is a value\"\b"));
    genericmap_insert_move(map, "test3", 5, variant_create_string("Hi to you too!"));

    *variant_get_container_base(v) = build_container_base(NULL, (Serializer) json_serialize_variant, 1);

    IO out = io_open_file(stdout);
    printf("\nJSON error: %s\n", error_description(variant_serialize(out, v, NULL)));
}

int main(int argc, char **argv, const char **envp)
{
    {
        Process p = process_start("Y:/Test_Data/CmdTest.exe", NULL);

        printf("Start error: %s\n", error_description(process_error(p)));

        io_puts("Test 11111\n", process_stdin(p, NULL));
        io_close(process_stdin(p, NULL));
        io_copy(process_stdout(p, NULL), io_open_file(stdout));
        printf("EOF: %d\n", io_eof(process_stdout(p, NULL)));
        printf("Read error: %s\n", error_description(io_error(process_stdout(p, NULL))));

        printf("Stop error: %s\n", error_description(process_destroy(p)));

        return 0;

        int exit = 0;
        ProcessNativeHandle handle;
        int success = process_start_async("C:\\Windows\\system32\\cmd.exe", NULL, &handle);

        // printf("Handle %d\n", handle);
        if (success == 0)
            success = process_native_kill_normal(handle);

        if (success == 0) {
            printf("Result code: %d\n", exit);
        } else {
            printf("Error occurred: %s\n", error_description(success));
        }

        thread_sleep(4000);

        return 0;
    }

    if (1) {
        json();
        return 0;
    }

    if (1)
    {
        DirectoryEntry entry = dirent_open("\\\\OLIVER-CODE\\Fast/Test_Data/Test - Copy");
        //DirectoryEntry entry = dirent_open("Y:/Test_Data/Test - Copy");

        printf("Item: %s\nExists: %s\n", dirent_fullname(entry), dirent_exists(entry)? "yes": "no");
        printf("Result: %s\n", error_description(dirent_remove(entry, 1)));
        return 0;
    }

    if (0)
    {
        IO out = io_open_file(stdout);
        io_printf(out, "%g\n", NAN);
        return 0;
    }

    {
        const char *test = "-inf";
        IO io = io_open_cstring(test, "rb");
        float result1 = 0, result2 = 0;
        char c1 = 0, c2 = 0;
        printf("String: %s\nResult sscanf(): %d\nResult io_scanf(): %d\n", test, sscanf(test, "%g%1c", &result1, &c1), io_scanf(io, "%g%1c", &result2, &c2));
        printf("Value sscanf(): %g, %c\nValue io_scanf(): %g, %c\n", result1, c1, result2, c2);
        return 0;
    }

    {
        IO cstrio = io_open_cstring("First\r\nSecond\r\nThird\r\nFourth\r\n", "r");

        StringList cstrlist = stringlist_split_io(cstrio, "\n", 1);

        for (size_t i = 0; i < stringlist_size(cstrlist); ++i) {
            printf("Item %u: '%s'\n", (unsigned) i, stringlist_array(cstrlist)[i]);
        }

        stringlist_destroy(cstrlist);
        io_close(cstrio);
        return 0;
    }

    {
        StringList list = stringlist_split("How are you doing  all today", " ", 1);

        stringlist_set_compare_fn(list, strcmp_no_case);

        StringList sorted = stringlist_stable_sorted(list, 0);

        printf("Index of you: %d\n", (unsigned) stringlist_rfind(list, "YOU", SIZE_MAX));

        for (size_t i = 0; i < stringlist_size(list); ++i) {
            printf("Item %u: '%s'\n", (unsigned) i, stringlist_array(list)[i]);
        }

        printf("Sorted index of you: %d\n", (unsigned) stringlist_rfind(sorted, "YOU", SIZE_MAX));

        for (size_t i = 0; i < stringlist_size(list); ++i) {
            printf("Sorted Item %u: '%s'\n", (unsigned) i, stringlist_array(sorted)[i]);
        }

        printf("Equal: %d\n", stringlist_compare(list, list));

        char *joined = stringlist_join(list, "");
        printf("Joined: %s\n", joined);
        //FREE(joined);

        stringlist_destroy(list);
        stringlist_destroy(sorted);

        return 0;
    }

    int args[5] = {1, 2, 3, 4, 5};
    Thread threads[5];

    for (size_t i = 0; i < 5; ++i) {
        threads[i] = thread_create(printer, &args[i]);
    }

    for (size_t i = 0; i < 5; ++i) {
        int result;
        thread_join(threads[i], &result);

        printf("Thread %d returned %d\n", i, result);
    }

    return 0;

    printf("ENVIRON PATH: %s\n", environment_get_variable("PATH"));

    StringMap environs = environment_get_variable_map();

    for (Iterator it = stringmap_begin(environs); it; it = stringmap_next(environs, it)) {
        printf("%s: %s\n", stringmap_key_of(environs, it), stringmap_value_of(environs, it));
    }

    stringmap_destroy(environs);

    {
        Variant var = variant_create_float(1000.1234);
        Variant str = variant_create_float(103.44);

        printf("Value of variant: %s\n", variant_to_string(var, NULL));
        printf("Kilograms as integer: %d\n", variant_to_int(str, NULL));
        printf("How many kilograms: %f\n", variant_to_float(str, NULL));
        printf("Compare: %d\n", variant_compare(var, str));

        variant_set_int64(var, 1233);
        variant_set_string(var, "Not so fast");

        printf("Value of variant: %s\n", variant_to_string(var, NULL));
        printf("Kilograms as integer: %d\n", variant_to_int(str, NULL));
        printf("How many kilograms: %f\n", variant_to_float(str, NULL));
        printf("Compare: %d\n", variant_compare(var, str));

        variant_destroy(var);
        variant_destroy(str);
        return 0;
    }

    {
        StringSet set = stringset_create();

        // stringset_add(set, "M");
        stringset_add(set, "N");
        // stringset_add(set, "O");
        // stringset_add(set, "L");
        // stringset_add(set, "K");
        stringset_add(set, "Q");
        stringset_add(set, "P");
        // stringset_add(set, "H");
        stringset_add(set, "I");
        // stringset_add(set, "A");
        stringset_remove(set, "N");
        stringset_remove(set, "Q");
        //stringset_remove(set, "I");
        //stringset_remove(set, "P");

        StringSet copy = stringset_copy(set);

        printf("Size: %d\n", stringset_size(set));
        for (Iterator it = stringset_begin(set); it; it = stringset_next(set, it)) {
            printf("Item: %s\n", stringset_value_of(set, it));
        }

        if (copy) {
            printf("Size: %d\n", stringset_size(copy));
            for (Iterator it = stringset_begin(copy); it; it = stringset_next(copy, it)) {
                printf("Item: %s\n", stringset_value_of(copy, it));
            }
        }

        stringset_destroy(set);
    }

    {
        StringList list = stringlist_split("How are you doing  all today", " ", 1);

        stringlist_set_compare_fn(list, strcmp_no_case);

        StringList sorted = stringlist_stable_sorted(list, 1);

        printf("Index of you: %d\n", (unsigned) stringlist_rfind(list, "YOU", SIZE_MAX));

        for (size_t i = 0; i < stringlist_size(list); ++i) {
            printf("Item %u: '%s'\n", (unsigned) i, stringlist_array(list)[i]);
        }

        printf("Sorted index of you: %d\n", (unsigned) stringlist_rfind(sorted, "YOU", SIZE_MAX));

        for (size_t i = 0; i < stringlist_size(list); ++i) {
            printf("Sorted Item %u: '%s'\n", (unsigned) i, stringlist_array(sorted)[i]);
        }

        printf("Equal: %d\n", stringlist_compare(list, list));

        char *joined = stringlist_join(list, "");
        printf("Joined: %s\n", joined);
        //FREE(joined);

        stringlist_destroy(list);
        stringlist_destroy(sorted);
    }

    {
        StringList list = stringlist_create();
        stringlist_append(list, "First");
        stringlist_append(list, "Second");
        stringlist_append(list, "Third");

        stringlist_insert_list(list, list, 2);

        for (size_t i = 0; i < stringlist_size(list); ++i) {
            printf("Item %u: %s\n", (unsigned) i, stringlist_array(list)[i]);
        }

        char *joined = stringlist_join(list, ", ");
        printf("Joined: %s\n", joined);
        FREE(joined);

        stringlist_destroy(list);
        return 0;
    }

    {
        Directory wdir = dir_open("/shared/Test_Data", DirFilterNone, DirSortNone);
        walk_dir(io_open_file(stdout), wdir);
        return 0;
    }

#if 1
    {
        IO base64 = io_open_base64_encode(io_open_cstring("any carnal pleasur", "rb"), "rb");
        IO base64b = io_open_base64_encode(io_open_cstring("any carnal pleasur", "rb"), "rb");
        IO base64decode = io_open_base64_decode(base64, "rb");
        IO base64bdecode = io_open_base64_decode(base64b, "rb");
        int err;

        if ((err = io_slow_copy(base64decode, io_open_file(stdout))) != 0)
            printf("An error occured: %s\n", error_description(err));
        io_puts("\n", io_open_file(stdout));
        io_copy(base64bdecode, io_open_file(stdout));

        puts("");
        IO bout2 = io_open_base64_decode(io_open_file(stdout), "wb");
        IO bout = io_open_base64_encode(bout2, "wb");
        io_copy(io_open_cstring("any carnal pleasure[]()~~~", "rb"), bout);
        io_close(bout);
        io_close(bout2);
        puts("");

        return 0;
    }
#endif

#if 0
    {
        IO buffer = io_open_thread_buffer();
        IO repeat = io_open_repeat(io_open_cstring("SomeData", "rb"), "rb");

        size_t iteration = 0;
        IO out = io_open_file(stdout);

        while (1) {
            ++iteration;
            if (io_putc(io_getc(repeat), buffer) == EOF)
                printf("\nError\n");
            if (io_putc(io_getc(repeat), buffer) == EOF)
                printf("\nError\n");

            if (iteration % 300 == 0) {
                io_copy(buffer, out);
                io_clearerr(buffer);
            } else {
                putc(io_getc(buffer), stdout);
            }
        }

        return 0;
    }

    {
        IO str = io_open_cstring("keyword", "r");
        IO rep = io_open_repeat(str, "r");

        //io_copy(io_open_limiter(rep, 0, 500, "rb"), io_open_file(stdout));
        io_copy(rep, io_open_limiter(io_open_file(stdout), 0, 500, "wb"));

        return 0;
    }
#endif

    {
        Process p = process_start("/shared/Test_Data/CmdTest", NULL);

        printf("Start error: %s\n", error_description(process_error(p)));

        io_puts("Test 1\n", process_stdin(p, NULL));
        io_close(process_stdin(p, NULL));
        io_copy(process_stdout(p, NULL), io_open_file(stdout));
        printf("EOF: %d\n", io_eof(process_stdout(p, NULL)));
        printf("Read error: %s\n", error_description(io_error(process_stdout(p, NULL))));

        printf("Stop error: %s\n", error_description(process_destroy(p)));

        return 0;

        int exit = 0;
        ProcessNativeHandle handle;
        int success = process_start_async("C:\\Windows\\system32\\cmd.exe", NULL, &handle);

        // printf("Handle %d\n", handle);
        if (success == 0)
            success = process_native_kill_normal(handle);

        if (success == 0) {
            printf("Result code: %d\n", exit);
        } else {
            printf("Error occurred: %s\n", error_description(success));
        }

        thread_sleep(4000);

        return 0;
    }

    {
        Directory dir = dir_open("/shared/Programming", DirFilterShowAll, DirSortByTime | DirSortFoldersBeforeFiles);
        DirectoryEntry entry;

        while ((entry = dir_next(dir)) != NULL) {
            time_t temp = dirent_last_modification_time(entry, NULL);
            char *timeStr = asctime(gmtime(&temp));

            timeStr[strlen(timeStr)-1] = 0;
            printf("%s: ", timeStr);

            if (dirent_is_subdirectory(entry))
                printf("%s/: %lld\n", dirent_name(entry), dirent_size(entry));
            else
                printf("%s: %lld\n", dirent_name(entry), dirent_size(entry));
        }

        dir_close(dir);
        return 0;
    }

    {
        const unsigned char key[] = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
        const unsigned char iv[] = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

        IO input = io_open_cstring("\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
                                   "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
                                   "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef\x10", "rb");
        IO input_padding = io_open_pkcs7_padding_encode(input, 16, "rb");
        IO input_aes = io_open_aes_encrypt(input_padding, AES_128, AES_PCBC, key, iv, "rb<");

        IO output = io_open_hex_encode(io_open_file(stdout), "wb");
        IO output_padding = io_open_pkcs7_padding_decode(output, 16, "wb");
        IO output_aes = io_open_aes_decrypt(output_padding, AES_128, AES_PCBC, key, iv, "wb");

        io_copy(input_aes, output_aes);

        printf("\n%d\n", io_close(output_padding));

        return 0;
    }

    {
        char buf1[10], buf2[10];

        IO lhs = io_open_buffer(buf1, sizeof(buf1), "wb");
        IO rhs = io_open_buffer(buf2, sizeof(buf2), "wb");
        IO concat = io_open_concat(lhs, rhs, "wb");

        io_puts("ABCDEFGHIJKLMNOPQRSTU", concat);

        printf("%.10s%.10s\n%s\n", buf1, buf2, error_description(io_error(concat)));

        return 0;
    }

    {
        Url url = url_from_percent_encoded("https://google.com");
        int err;
        HttpState http = http_create_state_from_url(url, NULL, &err, NULL);

        if (err) {
            printf("Error: %s\n", error_description(err));
            return 1;
        }

        printf("Connected.\n");
        if (http_begin_request(http, "GET", url))
            printf("Begin request failed.\n");
        printf("Sent request.\n");
        if (http_begin_response(http))
            printf("Begin response failed.\n");
        printf("Began response.\n");
        if (io_copy(http_response_body(http), io_open_file(stdout)))
            printf("Copy failed.\n");
        printf("Retrieved response body.\n");
        if (http_end_response(http))
            printf("End request failed.\n");
        printf("Request done.\n");

        http_destroy_state(http);
        return 0;
    }

#if 0
    Directory wdir = dir_open("/shared");
    walk_dir(io_open_file(stdout), wdir);

    const char *strs[][2] = {
        {"input", "*"},
        {"input1", "*n*u*"},
        {"This is a toasty fried egg!", "[T]* t* f*g[-!]"},
        {"1g1g1g1g1g1g1g1g1g1g1g1g1g1g.txt", "*g*g*g*g*g*g*g*g*g*g*g*g*g.txt"},
        {"input1", "in*u*???"},
        {"pattern", "pa*"},
        {"pattern", "*tt*?*"},
        {"pattern#", "pattern[0-9#a-z]"},
        {"Some really long string - with some special ranges like [ and ]", "*[^ ] really*]*"},
        {"Ҹари н[ншрләр", "*н[[]*[ә-a]шр*"}
    };

    for (size_t i = 0; i < sizeof(strs)/sizeof(*strs); ++i)
        printf("utf8glob(\"%s\", \"%s\") = %d\n", strs[i][0], strs[i][1], utf8glob(strs[i][0], strs[i][1]));

    return 0;

#ifdef CC_INCLUDE_ZLIB
    IO file = io_open("/shared/Test_Data/gzip.txt", "rb");
    IO defl = io_open_zlib_deflate_easy(file, ZlibDeflate, "rb");
    IO zlib = io_open_zlib_inflate_easy(defl, ZlibOnlyInflate, "rb");
    IO zout = io_open_file(stdout);

    const size_t iters = 1000000;

    srand(time(NULL));

    clock_t start = clock();
    for (size_t i = 0; i < iters; ++i) {
        const char *value = "My string";
        io_printf(zout, "|%.50s|\n", value);
    }
    double First_timediff = (double) (clock() - start) / CLOCKS_PER_SEC;

    start = clock();
    for (size_t i = 0; i < iters; ++i) {
        const char *value = "My string";
        printf("|%.50s|\n", value);
    }
    double Second_timediff = (double) (clock() - start) / CLOCKS_PER_SEC;

    printf("%g seconds for IO vs %g seconds for built-in\n", First_timediff, Second_timediff);

    return 0;

    if (io_copy(zlib, zout))
        printf("%s\n", error_description_alloc(io_error(zlib)));

    io_vclose(4, zout, zlib, defl, file);

    return 0;
#endif

    Url url = url_from_percent_encoded("http://www.google.com:80/teapot");
    int httperr;
    httperr = io_copy_and_close(io_http_get(url, NULL), io_open_file(stdout));
    if (httperr) {
        char *err = error_description_alloc(httperr);
        if (!err)
            printf("Error: %d\n", httperr);
        else
            printf("Error: %s\n", err);
        FREE(err);
    }
    url_destroy(url);

    return 0;

    url = url_from_percent_encoded("htTP://[2001:db8::7]:9655/c=GB?objectClass?one");

    if (url_get_scheme(url)) printf("scheme: %s\n", url_get_scheme(url));
    if (url_get_authority(url)) printf("authority: %s\n", url_get_authority(url));
    if (url_get_username(url)) printf("username: %s\n", url_get_username(url));
    if (url_get_password(url)) printf("password: %s\n", url_get_password(url));
    if (url_get_host(url)) printf("host: %s\n", url_get_host(url));
    if (url_get_port(url)) printf("port: %s\n", url_get_port(url));
    if (url_get_path(url)) printf("path: %s\n", url_get_path(url));
    if (url_get_query(url)) printf("query: %s\n", url_get_query(url));
    if (url_get_fragment(url)) printf("fragment: %s\n", url_get_fragment(url));
    if (url_get_path_and_query_and_fragment(url)) printf("fullpath: %s\n", url_get_path_and_query_and_fragment(url));
    printf("url: %s\n", url_get_percent_encoded(url));

    url_destroy(url);

    return 0;
#endif

    int err;
    const char *host = "www.google.com";
    IO net = io_open_tcp_socket(host, 443, NetAddressAny, "rwb", &err);
    IO stdio = io_open_file(stdout);

    io_printf(net, "GET /en HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    io_seek(net, 0, SEEK_CUR);

    if (io_error(net))
        io_puts("Error occured while writing\n", stdio);

    io_puts("Waiting for response...\n", stdio);

    if (net == NULL)
        io_printf(stdio, "Error: %d\n", err);
    else if (io_copy(net, stdio) != 0)
        io_puts("Error while copying\n", stdio);

    if (io_error(net))
        printf("Net: %s\n", error_description(io_error(net)));
    else if (io_error(stdio))
        printf("Stdio: %s\n", error_description(io_error(stdio)));

    printf("Close: %s\n", error_description(io_close(net)));
    printf("Close: %s\n", error_description(io_close(stdio)));
    return 0;

#if 1
    IO in = io_open_file(stdin);
    if (1) {
        char buf[20];
        int value = 0, res;
        res = io_scanf(io_open_cstring("+123 Some-long-string", "r"), "%d %8[A-Za-z]", &value, buf);
        printf("Matched = %d (%d, %s)\n", res, value, buf);
        io_rewind(in);
    }
#endif

    DirectoryEntry entry = dirent_open("/");
    if (!entry) {
        fputs("Cannot open directory\n", stderr);
        return 0;
    }

    FILE *out = fopen("F:\\Test_Data\\outf.txt", "w");

    fprintf(out, "is directory? %d\n", dirent_is_directory(entry));
    fprintf(out, "path = %s\n", dirent_path(entry));
    fprintf(out, "name = %s\n", dirent_name(entry));
    fprintf(out, "fullname = %s\n", dirent_fullname(entry));

    fclose(out);

    dirent_close(entry);

    const char *data[] = {"/", "home", "pi"};
    Path path = path_construct_gather(data, sizeof(data)/sizeof(*data));

    path = path_append(path, "data.txt");

    printf("Name = %s\n", path_name(path));
    printf("Ext = %s\n", path_ext(path));

    printf("Original = %s\n", path_str(path));
    printf("Normalized = %s\n", path_str(path_normalize(path)));
    printf("Parent Dir = %s\n", path_str(path_up(path)));
    printf("Parent Dir = %s\n", path_str(path_up(path)));

    path_destroy(path);

#if 0
    unsigned long long items = 0, size = 0;
    Directory dir = dir_open("/");
    walk(dir, &items, &size);

    printf("Walk of %s completed: %llu files found with a total size of %llu bytes\n", dir_path(dir), items, size);

#if 0
    DirectoryEntry entry;
    printf("Error: %d\n", dir_error(dir));
    perror("");
    printf("Printing directory \"%s\"\n", dir_path(dir));
    while ((entry = dir_next(dir)) != NULL)
        printf("%s %s %lld\n", dirent_fullname(entry), dirent_is_directory(entry)? "(dir)": "", dirent_size(entry));
#endif

    dir_close(dir);
#endif

    test_io();

    return 0;
}
