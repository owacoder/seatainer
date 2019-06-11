#include <stdio.h>

#include "cclnklst.h"
#include "ccdbllst.h"
#include "ccvector.h"
#include "cchash.h"
#include "ccstring.h"
#include "io.h"
#include "platforms.h"
#include "utility.h"

#include <assert.h>

#include <stdlib.h>

int cb(HElementData data, void *userdata)
{
    long long *sum = (long long *) userdata;
    if (cc_el_get_signed_int(data))
    {
        /* printf("%d\n", *cc_el_get_signed_int(data)); */
        *sum += *cc_el_get_signed_int(data);
    }
    return 0;
}

int printInt(HElementData data, void *userdata)
{
    (void) userdata;
    printf("%d ", *cc_el_get_signed_int(data));

    return 0;
}

int printIntKeyStringValue(HElementData key, HElementData value, void *userdata)
{
#if 0
    (void) userdata;
    printf("%d: %s\n", *cc_el_get_signed_int(key), cc_s_to_cstring(*cc_el_get_string(value)));
#endif

    return 0;
}

void test_ll(size_t cnt)
{
    puts("test_ll");

    HLinkedList list = cc_ll_init(El_SignedInt, NULL), list2;
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 23);
    assert(cc_ll_push_front(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_ll_copy(list, NULL, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        assert(cc_ll_insert_after(list, 0, cc_ll_begin(list), data, NULL) == CC_OK);
    }

    printf("element is %d bytes\n", (int) cc_el_sizeof());

    Iterator find;
    cc_el_assign_signed_int(data, 102445);
    cc_ll_find(list, cc_ll_begin(list), 0, data, NULL, &find);

    if (find)
        printf("element was found!\n");
    else
        printf("element was not found\n");

    long long sum = 0;
    cc_ll_iterate(list, cb, &sum);
    printf("sum is %d\n", (int) sum);
    cc_ll_iterate(list, printInt, NULL);
    cc_ll_reverse(list);
    cc_ll_iterate(list, printInt, NULL);
    cc_ll_destroy(list, NULL);

    cc_ll_iterate(list2, cb, &sum);
    cc_ll_destroy(list2, NULL);
    cc_el_destroy(data);
}

void test_dll(size_t cnt)
{
    puts("test_dll");

    HDoublyLinkedList list = cc_dll_init(El_SignedInt), list2;
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 23);
    assert(cc_dll_push_front(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_dll_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        assert(cc_dll_insert_after(list, 0, cc_dll_begin(list), data, NULL) == CC_OK);
    }

    printf("element is %d bytes\n", (int) cc_el_sizeof());

    Iterator find;
    cc_el_assign_signed_int(data, 102445);
    cc_dll_find(list, cc_dll_begin(list), CC_FORWARD, data, NULL, &find);

    if (find)
        printf("element was found!\n");
    else
        printf("element was not found\n");

    long long sum = 0;
    cc_dll_iterate(list, CC_FORWARD, cb, &sum);
    sum = 0;
    cc_dll_iterate(list, CC_BACKWARD, cb, &sum);
    printf("sum is %d\n", (int) sum);
    cc_dll_destroy(list, NULL);

    cc_dll_iterate(list2, CC_FORWARD, cb, &sum);
    cc_dll_destroy(list2, NULL);
    cc_el_destroy(data);
}

void test_v(size_t cnt)
{
    puts("test_v");

    HVector list = cc_v_init(El_SignedInt), list2;
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 23);
    assert(cc_v_push_back(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_v_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        cc_v_push_back(list, 0, data, NULL);
    }

    printf("element is %d bytes\n", (int) cc_el_sizeof());

    Iterator find;
    cc_el_assign_signed_int(data, 102445);
    cc_v_find(list, cc_v_begin(list), CC_FORWARD, data, NULL, &find);

    if (find)
        printf("element was found!\n");
    else
        printf("element was not found\n");

    long long sum = 0;
    cc_v_iterate(list, CC_FORWARD, cb, &sum);
    sum = 0;
    cc_v_iterate(list, CC_BACKWARD, cb, &sum);
    printf("sum is %d\n", (int) sum);

    cc_v_iterate(list, CC_FORWARD, printInt, NULL);
    cc_v_reverse(list);
    cc_v_iterate(list, CC_FORWARD, printInt, NULL);

    cc_v_destroy(list, NULL);

    cc_v_iterate(list2, CC_FORWARD, cb, &sum);
    cc_v_destroy(list2, NULL);
    cc_el_destroy(data);
}

void test_v_sort(size_t cnt)
{
    puts("test_v_sort");

    HVector list = cc_v_init(El_SignedInt);
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 23);
    assert(cc_v_push_back(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, rand());
        cc_v_push_back(list, 0, data, NULL);
    }

    cc_v_iterate(list, CC_FORWARD, printInt, NULL);

    cc_v_destroy(list, NULL);
    cc_el_destroy(data);
}

