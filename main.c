#include <stdio.h>

#include "cclnklst.h"
#include "ccdbllst.h"
#include "ccvector.h"
#include "cchash.h"
#include "ccstring.h"
#include "io.h"
#include "platforms.h"

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

    list2 = cc_ll_copy(list, NULL, NULL);

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
    puts("test_vv");

    HVector list = cc_v_init(El_Vector), list2, sub = cc_v_init(El_SignedInt);
    HElementData data = cc_el_init(El_Vector, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 42);
    cc_v_push_back(sub, 0, data, NULL);

    cc_el_assign_vector(data, sub);
    assert(cc_v_push_back(list, 0, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_v_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_vector(data, sub);
        cc_v_push_back(list, 0, data, NULL);
    }

    printf("element is %d bytes\n", (int) cc_el_sizeof());

    cc_v_destroy(sub, NULL);
    cc_v_destroy(list, NULL);
    cc_v_destroy(list2, NULL);
    cc_el_destroy(data);
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

    printf("Hash size: %u\n", cc_ht_size(table));
    printf("Hash total collisions: %u\n", cc_ht_total_collisions(table));
    printf("Hash most bucket collisions: %u\n", cc_ht_max_bucket_collisions(table));
    printf("Hash load factor: %f (capacity %u)\n", cc_ht_load_factor(table), cc_ht_capacity(table));
    /* cc_ht_adjust_load_factor(table, 0.41); */
    printf("Hash total collisions: %u\n", cc_ht_total_collisions(table));
    printf("Hash most bucket collisions: %u\n", cc_ht_max_bucket_collisions(table));
    printf("Hash load factor: %f (capacity %u)\n", cc_ht_load_factor(table), cc_ht_capacity(table));

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

void test_io() {
    test_aes();
    test_hex();

#if WINDOWS_OS
    IO io = io_open("F:/Test_Data/test.txt", "w");
#elif LINUX_OS
    IO io = io_open_native("/shared/Test_Data/test.txt", "wx");
#endif
    if (!io)
        return;

    // io_seek64(io, 1LL << 16, SEEK_SET);
    io_puts("This is some text", io);
    io_seek(io, 1, SEEK_SET);
    io_puts("That my", io);

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

    io = io_open_cstring("");
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

int main()
{
    test_io();

    const size_t cnt = 800000;

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
