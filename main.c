#include <stdio.h>

#include "cclnklst.h"
#include "ccdbllst.h"
#include "ccvector.h"

#include <assert.h>

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

void test_ll(size_t cnt)
{
    puts("test_ll");

    HLinkedList list = cc_ll_init(El_SignedInt), list2;
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 23);
    assert(cc_ll_push_front(list, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_ll_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        assert(cc_ll_insert_after(list, cc_ll_begin(list), data, NULL) == CC_OK);
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
    assert(cc_dll_push_front(list, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_dll_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        assert(cc_dll_insert_after(list, cc_dll_begin(list), data, NULL) == CC_OK);
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
    assert(cc_v_push_back(list, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_v_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_signed_int(data, i);
        cc_v_push_back(list, data, NULL);
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
    cc_v_destroy(list, NULL);

    cc_v_iterate(list2, CC_FORWARD, cb, &sum);
    cc_v_destroy(list2, NULL);
    cc_el_destroy(data);
}

void test_vv(size_t cnt)
{
    puts("test_vv");

    HVector list = cc_v_init(El_Vector), list2, sub = cc_v_init(El_SignedInt);
    HElementData data = cc_el_init(El_Vector, NULL, NULL, NULL);

    cc_el_assign_signed_int(data, 42);
    cc_v_push_back(sub, data, NULL);

    cc_el_assign_vector(data, sub);
    assert(cc_v_push_back(list, data, NULL) != CC_TYPE_MISMATCH);

    list2 = cc_v_copy(list, NULL, NULL);

    size_t i;
    for (i = 0; i < cnt; ++i)
    {
        cc_el_assign_vector(data, sub);
        cc_v_push_back(list, data, NULL);
    }

    printf("element is %d bytes\n", (int) cc_el_sizeof());

    cc_v_destroy(sub, NULL);
    cc_v_destroy(list, NULL);
    cc_v_destroy(list2, NULL);
    cc_el_destroy(data);
}

void test_small_dll()
{
    puts("test_small_dll");

    unsigned hash;
    HVector list = cc_v_init(El_SignedInt);
    HElementData data = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    for (int i = 0; i < 5; ++i)
    {
        cc_el_assign_signed_int(data, i);
        cc_v_push_back(list, data, NULL);
    }

    cc_v_iterate(list, CC_FORWARD, printInt, NULL);
    puts("");

    cc_el_assign_vector(data, list);
    cc_el_hash_default(data, &hash);
    printf("%08x\n", hash);

    cc_el_assign_signed_int(data, 2);
    Iterator find;
    cc_v_find(list, cc_v_begin(list), CC_FORWARD | CC_ORGANIZE_TRANSPOSE, data, NULL, &find);

    if (!find)
        puts("not found");
    else
        puts("found");

    cc_el_assign_vector(data, list);
    cc_el_hash_default(data, &hash);
    printf("%08x\n", hash);

    cc_v_iterate(list, CC_FORWARD, printInt, NULL);
    puts("");
    cc_v_iterate(list, CC_BACKWARD, printInt, NULL);
    puts("");

    cc_el_destroy(data);
    cc_v_destroy(list, NULL);
}

int main()
{
    const size_t cnt = 400000;

#if 0
    test_ll(cnt);
    test_dll(cnt);
    test_v(cnt);
    test_vv(cnt);
#endif


    test_small_dll();

    return 0;
}