void test_vv(size_t cnt)
{
    const char *strings[] = {"alpha", "omega", "pi", "a much much longer string that contains some text"};
    puts("test_vv");

    HHashTable list = cc_ht_init(El_SignedInt, El_String), list2;
    HVector sub = cc_v_init(El_String);
    HString str = cc_s_init();
    HElementData data = cc_el_init(El_String, NULL, NULL, NULL), key = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(key, rand());

    cc_s_assign_cstring(str, strings[rand() % 4]);
    *cc_el_storage_location_ptr(data) = str;

    cc_ht_insert(list, 0, key, data, NULL);

    // cc_el_clear_external_reference(data);
    // assert(cc_el_assign_vector(data, sub) != CC_BAD_PARAM);
    // assert(cc_ll_push_back(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_ht_copy(list);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(key, rand());
        cc_s_assign_cstring(str, strings[rand() % 4]);
        cc_ht_insert(list, CC_MOVE_VALUE | CC_MULTI_VALUE | CC_ORGANIZE_AUTO, key, data, NULL);
    }

    HElementData master = cc_el_init(El_HashTable, NULL, NULL, NULL);
    cc_el_assign_hash_table(master, list);
    cc_el_pretty_print(stdout, master, 0x01);
    cc_el_destroy(master);

    printf("Most collisions: %zu\n", cc_ht_max_bucket_collisions(list));
    printf("Capacity: %zu\n", cc_ht_capacity(list));
    printf("element is %d bytes\n", (int) cc_el_sizeof());

    cc_v_destroy(sub, NULL);
    cc_ht_destroy(list);
    cc_ht_destroy(list2);
    cc_el_destroy_reference(key);
    cc_el_destroy_reference(data);
    cc_s_destroy(str);
}

#include <time.h>
#include <stdlib.h>

void test_ht(size_t cnt)
{
    const char *strings[] = {"test", "Hi!", "negative", "That's ridiculous!", "Affirmatively negative"};
    const size_t strings_size = sizeof(strings)/sizeof(*strings);

    puts("test_ht");

    srand(time(NULL));

    HHashTable table = cc_ht_init(El_SignedInt, El_String);
    HElementData key = cc_el_init(El_SignedInt, NULL, NULL, NULL);
    HElementData value = cc_el_init(El_String, NULL, NULL, NULL);
    HString string = cc_s_init();

    cc_el_assign_signed_int(key, 42);
    cc_el_assign_cstring(value, strings[rand() % strings_size]);
    cc_ht_insert(table, 0, key, value, NULL);
    cc_ht_adjust_capacity(table, 1000000);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(key, i);
        cc_el_assign_cstring(value, strings[rand() % strings_size]);
        cc_ht_insert(table, CC_MULTI_VALUE | CC_MOVE_VALUE, key, value, NULL);
    }

    printf("Hash size: %zu\n", cc_ht_size(table));
    printf("Hash total collisions: %zu\n", cc_ht_total_collisions(table));
    printf("Hash most bucket collisions: %zu\n", cc_ht_max_bucket_collisions(table));
    printf("Hash load factor: %f (capacity %zu)\n", cc_ht_load_factor(table), cc_ht_capacity(table));
    /* cc_ht_adjust_load_factor(table, 0.41); */
    printf("Hash total collisions: %zu\n", cc_ht_total_collisions(table));
    printf("Hash most bucket collisions: %zu\n", cc_ht_max_bucket_collisions(table));
    printf("Hash load factor: %f (capacity %zu)\n", cc_ht_load_factor(table), cc_ht_capacity(table));

    /*cc_ht_iterate(table, printIntKeyStringValue, NULL);
    cc_el_assign_signed_int(key, 0);
    cc_ht_erase(table, CC_MULTI_VALUE, key, NULL);
    puts("");
    cc_ht_iterate(table, printIntKeyStringValue, NULL);*/

    cc_s_destroy(string);
    cc_ht_destroy(table);
    cc_el_destroy(key);
    cc_el_destroy(value);
}

void test_small_dll()
{
    puts("test_small_dll");

    unsigned hash;
    HDoublyLinkedList list = cc_dll_init(El_SignedInt);
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    int i = 0;
    for (i = 0; i < 5; ++i)
    {
        cc_el_assign_signed_int(data, i);
        cc_dll_push_back(list, 0, data, NULL);
    }

    cc_dll_iterate(list, CC_FORWARD, printInt, NULL);
    puts("");

    cc_el_assign_signed_int(data, 2);
    Iterator find;
    cc_dll_find(list, cc_dll_begin(list), CC_FORWARD | CC_ORGANIZE_TRANSPOSE, data, NULL, &find);

    if (!find)
        puts("not found");
    else
        puts("found");

    cc_el_assign_unsigned_long(data, 3);
    cc_el_hash_default(data, &hash);
    printf("%08x\n", hash);

    cc_dll_iterate(list, CC_FORWARD, printInt, NULL);
    puts("");
    cc_dll_iterate(list, CC_BACKWARD, printInt, NULL);
    puts("");

    cc_el_destroy(data);
    cc_dll_destroy(list, NULL);
}

#include "IO/aes.h"
#include "IO/crypto_rand.h"
#include "IO/hex.h"
#include "IO/md5.h"
#include "IO/sha1.h"
#include "IO/net.h"

void test_io() {
    test_aes();
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

void walk(Directory directory, unsigned long long *items, unsigned long long *size) {
    if (dir_error(directory)) {
        printf("Error while scanning %s\n", dir_path(directory));
        return;
    }

    DirectoryEntry entry;
    while ((entry = dir_next(directory)) != NULL) {
        if (dirent_is_subdirectory(entry)) {
            Directory nextDirectory = dir_open(dirent_fullname(entry));
            if (nextDirectory) {
                walk(nextDirectory, items, size);
                dir_close(nextDirectory);
            }
        } else if (!dirent_is_directory(entry)) {
            ++*items;
            long long dsize = dirent_size(entry);
            if (dsize >= 0)
                *size += dsize;
        }
    }
}

int main()
{
    const char *strs[][2] = {
        {"input", "*"},
        {"pattern", "pa*"},
        {"pattern", "*ttern?*"},
        {"pattern#", "pattern[0-9#a-z]"},
        {"Some really long string - with some special ranges like [ and ]", "*[^ ] really*]*"}
    };

    Url url = url_from_percent_encoded("http://www.google.com:80/teapot");
    int httperr;
    httperr = io_copy_and_close(io_http_get(url, NULL), io_open_file(stdout));
    if (httperr) {
        char *err = io_error_description_alloc(httperr);
        if (!err)
            printf("Error: %d\n", httperr);
        else
            printf("Error: %s\n", err);
        free(err);
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

    int err;
    const char *host = "jw.org";
    IO net = io_open_tcp_socket(host, 80, NetAddressAny, "rwb", &err);
    IO stdio = io_open_file(stdout);

    io_printf(net, "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
    io_seek(net, 0, SEEK_CUR);

    if (io_error(net))
        io_puts("Error occured while writing\n", stdio);

    io_puts("Waiting for response...\n", stdio);

    if (net == NULL)
        io_printf(stdio, "Error: %d\n", err);
    else if (io_copy(net, stdio) != 0)
        io_puts("Error while copying\n", stdio);

    io_puts("End of response.\n", stdio);

    io_close(net);
    io_close(stdio);
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

    for (size_t i = 0; i < sizeof(strs)/sizeof(*strs); ++i)
        printf("glob(\"%s\", \"%s\") = %d\n", strs[i][0], strs[i][1], glob(strs[i][0], strs[i][1]));

    test_io();

    return 0;

    const size_t cnt = 8000;

    test_ht(cnt);

#if 0
    srand(time(NULL));

    test_v_sort(cnt);
    test_ht(cnt);
#endif

    return 0;

#if 0
    test_ll(cnt);
    test_dll(cnt);
    test_v(cnt);
    test_vv(cnt);
#endif


    test_small_dll();

    return 0;
}
