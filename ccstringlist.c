#include "ccstringlist.h"

#include "utility.h"
#include <math.h>
#include <stdio.h>

static int binary_compare(const Binary *a, const Binary *b) {
    size_t max = MIN(a->length, b->length);
    int cmp = memcmp(a->data, b->data, max);

    if (cmp)
        return cmp;
    else if (a->length < max)
        return -1;
    else if (b->length < max)
        return 1;
    else
        return 0;
}

struct VariantStructCustomHelper {
    Compare compare;
    Copier copy;
    Deleter deleter;
    void *data;
};

struct VariantStructAtomHelper {
    union {
        int boolean;
        long long integer;
        double floating;
    } d;
    Binary string;
};

struct VariantStruct {
    union {
        struct VariantStructCustomHelper custom;
        struct VariantStructAtomHelper atom;
    } d;
    enum VariantType type;
};

Variant variant_create_null() {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantNull;

    return var;
}

Variant variant_create_boolean(int b) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBoolean;
    var->d.atom.d.boolean = !!b;

    return var;
}

Variant variant_create_int(int value) {
    return variant_create_int64(value);
}

Variant variant_create_int64(long long value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantInteger;
    var->d.atom.d.integer = value;

    return var;
}

Variant variant_create_float(double value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantFloat;
    var->d.atom.d.floating = value;

    return var;
}

Variant variant_create_string_move(char *value) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantString;
    var->d.atom.string.data = value;

    return var;
}

Variant variant_create_string(const char *value) {
    char *duplicate = strdup(value);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_string_move(duplicate);
    if (var == NULL)
        FREE(duplicate);

    return var;
}

Variant variant_create_binary_string_move(char *value, size_t value_len) {
    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantBinary;
    var->d.atom.string.data = value;
    var->d.atom.string.length = value_len;

    return var;
}

Variant variant_create_binary_string(const char *value, size_t value_len) {
    char *duplicate = MALLOC(value_len+1);
    if (duplicate == NULL)
        return NULL;

    Variant var = variant_create_binary_string_move(duplicate, value_len);
    if (var == NULL)
        FREE(duplicate);
    else {
        memcpy(duplicate, value, value_len);
        duplicate[value_len] = 0;
    }

    return var;
}

Variant variant_create_binary_string_binary_move(Binary value) {
    return variant_create_binary_string_move(value.data, value.length);
}

Variant variant_create_binary_string_binary(const Binary value) {
    return variant_create_binary_string(value.data, value.length);
}

Variant variant_create_custom_move(void *item, Compare compare, Copier copier, Deleter deleter) {
    if (compare == NULL || copier == NULL)
        return NULL;

    Variant var = CALLOC(sizeof(*var), 1);
    if (var == NULL)
        return NULL;

    var->type = VariantCustom;
    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = item;

    return var;
}

Variant variant_create_custom(const void *item, Compare compare, Copier copier, Deleter deleter) {
    if (compare == NULL || copier == NULL)
        return NULL;

    if (deleter == NULL)
        deleter = FREE;

    void *duplicate = copier(item);
    if (duplicate == NULL && item != NULL)
        return NULL;

    Variant var = variant_create_custom_move(duplicate, compare, copier, deleter);
    if (var == NULL)
        deleter(duplicate);

    return var;
}

Variant variant_copy(Variant other) {
    switch (variant_get_type(other)) {
        default:
        case VariantNull: return variant_create_null();
        case VariantBoolean: return variant_create_boolean(variant_get_boolean(other));
        case VariantInteger: return variant_create_int64(variant_get_int64(other));
        case VariantFloat: return variant_create_float(variant_get_float(other));
        case VariantString: return variant_create_string(variant_get_string(other));
        case VariantBinary: return variant_create_binary_string_binary(variant_get_binary(other));
        case VariantCustom: return variant_create_custom(variant_get_custom(other),
                                                         variant_get_compare_fn(other),
                                                         variant_get_copier_fn(other),
                                                         variant_get_deleter_fn(other));
    }
}

int variant_compare(Variant lhs, Variant rhs) {
    if (variant_get_type(lhs) < variant_get_type(rhs))
        return -1;
    else if (variant_get_type(lhs) > variant_get_type(rhs))
        return 1;

    switch (variant_get_type(lhs)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return (lhs->d.atom.d.boolean > rhs->d.atom.d.boolean) - (lhs->d.atom.d.boolean < rhs->d.atom.d.boolean);
        case VariantInteger: return (lhs->d.atom.d.integer > rhs->d.atom.d.integer) - (lhs->d.atom.d.integer < rhs->d.atom.d.integer);
        case VariantFloat: return (lhs->d.atom.d.floating > rhs->d.atom.d.floating) - (lhs->d.atom.d.floating < rhs->d.atom.d.floating);
        case VariantString: return strcmp(lhs->d.atom.string.data, rhs->d.atom.string.data);
        case VariantBinary: return binary_compare(&lhs->d.atom.string, &rhs->d.atom.string);
        case VariantCustom: return lhs->d.custom.compare(lhs->d.custom.data, rhs->d.custom.data);
    }
}

enum VariantType variant_get_type(Variant var) {
    return var->type;
}

Compare variant_get_compare_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.compare;
}

Copier variant_get_copier_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.copy;
}

Deleter variant_get_deleter_fn(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.deleter;
}

int variant_is_null(Variant var) {
    return variant_get_type(var) == VariantNull;
}

int variant_is_boolean(Variant var) {
    return variant_get_type(var) == VariantBoolean;
}

int variant_is_int(Variant var) {
    if (variant_get_type(var) != VariantInteger)
        return 0;

    return var->d.atom.d.integer >= INT_MIN && var->d.atom.d.integer <= INT_MAX;
}

int variant_is_int64(Variant var) {
    return variant_get_type(var) == VariantInteger;
}

int variant_is_float(Variant var) {
    return variant_get_type(var) == VariantFloat;
}

int variant_is_number(Variant var) {
    return variant_is_int64(var) || variant_is_float(var);
}

int variant_is_string(Variant var) {
    return variant_get_type(var) == VariantString;
}

int variant_is_binary(Variant var) {
    return variant_get_type(var) == VariantBinary;
}

int variant_is_custom(Variant var) {
    return variant_get_type(var) == VariantCustom;
}

int variant_set_null(Variant var) {
    variant_clear(var);
    return 0;
}

int variant_set_boolean(Variant var, int b) {
    variant_clear(var);

    var->d.atom.d.boolean = !!b;
    var->type = VariantBoolean;

    return 0;
}

int variant_set_int(Variant var, int value) {
    variant_clear(var);

    var->d.atom.d.integer = value;
    var->type = VariantInteger;

    return 0;
}

int variant_set_int64(Variant var, long long value) {
    variant_clear(var);

    var->d.atom.d.integer = value;
    var->type = VariantInteger;

    return 0;
}

int variant_set_float(Variant var, double value) {
    variant_clear(var);

    var->d.atom.d.floating = value;
    var->type = VariantFloat;

    return 0;
}

int variant_set_string_move(Variant var, char *value) {
    variant_clear(var);

    var->d.atom.string.data = value;
    var->d.atom.string.length = strlen(value);
    var->type = VariantString;

    return 0;
}

int variant_set_string(Variant var, const char *value) {
    char *duplicate = strdup(value);
    if (duplicate == NULL)
        return CC_ENOMEM;

    int err = variant_set_string_move(var, duplicate);
    if (err)
        FREE(duplicate);

    return err;
}

int variant_set_binary_string_move(Variant var, char *value, size_t value_len) {
    variant_clear(var);

    var->d.atom.string.data = value;
    var->d.atom.string.length = value_len;
    var->type = VariantBinary;

    return 0;
}

int variant_set_binary_string_binary_move(Variant var, Binary value) {
    return variant_set_binary_string_move(var, value.data, value.length);
}

int variant_set_binary_string(Variant var, const char *value, size_t value_len) {
    char *duplicate = MALLOC(value_len+1);
    if (duplicate == NULL)
        return CC_ENOMEM;

    int err = variant_set_binary_string_move(var, duplicate, value_len);
    if (err)
        FREE(duplicate);
    else {
        memcpy(duplicate, value, value_len);
        duplicate[value_len] = 0;
    }

    return err;
}

int variant_set_binary_string_binary(Variant var, const Binary value) {
    return variant_set_binary_string(var, value.data, value.length);
}

int variant_set_custom_move(Variant var, void *item, Compare compare, Copier copier, Deleter deleter) {
    if (compare == NULL || copier == NULL)
        return CC_EINVAL;

    variant_clear(var);

    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = item;
    var->type = VariantCustom;

    return 0;
}

int variant_set_custom(Variant var, const void *item, Compare compare, Copier copier, Deleter deleter) {
    if (compare == NULL || copier == NULL)
        return CC_EINVAL;

    void *duplicate = copier(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    variant_clear(var);

    var->d.custom.compare = compare;
    var->d.custom.copy = copier;
    var->d.custom.deleter = deleter? deleter: (Deleter) FREE;
    var->d.custom.data = duplicate;
    var->type = VariantCustom;

    return 0;
}

int variant_get_boolean(Variant var) {
    if (!variant_is_boolean(var))
        return 0;

    return var->d.atom.d.boolean;
}

int variant_get_int(Variant var) {
    if (!variant_is_int(var) || var->d.atom.d.integer < INT_MIN || var->d.atom.d.integer > INT_MAX)
        return 0;

    return var->d.atom.d.integer;
}

long long variant_get_int64(Variant var) {
    if (!variant_is_int(var))
        return 0;

    return var->d.atom.d.integer;
}

double variant_get_float(Variant var) {
    if (!variant_is_float(var))
        return 0.0;

    return var->d.atom.d.floating;
}

char *variant_get_string(Variant var) {
    if (!variant_is_string(var))
        return NULL;

    return var->d.atom.string.data;
}

Binary variant_get_binary(Variant var) {
    if (!variant_is_binary(var)) {
        Binary temp = {.data = NULL, .length = 0};
        return temp;
    }

    return var->d.atom.string;
}

void *variant_get_custom(Variant var) {
    if (!variant_is_custom(var))
        return NULL;

    return var->d.custom.data;
}

int variant_to_boolean(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return !!var->d.atom.d.integer;
        case VariantFloat: return !!var->d.atom.d.floating;
        case VariantString:
        case VariantBinary: return var->d.atom.string.data && var->d.atom.string.data[0] != 0;
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

int variant_to_int(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return variant_get_int(var);
        case VariantFloat:
            if (var->d.atom.d.floating < INT_MIN || var->d.atom.d.floating > INT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            long result = strtol(var->d.atom.string.data, NULL, 0);
            if (errno || result < INT_MIN || result > INT_MAX) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

long long variant_to_int64(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return var->d.atom.d.integer;
        case VariantFloat: return trunc(var->d.atom.d.floating);
        case VariantString:
        case VariantBinary: {
            errno = 0;
            long long result = strtoll(var->d.atom.string.data, NULL, 0);
            if (errno) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

double variant_to_float(Variant var, int *error) {
    if (error)
        *error = 0;

    switch (variant_get_type(var)) {
        default:
        case VariantNull: return 0;
        case VariantBoolean: return var->d.atom.d.boolean;
        case VariantInteger: return var->d.atom.d.integer;
        case VariantFloat: return var->d.atom.d.floating;
        case VariantString:
        case VariantBinary: {
            errno = 0;
            double result = strtod(var->d.atom.string.data, NULL);
            if (errno) {
                if (error)
                    *error = CC_ERANGE;
                return 0;
            }

            return result;
        }
        case VariantCustom:
            if (error)
                *error = CC_ENOTSUP;
            return 0;
    }
}

const char *variant_to_string(Variant var, int *error) {
    if (variant_is_custom(var)) {
        if (error)
            *error = CC_ENOTSUP;
        return NULL;
    }

    if (var->d.atom.string.data)
        return var->d.atom.string.data;

    char *string = NULL;
    switch (variant_get_type(var)) {
        default:
        case VariantNull: string = strdup(""); break;
        case VariantBoolean: string = strdup(var->d.atom.d.boolean? "true": "false"); break;
        case VariantInteger: {
            /* Silence some warnings with using the wrong (standard!) format */
#if WINDOWS_OS && (MSVC_COMPILER | CLANG_COMPILER | GCC_COMPILER)
# define VARIANT_TO_STRING_FP_FORMAT "%I64d"
#else
# define VARIANT_TO_STRING_FP_FORMAT "%lld"
#endif
            size_t len = snprintf(NULL, 0, VARIANT_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, VARIANT_TO_STRING_FP_FORMAT, var->d.atom.d.integer);
            break;
        }
        case VariantFloat: {
            size_t len = snprintf(NULL, 0, "%.*g", __DBL_DECIMAL_DIG__-1, var->d.atom.d.floating);
            string = MALLOC(len+1);
            if (string)
                snprintf(string, len+1, "%.*g", __DBL_DECIMAL_DIG__-1, var->d.atom.d.floating);
            break;
        }
    }

    if (error && string == NULL)
        *error = CC_ENOMEM;

    var->d.atom.string.length = string? strlen(string): 0;
    return var->d.atom.string.data = string;
}

Binary variant_to_binary(Variant var, int *error) {
    if (variant_is_custom(var)) {
        if (error)
            *error = CC_ENOTSUP;
        Binary temp = {.data = NULL, .length = 0};
        return temp;
    }

    if (variant_is_string(var)) {
        if (var->d.atom.string.length == 0)
            var->d.atom.string.length = strlen(var->d.atom.string.data);
        return var->d.atom.string;
    }

    if (var->d.atom.string.data)
        return var->d.atom.string;

    variant_to_string(var, error);

    return var->d.atom.string;
}

void variant_clear(Variant var) {
    if (variant_get_type(var) == VariantCustom)
        var->d.custom.deleter(var->d.custom.data);
    else
        FREE(var->d.atom.string.data);

    var->d.atom.string.data = NULL;
    var->d.atom.string.length = 0;
    var->type = VariantNull;
}

void variant_destroy(Variant var) {
    if (var) {
        if (variant_get_type(var) == VariantCustom) {
            var->d.custom.deleter(var->d.custom.data);
        } else {
            FREE(var->d.atom.string.data);
        }

        FREE(var);
    }
}

typedef struct AVLNode {
    struct AVLNode *left, *right, *parent;
    Binary key;
    void *value;
    int balance;
} AVLNode;

typedef struct AVLTree {
    struct AVLNode *root;
    BinaryCompare compare;
    Copier copy;
    Deleter deleter;
    size_t size;
} AVLTree;

static void *avltree_copier(const void *p) {
    return (void *) p;
}

static void avltree_deleter(void *p) {
    UNUSED(p)
}

static AVLNode *avltree_nodealloc(const char *key, size_t key_len) {
    AVLNode *node = CALLOC(1, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->key.data = MALLOC(key_len + 1);
    if (node->key.data == NULL) {
        FREE(node);
        return NULL;
    }

    memcpy(node->key.data, key, key_len);
    node->key.data[key_len] = 0;
    node->key.length = key_len;

    return node;
}

static void avltree_nodedestroy(AVLNode *node, Deleter deleter) {
    deleter(node->value);
    FREE(node->key.data);
    FREE(node);
}

static BinaryCompare avltree_get_compare_fn(AVLTree *tree) {
    return tree->compare;
}

static void avltree_set_compare_fn(AVLTree *tree, BinaryCompare compare) {
    tree->compare = compare? compare: binary_compare;
}

static Copier avltree_get_copier_fn(AVLTree *tree) {
    return tree->copy;
}

static void avltree_set_copier_fn(AVLTree *tree, Copier copier) {
    tree->copy = copier? copier: (Copier) avltree_copier;
}

static Deleter avltree_get_deleter_fn(AVLTree *tree) {
    return tree->deleter;
}

static void avltree_set_deleter_fn(AVLTree *tree, Deleter deleter) {
    tree->deleter = deleter? deleter: avltree_deleter;
}

static AVLTree *avltree_create() {
    AVLTree *tree = CALLOC(1, sizeof(*tree));
    if (tree == NULL)
        return NULL;

    tree->compare = binary_compare;
    tree->copy = avltree_copier;
    tree->deleter = avltree_deleter;

    return tree;
}

static void avltree_destroy_helper(AVLNode *node, Deleter deleter) {
    if (node == NULL)
        return;

    avltree_destroy_helper(node->left, deleter);
    avltree_destroy_helper(node->right, deleter);
    avltree_nodedestroy(node, deleter);
}

static void avltree_destroy(AVLTree *tree) {
    if (tree != NULL)
        avltree_destroy_helper(tree->root, tree->deleter);

    FREE(tree);
}

static AVLNode *avltree_copy_helper(AVLNode *node, AVLNode *parent, Copier copier, Deleter deleter) {
    if (node == NULL)
        return NULL;

    AVLNode *new_node = avltree_nodealloc(node->key.data, node->key.length);
    if (new_node == NULL)
        return NULL;

    new_node->parent = parent;
    new_node->value = copier(node->value);
    new_node->left = avltree_copy_helper(node->left, new_node, copier, deleter);
    new_node->right = avltree_copy_helper(node->right, new_node, copier, deleter);

    if ((new_node->value == NULL && node->value != NULL) ||
        (new_node->left == NULL && node->left != NULL) ||
        (new_node->right == NULL && node->right != NULL)) {
        avltree_destroy_helper(new_node, deleter);
        return NULL;
    }

    return new_node;
}

static AVLTree *avltree_copy(AVLTree *other) {
    AVLTree *tree = avltree_create();
    if (tree == NULL)
        return NULL;

    tree->compare = other->compare;
    tree->copy = other->copy;
    tree->deleter = other->deleter;
    tree->size = other->size;
    tree->root = avltree_copy_helper(other->root, NULL, other->copy, other->deleter);

    if (tree->root == NULL && other->root != NULL) {
        avltree_destroy(tree);
        return NULL;
    }

    return tree;
}

static AVLNode **avltree_find_helper(AVLNode **node, AVLNode **parent, BinaryCompare compare, Binary item) {
    *parent = NULL;

    while (1) {
        if (*node == NULL)
            return node;

        int cmp = compare(&item, &((*node)->key));

        if (cmp == 0)
            return node;
        else if (cmp < 0) {
            *parent = *node;
            node = &((*node)->left);
        }
        else if (cmp > 0) {
            *parent = *node;
            node = &((*node)->right);
        }
    }
}

static AVLNode *avltree_min_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->left != NULL) {
        node = node->left;
    }

    return node;
}

static AVLNode *avltree_max_node(AVLTree *tree) {
    AVLNode *node = tree->root;

    if (node == NULL)
        return NULL;

    while (node->right != NULL) {
        node = node->right;
    }

    return node;
}

static AVLNode *avltree_inorder_previous(AVLNode *node) {
    AVLNode *last = NULL;

    if (node->left != NULL) {
        node = node->left;

        while (node->right != NULL)
            node = node->right;

        return node;
    }

    while (node->left == last) {
        last = node;
        node = node->parent;

        if (node == NULL)
            return NULL;
    }

    return node;
}

static AVLNode *avltree_inorder_next(AVLNode *node) {
    AVLNode *last = NULL;

    if (node->right != NULL) {
        node = node->right;

        while (node->left != NULL)
            node = node->left;

        return node;
    }

    while (node->right == last) {
        last = node;
        node = node->parent;

        if (node == NULL)
            return NULL;
    }

    return node;
}

static AVLNode *avltree_find(AVLTree *tree, const char *key, size_t key_len) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent;

    return *avltree_find_helper(&tree->root, &parent, tree->compare, b);
}

static AVLNode *avltree_rotate_left(AVLNode *parent, AVLNode *child) {
    AVLNode *temp = child->left;
    child->left = parent;
    parent->parent = child;
    parent->right = temp;

    if (temp != NULL)
        temp->parent = parent;

    if (child->balance == 0) { /* Child already a balanced tree, will become left heavy */
        parent->balance = +1;
        child->balance = -1;
    } else {
        parent->balance = child->balance = 0;
    }

    return child;
}

static AVLNode *avltree_rotate_right(AVLNode *parent, AVLNode *child) {
    AVLNode *temp = child->right;
    child->right = parent;
    parent->parent = child;
    parent->left = temp;

    if (temp != NULL)
        temp->parent = parent;

    if (child->balance == 0) { /* Child already a balanced tree, will become right heavy */
        parent->balance = -1;
        child->balance = +1;
    } else {
        parent->balance = child->balance = 0;
    }

    return child;
}

static AVLNode *avltree_rotate_rightleft(AVLNode *parent, AVLNode *child) {
    AVLNode *grandchild = child->left;

    child->left = grandchild->right;
    grandchild->right = child;
    if (child->left != NULL)
        child->left->parent = child;
    child->parent = grandchild;

    parent->right = grandchild->left;
    grandchild->left = parent;
    parent->parent = grandchild;
    if (parent->right != NULL)
        parent->right->parent = parent;

    if (grandchild->balance < 0) {
        parent->balance = 0;
        child->balance = +1;
    } else if (grandchild->balance > 0) {
        parent->balance = -1;
        child->balance = 0;
    } else {
        parent->balance = child->balance = 0;
    }
    grandchild->balance = 0;

    return grandchild;
}

static AVLNode *avltree_rotate_leftright(AVLNode *parent, AVLNode *child) {
    AVLNode *grandchild = child->right;

    child->right = grandchild->left;
    grandchild->left = child;
    if (child->right != NULL)
        child->right->parent = child;
    child->parent = grandchild;

    parent->left = grandchild->right;
    grandchild->right = parent;
    parent->parent = grandchild;
    if (parent->left != NULL)
        parent->left->parent = parent;

    if (grandchild->balance < 0) {
        parent->balance = +1;
        child->balance = 0;
    } else if (grandchild->balance > 0) {
        parent->balance = 0;
        child->balance = -1;
    } else {
        parent->balance = child->balance = 0;
    }
    grandchild->balance = 0;

    return grandchild;
}

static AVLNode *avltree_insert(AVLTree *tree, const char *key, size_t key_len, void *value) {
    Binary b = {.data = (char *) key, .length = key_len};
    AVLNode *parent, *node, *inserted;
    AVLNode **nodeptr = avltree_find_helper(&tree->root, &parent, tree->compare, b);

    if (*nodeptr != NULL) {
        tree->deleter((*nodeptr)->value);
        (*nodeptr)->value = value;
        return *nodeptr;
    }

    inserted = node = *nodeptr = avltree_nodealloc(key, key_len);
    if (node == NULL)
        return NULL;

    tree->size += 1;
    node->value = value;
    node->parent = parent;

    for (; parent != NULL; parent = node->parent) {
        AVLNode *temp, *grandparent;

        if (node == parent->left) { /* Inserted as left child */
            if (parent->balance < 0) { /* Already a left-heavy tree */
                grandparent = parent->parent;
                if (node->balance > 0) /* Double rotation needed */
                    temp = avltree_rotate_leftright(parent, node);
                else
                    temp = avltree_rotate_right(parent, node);
            } else { /* Already a right-heavy or balanced tree */
                if (parent->balance > 0) {
                    parent->balance = 0;
                    break;
                }

                parent->balance = -1;
                node = parent; /* Skip node we just adjusted */
                continue;
            }
        } else { /* Inserted as right child */
            if (parent->balance > 0) { /* Already a right-heavy tree */
                grandparent = parent->parent;
                if (node->balance < 0) /* Double rotation needed */
                    temp = avltree_rotate_rightleft(parent, node);
                else
                    temp = avltree_rotate_left(parent, node);
            } else { /* Already a left-heavy or balanced tree */
                if (parent->balance < 0) {
                    parent->balance = 0;
                    break;
                }

                parent->balance = +1;
                node = parent; /* Skip node we just adjusted */
                continue;
            }
        }

        /* Could also be adapted in the double-rotation routines, but it saves code here */
        temp->parent = grandparent;
        if (grandparent != NULL) {
            if (grandparent->left == parent)
                grandparent->left = temp;
            else
                grandparent->right = temp;
        } else {
            tree->root = temp;
        }

        break;
    }

    return inserted;
}

/* Returns node after deleted node, or NULL if the node to be deleted has no successor */
static AVLNode *avltree_delete_node_helper(AVLTree *tree, AVLNode **node) {
    AVLNode *successor = avltree_inorder_next(*node);
    AVLNode save = **node, *node_replacement = NULL;

    if ((*node)->left == NULL && (*node)->right == NULL) {
        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = NULL;
    } else if ((*node)->left == NULL) { /* Right child present */
        AVLNode *right = (*node)->right;
        right->parent = (*node)->parent;

        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = right;
    } else if ((*node)->right == NULL) { /* Left child present */
        AVLNode *left = (*node)->left;
        left->parent = (*node)->parent;

        avltree_nodedestroy(*node, tree->deleter);

        node_replacement = left;
    } else { /* Both children present, successor will never be parent of node */
        /* This routine just swaps out the successor (which must have at most one child) with the node to be deleted
         * and queues the successor to be deleted instead */
        (*node)->value = successor->value;
        (*node)->key = successor->key;
        successor->value = save.value;
        successor->key = save.key;

        if (successor == successor->parent->left)
            node = &successor->parent->left;
        else
            node = &successor->parent->right;

        return avltree_delete_node_helper(tree, node);
    }

    AVLNode *n = *node, *parent = save.parent, *sibling = NULL, *grandparent = NULL, *left = NULL;
    if (parent != NULL)
        left = parent->left;
    *node = node_replacement;
    tree->size -= 1;

    int b = 0;

    for (; parent != NULL; left = grandparent != NULL? grandparent->left: NULL, parent = grandparent) {
        grandparent = parent->parent;

        if (n == left) { /* Left child is shrinking */
            if (parent->balance > 0) { /* Weighted too far to the right, rebalance */
                sibling = parent->right;
                b = sibling->balance;
                if (b < 0) /* Sibling weighted to the left, double rotate */
                    n = avltree_rotate_rightleft(parent, sibling);
                else
                    n = avltree_rotate_left(parent, sibling);
            } else {
                if (parent->balance == 0) {
                    parent->balance = +1;
                    break;
                }

                n = parent;
                n->balance = 0;
                continue;
            }
        } else { /* Right child is shrinking */
            if (parent->balance < 0) { /* Weighted too far to the left, rebalance */
                sibling = parent->left;
                b = sibling->balance;
                if (b > 0) /* Sibling weighted to the right, double rotate */
                    n = avltree_rotate_leftright(parent, sibling);
                else
                    n = avltree_rotate_right(parent, sibling);
            } else {
                if (parent->balance == 0) {
                    parent->balance = -1;
                    break;
                }

                n = parent;
                n->balance = 0;
                continue;
            }
        }

        n->parent = grandparent;
        if (grandparent != NULL) {
            if (grandparent->left == parent)
                grandparent->left = n;
            else
                grandparent->right = n;
        } else {
            tree->root = n;
        }

        if (b == 0)
            break;
    }

    return successor;
}

static AVLNode *avltree_delete_node(AVLTree *tree, AVLNode *node) {
    if (node == NULL)
        return NULL;

    AVLNode **p = &tree->root;

    if (node->parent != NULL) {
        if (node->parent->left == node)
            p = &node->parent->left;
        else
            p = &node->parent->right;
    }

    return avltree_delete_node_helper(tree, p);
}

static AVLNode *avltree_delete(AVLTree *tree, const char *key, size_t key_len) {
    return avltree_delete_node(tree, avltree_find(tree, key, key_len));
}

static void avltree_print_helper(AVLNode *node) {
    if (node == NULL)
        return;

    printf("Item: %s\n", node->key.data);
    printf(" Balance: %+d\n", node->balance);
    printf(" Parent: %s\n", node->parent? node->parent->key.data: "<not present>");
    printf(" Left child: %-20s  Right child: %s\n", node->left? node->left->key.data: "<not present>",
                                                    node->right? node->right->key.data: "<not present>");
    printf("\n");

    avltree_print_helper(node->left);
    avltree_print_helper(node->right);
}

void avltree_print(AVLTree *tree) {
    avltree_print_helper(tree->root);
}

static void avltree_is_avl_helper(AVLNode *node, int level, int *shallowest_leaf, int *deepest_leaf) {
    if (node == NULL)
        return;

    if (node->left == NULL && node->right == NULL) { /* Leaf */
        if (*shallowest_leaf < 0 || level < *shallowest_leaf)
            *shallowest_leaf = level;

        if (*deepest_leaf < level)
            *deepest_leaf = level;
    }

    avltree_is_avl_helper(node->left, level+1, shallowest_leaf, deepest_leaf);
    avltree_is_avl_helper(node->right, level+1, shallowest_leaf, deepest_leaf);
}

int avltree_is_avl(AVLTree *tree) {
    int shallow = -1, deep = -1;

    avltree_is_avl_helper(tree->root, 0, &shallow, &deep);

    return abs(deep - shallow) <= 1;
}

static int avltree_compare(AVLTree *left, AVLTree *right, Compare value_compare) {
    AVLNode *lhs = avltree_min_node(left);
    AVLNode *rhs = avltree_min_node(right);

    while (lhs && rhs) {
        int cmp = left->compare(&lhs->key, &rhs->key);
        if (cmp)
            return cmp;

        if (value_compare) {
            cmp = value_compare(lhs->value, rhs->value);
            if (cmp)
                return cmp;
        }

        lhs = avltree_inorder_next(lhs);
        rhs = avltree_inorder_next(rhs);
    }

    if (lhs)
        return 1;
    else if (rhs)
        return -1;
    else
        return 0;
}

static void avltree_clear(AVLTree *tree) {
    avltree_destroy_helper(tree->root, tree->deleter);
    tree->root = NULL;
    tree->size = 0;
}

static size_t avltree_size(AVLTree *tree) {
    return tree->size;
}

BinarySet binaryset_create() {
    return binaryset_create_custom(NULL);
}

BinarySet binaryset_create_custom(BinaryCompare compare) {
    BinarySet set = (BinarySet) avltree_create();
    if (set)
        avltree_set_compare_fn((AVLTree *) set, compare);

    return set;
}

BinarySet binaryset_from_binarylist(BinaryList list) {
    BinarySet set = binaryset_create_custom(binarylist_get_compare_fn(list));

    if (set) {
        const Binary *array = binarylist_array(list);
        for (size_t i = 0; i < binarylist_size(list); ++i) {
            if (binaryset_add(set, array[i].data, array[i].length)) {
                binaryset_destroy(set);
                return NULL;
            }
        }
    }

    return set;
}

BinarySet binaryset_copy(BinarySet other) {
    return (BinarySet) avltree_copy((AVLTree *) other);
}

BinarySet binaryset_union(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            err = binaryset_add_binary(result, rhs_value);
            rhs = binaryset_next(b, rhs);
        } else {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }

        if (err) {
            binaryset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(a, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(a, lhs);
    }

    while (rhs) {
        if (binaryset_add_binary(result, binaryset_value_of(b, rhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        rhs = binaryset_next(b, rhs);
    }

    return result;
}

BinarySet binaryset_intersection(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);

        if (cmp < 0) {
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            rhs = binaryset_next(b, rhs);
        } else {
            if (binaryset_add_binary(result, lhs_value)) {
                binaryset_destroy(result);
                return NULL;
            }
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }
    }

    return result;
}

BinarySet binaryset_subtract(BinarySet from, BinarySet set_to_subtract) {
    BinaryCompare compare = binaryset_get_compare_fn(from);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(from);
    Iterator rhs = binaryset_begin(set_to_subtract);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(from, lhs);
        const Binary rhs_value = binaryset_value_of(set_to_subtract, rhs);
        int cmp = compare(&lhs_value, &rhs_value);

        if (cmp < 0) {
            if (binaryset_add_binary(result, lhs_value)) {
                binaryset_destroy(result);
                return NULL;
            }
            lhs = binaryset_next(from, lhs);
        } else if (cmp > 0) {
            rhs = binaryset_next(set_to_subtract, rhs);
        } else {
            lhs = binaryset_next(from, lhs);
            rhs = binaryset_next(set_to_subtract, rhs);
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(from, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(from, lhs);
    }

    return result;
}

BinarySet binaryset_difference(BinarySet a, BinarySet b) {
    BinaryCompare compare = binaryset_get_compare_fn(a);
    BinarySet result = binaryset_create_custom(compare);
    if (result == NULL)
        return NULL;

    Iterator lhs = binaryset_begin(a);
    Iterator rhs = binaryset_begin(b);
    while (lhs && rhs) {
        const Binary lhs_value = binaryset_value_of(a, lhs);
        const Binary rhs_value = binaryset_value_of(b, rhs);
        int cmp = compare(&lhs_value, &rhs_value);
        int err = 0;

        if (cmp < 0) {
            err = binaryset_add_binary(result, lhs_value);
            lhs = binaryset_next(a, lhs);
        } else if (cmp > 0) {
            err = binaryset_add_binary(result, rhs_value);
            rhs = binaryset_next(b, rhs);
        } else {
            lhs = binaryset_next(a, lhs);
            rhs = binaryset_next(b, rhs);
        }

        if (err) {
            binaryset_destroy(result);
            return NULL;
        }
    }

    while (lhs) {
        if (binaryset_add_binary(result, binaryset_value_of(a, lhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        lhs = binaryset_next(a, lhs);
    }

    while (rhs) {
        if (binaryset_add_binary(result, binaryset_value_of(b, rhs))) {
            binaryset_destroy(result);
            return NULL;
        }

        rhs = binaryset_next(b, rhs);
    }

    return result;
}

int binaryset_add(BinarySet set, const char *item, size_t item_len) {
    if (avltree_insert((AVLTree *) set, item, item_len, NULL) == NULL)
        return CC_ENOMEM;

    return 0;
}

int binaryset_add_binary(BinarySet set, const Binary item) {
    return binaryset_add(set, item.data, item.length);
}

int binaryset_contains(BinarySet set, const char *item, size_t item_len) {
    return avltree_find((AVLTree *) set, item, item_len) != NULL;
}

int binaryset_contains_binary(BinarySet set, const Binary item) {
    return binaryset_contains(set, item.data, item.length);
}

void binaryset_remove(BinarySet set, const char *item, size_t item_len) {
    avltree_delete((AVLTree *) set, item, item_len);
}

void binaryset_remove_binary(BinarySet set, const Binary item) {
    binaryset_remove(set, item.data, item.length);
}

Iterator binaryset_erase(BinarySet set, Iterator it) {
    return avltree_delete_node((AVLTree *) set, (AVLNode *) it);
}

Iterator binaryset_begin(BinarySet set) {
    return avltree_min_node((AVLTree *) set);
}

Iterator binaryset_next(BinarySet set, Iterator it) {
    UNUSED(set)

    if (it == NULL)
        return NULL;

    return avltree_inorder_next((AVLNode *) it);
}

Binary binaryset_value_of(BinarySet set, Iterator it) {
    UNUSED(set)

    return ((AVLNode *) it)->key;
}

size_t binaryset_size(BinarySet set) {
    return avltree_size((AVLTree *) set);
}

int binaryset_compare(BinarySet lhs, BinarySet rhs) {
    return avltree_compare((AVLTree *) lhs, (AVLTree *) rhs, NULL);
}

BinaryCompare binaryset_get_compare_fn(BinarySet set) {
    return avltree_get_compare_fn((AVLTree *) set);
}

void binaryset_set_compare_fn(BinarySet set, BinaryCompare compare) {
    avltree_set_compare_fn((AVLTree *) set, compare);
}

void binaryset_clear(BinarySet set) {
    avltree_clear((AVLTree *) set);
}

void binaryset_destroy(BinarySet set) {
    avltree_destroy((AVLTree *) set);
}

StringSet stringset_create() {
    return (StringSet) binaryset_create();
}

StringSet stringset_create_custom(BinaryCompare compare) {
    return (StringSet) binaryset_create_custom(compare);
}

StringSet stringset_from_stringlist(StringList list, BinaryCompare compare) {
    StringSet set = stringset_create_custom(compare);

    if (set) {
        char **array = stringlist_array(list);
        for (size_t i = 0; i < stringlist_size(list); ++i) {
            if (stringset_add(set, array[i])) {
                stringset_destroy(set);
                return NULL;
            }
        }
    }

    return set;
}

StringSet stringset_copy(StringSet other) {
    return (StringSet) binaryset_copy((BinarySet) other);
}

StringSet stringset_union(StringSet a, StringSet b) {
    return (StringSet) binaryset_union((BinarySet) a, (BinarySet) b);
}

StringSet stringset_intersection(StringSet a, StringSet b) {
    return (StringSet) binaryset_intersection((BinarySet) a, (BinarySet) b);
}

StringSet stringset_subtract(StringSet from, StringSet set_to_subtract) {
    return (StringSet) binaryset_subtract((BinarySet) from, (BinarySet) set_to_subtract);
}

StringSet stringset_difference(StringSet a, StringSet b) {
    return (StringSet) binaryset_difference((BinarySet) a, (BinarySet) b);
}

int stringset_add(StringSet set, const char *item) {
    return binaryset_add((BinarySet) set, item, strlen(item));
}

int stringset_add_n(StringSet set, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    return binaryset_add((BinarySet) set, item, item_len);
}

int stringset_contains(StringSet set, const char *item) {
    return binaryset_contains((BinarySet) set, item, strlen(item));
}

void stringset_remove(StringSet set, const char *item) {
    binaryset_remove((BinarySet) set, item, strlen(item));
}

Iterator stringset_erase(StringSet set, Iterator it) {
    return binaryset_erase((BinarySet) set, it);
}

Iterator stringset_begin(StringSet set) {
    return binaryset_begin((BinarySet) set);
}

Iterator stringset_next(StringSet set, Iterator it) {
    return binaryset_next((BinarySet) set, it);
}

const char *stringset_value_of(StringSet set, Iterator it) {
    return binaryset_value_of((BinarySet) set, it).data;
}

size_t stringset_size(StringSet set) {
    return binaryset_size((BinarySet) set);
}

int stringset_compare(StringSet lhs, StringSet rhs) {
    return binaryset_compare((BinarySet) lhs, (BinarySet) rhs);
}

BinaryCompare stringset_get_compare_fn(StringSet set) {
    return binaryset_get_compare_fn((BinarySet) set);
}

void stringset_set_compare_fn(StringSet set, BinaryCompare compare) {
    binaryset_set_compare_fn((BinarySet) set, compare);
}

void stringset_clear(StringSet set) {
    binaryset_clear((BinarySet) set);
}

void stringset_destroy(StringSet set) {
    binaryset_destroy((BinarySet) set);
}

GenericMap genericmap_create(BinaryCompare compare, Copier copy, Deleter deleter) {
    AVLTree *tree = avltree_create();
    if (tree == NULL)
        return NULL;

    avltree_set_compare_fn(tree, compare);
    avltree_set_copier_fn(tree, copy);
    avltree_set_deleter_fn(tree, deleter);

    return (GenericMap) tree;
}

GenericMap genericmap_copy(GenericMap other) {
    return (GenericMap) avltree_copy((AVLTree *) other);
}

int genericmap_insert_move(GenericMap map, const char *key, size_t key_len, void *item) {
    if (avltree_insert((AVLTree *) map, key, key_len, item) == NULL)
        return CC_ENOMEM;

    return 0;
}

int genericmap_insert(GenericMap map, const char *key, size_t key_len, const void *item) {
    void *duplicate = avltree_get_copier_fn((AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_insert_move(map, key, key_len, duplicate);
    if (err)
        avltree_get_deleter_fn((AVLTree *) map)(duplicate);

    return err;
}

int genericmap_replace_move(GenericMap map, Iterator it, void *item) {
    AVLNode *node = it;

    avltree_get_deleter_fn((AVLTree *) map)(node->value);
    node->value = item;
    return 0;
}

int genericmap_replace(GenericMap map, Iterator it, const void *item) {
    void *duplicate = avltree_get_copier_fn((AVLTree *) map)(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericmap_replace_move(map, it, duplicate);
    if (err)
        avltree_get_deleter_fn((AVLTree *) map)(duplicate);

    return err;
}

int genericmap_contains(GenericMap map, const char *key, size_t key_len) {
    return avltree_find((AVLTree *) map, key, key_len) != NULL;
}

Iterator genericmap_find(GenericMap map, const char *key, size_t key_len) {
    return avltree_find((AVLTree *) map, key, key_len);
}

void genericmap_remove(GenericMap map, const char *key, size_t key_len) {
    avltree_delete((AVLTree *) map, key, key_len);
}

Iterator genericmap_erase(GenericMap map, Iterator it) {
    return avltree_delete_node((AVLTree *) map, (AVLNode *) it);
}

Iterator genericmap_begin(GenericMap map) {
    return avltree_min_node((AVLTree *) map);
}

Iterator genericmap_next(GenericMap map, Iterator it) {
    UNUSED(map)

    return avltree_inorder_next((AVLNode *) it);
}

Binary genericmap_key_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return ((AVLNode *) it)->key;
}

void *genericmap_value_of(GenericMap map, Iterator it) {
    UNUSED(map)

    return ((AVLNode *) it)->value;
}

void *genericmap_value_of_key(GenericMap map, const char *key, size_t key_len) {
    Iterator it = genericmap_find(map, key, key_len);
    if (it == NULL)
        return NULL;

    return genericmap_value_of(map, it);
}

int genericmap_compare(GenericMap lhs, GenericMap rhs, Compare value_compare) {
    return avltree_compare((AVLTree *) lhs, (AVLTree *) rhs, value_compare);
}

size_t genericmap_size(GenericMap map) {
    return avltree_size((AVLTree *) map);
}

BinaryCompare genericmap_get_compare_fn(GenericMap map) {
    return avltree_get_compare_fn((AVLTree *) map);
}

void genericmap_set_compare_fn(GenericMap map, BinaryCompare compare) {
    avltree_set_compare_fn((AVLTree *) map, compare);
}

Copier genericmap_get_copier_fn(GenericMap map) {
    return avltree_get_copier_fn((AVLTree *) map);
}

void genericmap_set_copier_fn(GenericMap map, Copier copier) {
    avltree_set_copier_fn((AVLTree *) map, copier);
}

Deleter genericmap_get_deleter_fn(GenericMap map) {
    return avltree_get_deleter_fn((AVLTree *) map);
}

void genericmap_set_deleter_fn(GenericMap map, Deleter deleter) {
    avltree_set_deleter_fn((AVLTree *) map, deleter);
}

void genericmap_clear(GenericMap map) {
    avltree_clear((AVLTree *) map);
}

void genericmap_destroy(GenericMap map) {
    avltree_destroy((AVLTree *) map);
}

StringMap stringmap_create() {
    return stringmap_create_custom(NULL);
}

StringMap stringmap_create_custom(BinaryCompare compare) {
    return (StringMap) genericmap_create(compare, (Copier) strdup, (Deleter) FREE);
}

StringMap stringmap_copy(StringMap other) {
    return (StringMap) genericmap_copy((GenericMap) other);
}

int stringmap_insert_move(StringMap map, const char *key, char *item) {
    return genericmap_insert_move((GenericMap) map, key, strlen(key), item);
}

int stringmap_insert(StringMap map, const char *key, const char *item) {
    return genericmap_insert((GenericMap) map, key, strlen(key), item);
}

int stringmap_insert_n(StringMap map, const char *key, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = stringmap_insert_move(map, key, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringmap_replace_move(StringMap map, Iterator it, char *item) {
    return genericmap_replace_move((GenericMap) map, it, item);
}

int stringmap_replace(StringMap map, Iterator it, const char *item) {
    return genericmap_replace((GenericMap) map, it, item);
}

int stringmap_replace_n(StringMap map, Iterator it, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = stringmap_replace_move(map, it, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringmap_contains(StringMap map, const char *key) {
    return genericmap_contains((GenericMap) map, key, strlen(key));
}

void stringmap_remove(StringMap map, const char *key) {
    genericmap_remove((GenericMap) map, key, strlen(key));
}

Iterator stringmap_find(StringMap map, const char *key) {
    return genericmap_find((GenericMap) map, key, strlen(key));
}

Iterator stringmap_erase(StringMap map, Iterator it) {
    return genericmap_erase((GenericMap) map, it);
}

Iterator stringmap_begin(StringMap map) {
    return genericmap_begin((GenericMap) map);
}

Iterator stringmap_next(StringMap map, Iterator it) {
    return genericmap_next((GenericMap) map, it);
}

const char *stringmap_key_of(StringMap map, Iterator it) {
    return genericmap_key_of((GenericMap) map, it).data;
}

char *stringmap_value_of(StringMap map, Iterator it) {
    return genericmap_value_of((GenericMap) map, it);
}

char *stringmap_value_of_key(StringMap map, const char *key) {
    return genericmap_value_of_key((GenericMap) map, key, strlen(key));
}

size_t stringmap_size(StringMap map) {
    return genericmap_size((GenericMap) map);
}

int stringmap_compare(StringMap lhs, StringMap rhs, StringCompare value_compare) {
    return genericmap_compare((GenericMap) lhs, (GenericMap) rhs, (Compare) (value_compare? value_compare: strcmp));
}

BinaryCompare stringmap_get_compare_fn(StringMap map) {
    return genericmap_get_compare_fn((GenericMap) map);
}

void stringmap_set_compare_fn(StringMap map, BinaryCompare compare) {
    genericmap_set_compare_fn((GenericMap) map, compare);
}

void stringmap_clear(StringMap map) {
    genericmap_clear((GenericMap) map);
}

void stringmap_destroy(StringMap map) {
    genericmap_destroy((GenericMap) map);
}

struct GenericListStruct {
    void **array;
    Compare compare;
    Copier copy;
    Deleter deleter;
    size_t array_size;
    size_t array_capacity;
};

struct BinaryListStruct {
    Binary *array;
    BinaryCompare compare;
    size_t array_size;
    size_t array_capacity;
};

GenericList genericlist_create(Compare compare, Copier copy, Deleter deleter) {
    return genericlist_create_reserve(0, compare, copy, deleter);
}

GenericList genericlist_create_reserve(size_t reserve, Compare compare, Copier copy, Deleter deleter) {
    if (compare == NULL || copy == NULL)
        return NULL;

    GenericList list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = MAX(8, reserve);
    list->array = MALLOC(list->array_capacity * sizeof(*list->array));
    list->array[0] = NULL;
    list->compare = compare;
    list->copy = copy;
    list->deleter = deleter? deleter: (Deleter) FREE;

    return list;
}

GenericList genericlist_copy(GenericList other) {
    void **array = genericlist_array(other);
    GenericList list = genericlist_create_reserve(other->array_size, other->compare, other->copy, other->deleter);
    if (!list)
        return NULL;

    for (size_t i = 0; i < genericlist_size(other); ++i)
        if (genericlist_append(list, array[i])) {
            genericlist_destroy(list);
            return NULL;
        }

    return list;
}

GenericList genericlist_concatenate(GenericList left, GenericList right) {
    GenericList result = genericlist_create_reserve(genericlist_size(left) + genericlist_size(right),
                                                    genericlist_get_compare_fn(left),
                                                    genericlist_get_copier_fn(left),
                                                    genericlist_get_deleter_fn(left));

    if (result == NULL)
        return NULL;

    void **array = genericlist_array(left);
    for (size_t i = 0; i < genericlist_size(left); ++i)
        if (genericlist_append(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    array = genericlist_array(right);
    for (size_t i = 0; i < genericlist_size(right); ++i)
        if (genericlist_append(result, array[i])) {
            genericlist_destroy(result);
            return NULL;
        }

    return result;
}

GenericList genericlist_from_genericmap_values(GenericMap other, Compare compare) {
    GenericList list = genericlist_create_reserve(genericmap_size(other),
                                                  compare,
                                                  genericmap_get_copier_fn(other),
                                                  genericmap_get_deleter_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        if (genericlist_append(list, genericmap_value_of(other, it))) {
            genericlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

GenericList genericlist_create_filled(const void *item, size_t size, Compare compare, Copier copy, Deleter deleter) {
    GenericList list = genericlist_create_reserve(size, compare, copy, deleter);
    if (!list || genericlist_fill(list, item, size)) {
        genericlist_destroy(list);
        return NULL;
    }

    return list;
}

static int genericlist_grow(GenericList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_size + added) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size || new_capacity < list->array_capacity)
            return CC_ENOMEM;

        void **new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

int genericlist_fill(GenericList list, const void *item, size_t size) {
    size_t fill_size = MIN(genericlist_size(list), size);

    int error = genericlist_resize(list, size, item);
    if (error)
        return error;

    for (size_t i = 0; i < fill_size; ++i) {
        void *duplicate = list->copy(item);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        list->deleter(list->array[i]);
        list->array[i] = duplicate;
    }

    return 0;
}

int genericlist_resize(GenericList list, size_t size, const void *empty_item) {
    size_t original_size = genericlist_size(list);

    if (size > original_size) {
        int error = genericlist_grow(list, size - original_size);
        if (error)
            return error;

        for (size_t i = original_size; i < size; ++i) {
            void *duplicate = list->copy(empty_item);
            if (duplicate == NULL && empty_item != NULL) {
                list->array_size = i;
                goto cleanup;
            }

            list->array[i] = duplicate;
        }
    } else if (size < original_size) {
        for (size_t i = size; i < original_size; ++i) {
            list->deleter(list->array[i]);
        }
    }

    list->array_size = size;
    list->array[list->array_size] = NULL;

    return 0;

cleanup:
    for (size_t i = original_size; i < list->array_size; ++i) {
        list->deleter(list->array[i]);
    }

    list->array_size = original_size;
    list->array[list->array_size] = NULL;

    return CC_ENOMEM;
}

int genericlist_append_list(GenericList list, GenericList other) {
    int err = 0;
    /* Sizes are stored in variables because if `list` and `other` point to the same list, the sizes will change when items are added */
    size_t original_size = genericlist_size(list);
    size_t other_size = genericlist_size(other);

    for (size_t i = 0; i < other_size; ++i)
        if ((err = genericlist_append(list, genericlist_array(other)[i])) != 0)
            goto cleanup;

    return 0;

cleanup:
    for (size_t i = original_size; i < genericlist_size(list); ++i) {
        list->deleter(list->array[i]);
    }
    list->array_size = original_size;
    list->array[list->array_size] = NULL;
    return err;
}

int genericlist_append_move(GenericList list, void *item) {
    int error = genericlist_grow(list, 1);
    if (error)
        return error;

    list->array[list->array_size++] = item;
    list->array[list->array_size] = NULL;
    return 0;
}

int genericlist_append(GenericList list, const void *item) {
    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_append_move(list, duplicate);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_insert_list(GenericList list, GenericList other, size_t before_index) {
    if (before_index >= genericlist_size(list))
        before_index = genericlist_size(list);

    const size_t original_size = genericlist_size(list);
    const size_t other_size = genericlist_size(other);
    size_t rollback_location = 0;
    int err = genericlist_grow(list, other_size);
    if (err)
        return err;

    /* One is added at the end of the move so the trailing NULL will be moved as well */
    memmove(list->array + before_index + other_size, list->array + before_index, sizeof(*list->array) * (original_size - before_index + 1));

    if (list == other) { /* Inserting list into itself */
        for (size_t i = 0; i < original_size; ++i) {
            size_t source_index = i;
            size_t destination_index = before_index + i;

            if (source_index >= before_index)
                source_index += original_size;

            const void *other_item = list->array[source_index];
            void *duplicate = list->copy(other_item);
            if (duplicate == NULL && other_item != NULL) {
                rollback_location = destination_index;
                goto cleanup;
            }

            list->array[destination_index] = duplicate;
        }
    } else {
        for (size_t i = before_index; i < before_index + other_size; ++i) {
            const void *other_item = other->array[i-before_index];
            void *duplicate = list->copy(other_item);
            if (duplicate == NULL && other_item != NULL) {
                rollback_location = i;
                goto cleanup;
            }

            list->array[i] = duplicate;
        }
    }

    list->array_size += other_size;
    list->array[list->array_size] = NULL;

    return 0;

cleanup:
    for (size_t i = before_index; i < rollback_location; ++i) {
        list->deleter(list->array[i]);
    }

    /* One is added at the end of the move so the trailing NULL will be moved back as well */
    memmove(list->array + before_index, list->array + before_index + other_size, sizeof(*list->array) * (original_size - before_index + 1));

    return CC_ENOMEM;
}

int genericlist_insert_move(GenericList list, void *item, size_t before_index) {
    if (before_index >= genericlist_size(list))
        before_index = genericlist_size(list);

    int error = genericlist_grow(list, 1);
    if (error)
        return error;

    /* Make space in list */
    memmove(list->array + before_index + 1, list->array + before_index, (genericlist_size(list) - before_index) * sizeof(*list->array));

    /* Insert into list */
    list->array[before_index] = item;
    list->array[list->array_size] = NULL;
    return 0;
}

int genericlist_insert(GenericList list, const void *item, size_t before_index) {
    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_insert_move(list, duplicate, before_index);
    if (err)
        list->deleter(duplicate);

    return err;
}

int genericlist_replace_move_at(GenericList list, size_t index, void *item) {
    if (index >= list->array_size)
        return CC_EINVAL;

    list->deleter(list->array[index]);
    list->array[index] = item;
    return 0;
}

int genericlist_replace_at(GenericList list, size_t index, const void *item) {
    void *duplicate = list->copy(item);
    if (duplicate == NULL && item != NULL)
        return CC_ENOMEM;

    int err = genericlist_replace_move_at(list, index, duplicate);
    if (err)
        list->deleter(duplicate);

    return err;
}

size_t genericlist_remove_at(GenericList list, size_t index) {
    return genericlist_erase(list, index, index+1);
}

size_t genericlist_remove_one(GenericList list, const void *item) {
    size_t index = genericlist_find(list, item, 0);
    if (index == SIZE_MAX)
        return 0;

    return genericlist_remove_at(list, index);
}

size_t genericlist_remove_all(GenericList list, const void *item) {
    size_t count = 0;
    size_t index = 0;

    while (1) {
        index = genericlist_find(list, item, index);
        if (index == SIZE_MAX)
            return count;

        count += genericlist_remove_at(list, index);
    }
}

size_t genericlist_erase(GenericList list, size_t begin_index, size_t end_index) {
    if (begin_index >= genericlist_size(list))
        return 0;

    if (end_index > genericlist_size(list))
        end_index = genericlist_size(list);

    size_t length = end_index - begin_index;
    for (size_t i = begin_index; i < end_index; ++i) {
        list->deleter(list->array[i]);
    }

    memmove(list->array + begin_index, list->array + end_index, (genericlist_size(list) - end_index) * sizeof(*list->array));
    list->array_size -= length;
    list->array[list->array_size] = NULL;

    return length;
}

void **genericlist_array(GenericList list) {
    return list->array;
}

int genericlist_contains(GenericList list, const void *item) {
    return genericlist_find(list, item, 0) != SIZE_MAX;
}

size_t genericlist_find(GenericList list, const void *item, size_t begin_index) {
    for (size_t i = begin_index; i < genericlist_size(list); ++i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t genericlist_rfind(GenericList list, const void *item, size_t begin_index) {
    if (begin_index >= genericlist_size(list))
        begin_index = genericlist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->compare(genericlist_array(list)[i], item) == 0)
            return i;
    }

    return SIZE_MAX;
}

int genericlist_compare(GenericList list, GenericList other) {
    size_t max = MIN(genericlist_size(list), genericlist_size(other));

    for (size_t i = 0; i < max; ++i) {
        int cmp = list->compare(genericlist_array(list)[i], genericlist_array(other)[i]);

        if (cmp)
            return cmp;
    }

    if (genericlist_size(other) != max)
        return -1;
    else if (genericlist_size(list) != max)
        return 1;

    return 0;
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_insertion_sort(void **base, size_t num, int descending, int (*compar)(const void *, const void *)) {
    for (size_t i = 1; i < num; ++i) {
        for (size_t j = i; j > 0; --j) {
            if (compar(base[j], base[j-1]) * descending >= 0)
                break;

            void *temp = base[j];
            base[j] = base[j-1];
            base[j-1] = temp;
        }
    }
}

/* descending must be -1 (descending) or 1 (ascending) */
static void genericlist_merge_helper(void **result, void **base, size_t begin, size_t pivot, size_t end, int descending, int (*compar)(const void *, const void *)) {
    size_t left = begin, right = pivot;

    for (size_t i = begin; i < end; ++i) {
        if (left < pivot && (right >= end || compar(base[left], base[right]) * descending <= 0))
            result[i] = base[left++];
        else
            result[i] = base[right++];
    }
}

static void genericlist_merge_sort_helper(void **result, void **base, size_t begin, size_t end, int descending, int (*compar)(const void *, const void *)) {
    if (end - begin <= 4) {
        genericlist_insertion_sort(base + begin, end - begin, descending, compar);
        return;
    }

    size_t pivot = begin + (end - begin) / 2;

    genericlist_merge_sort_helper(base, result, begin, pivot, descending, compar);
    genericlist_merge_sort_helper(base, result, pivot, end, descending, compar);
    genericlist_merge_helper(base, result, begin, pivot, end, descending, compar);
}

GenericList genericlist_sorted(GenericList list, int descending) {
    return genericlist_stable_sorted(list, descending);
}

GenericList genericlist_stable_sorted(GenericList list, int descending) {
    GenericList copy = genericlist_copy(list);
    if (copy == NULL || genericlist_stable_sort(copy, descending)) {
        genericlist_destroy(copy);
        return NULL;
    }

    return copy;
}

int genericlist_sort(GenericList list, int descending) {
    return genericlist_stable_sort(list, descending);
}

int genericlist_stable_sort(GenericList list, int descending) {
    void **temp = MALLOC(genericlist_size(list) * sizeof(*temp));
    if (temp == NULL) {
        FREE(temp);
        return CC_ENOMEM;
    }

    memcpy(temp, list->array, genericlist_size(list) * sizeof(*temp));
    genericlist_merge_sort_helper(temp, list->array, 0, genericlist_size(list), descending? -1: 1, list->compare);

    FREE(temp);

    return 0;
}

void genericlist_reserve(GenericList list, size_t size) {
    if (size <= genericlist_size(list))
        return;

    genericlist_grow(list, size - genericlist_size(list));
}

Iterator genericlist_begin(GenericList list) {
    return list->array;
}

Iterator genericlist_next(GenericList list, Iterator it) {
    if (it == NULL || genericlist_size(list) == 0 || it == list->array + genericlist_size(list) - 1)
        return NULL;

    return ((void **) it) + 1;
}

void *genericlist_value_of(GenericList list, Iterator it) {
    UNUSED(list)

    return *((void **) it);
}

size_t genericlist_size(GenericList list) {
    return list->array_size;
}

Compare genericlist_get_compare_fn(GenericList list) {
    return list->compare;
}

void genericlist_set_compare_fn(GenericList list, Compare compare) {
    if (compare != NULL)
        list->compare = compare;
}

Copier genericlist_get_copier_fn(GenericList list) {
    return list->copy;
}

void genericlist_set_copier_fn(GenericList list, Copier copier) {
    if (copier != NULL)
        list->copy = copier;
}

Deleter genericlist_get_deleter_fn(GenericList list) {
    return list->deleter;
}

void genericlist_set_deleter_fn(GenericList list, Deleter deleter) {
    if (deleter != NULL)
        list->deleter = deleter;
}

void genericlist_clear(GenericList list) {
    for (size_t i = 0; i < genericlist_size(list); ++i)
        list->deleter(list->array[i]);

    list->array_size = 0;
    list->array[0] = NULL;
}

void genericlist_destroy(GenericList list) {
    if (list) {
        for (size_t i = 0; i < genericlist_size(list); ++i)
            list->deleter(list->array[i]);

        FREE(list->array);
        FREE(list);
    }
}

StringList stringlist_create() {
    return stringlist_create_custom((StringCompare) strcmp);
}

StringList stringlist_create_custom(StringCompare compare) {
    return stringlist_create_reserve(0, compare);
}

StringList stringlist_create_reserve(size_t reserve, StringCompare compare) {
    return (StringList) genericlist_create_reserve(reserve, (Compare) compare, (Copier) strdup, (Deleter) FREE);
}

StringList stringlist_concatenate(StringList left, StringList right) {
    return (StringList) genericlist_concatenate((GenericList) left, (GenericList) right);
}

StringList stringlist_split(const char *string, const char *separator, int keep_empty) {
    int done = 0;
    size_t separatorLen = strlen(separator);
    StringList list = stringlist_create();
    if (!list)
        return NULL;

    if (!separatorLen) { /* Empty separator, separate every character */
        while (*string) {
            if (stringlist_append_n(list, string, 1))
                goto cleanup;

            ++string;
        }

        return list;
    }

    while (!done) {
        const char *separator_location = strstr(string, separator);
        if (separator_location == NULL) {
            done = 1;
            separator_location = string + strlen(string);
        }

        size_t item_len = separator_location - string;
        if (item_len || keep_empty) {
            if (stringlist_append_n(list, string, separator_location - string))
                goto cleanup;
        }

        string = separator_location + separatorLen;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

StringList stringlist_copy(StringList other) {
    return (StringList) genericlist_copy((GenericList) other);
}

StringList stringlist_from_stringset(StringSet other) {
    StringList list = stringlist_create_reserve(stringset_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringset_begin(other); it; it = stringset_next(other, it)) {
        if (stringlist_append(list, stringset_value_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_from_binarylist(BinaryList other) {
    Binary *array = binarylist_array(other);
    StringList list = stringlist_create_reserve(binarylist_size(other), NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; i < binarylist_size(other); ++i) {
        if (memchr(array[i].data, 0, array[i].length) != NULL || stringlist_append(list, array[i].data))
            goto cleanup;
    }

    return list;

cleanup:
    stringlist_destroy(list);
    return NULL;
}

StringList stringlist_from_genericmap_keys(GenericMap other) {
    StringList list = stringlist_create_reserve(genericmap_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        Binary key = genericmap_key_of(other, it);
        if (memchr(key.data, 0, key.length) != NULL || stringlist_append(list, key.data)) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_from_stringmap_keys(StringMap other) {
    StringList list = stringlist_create_reserve(stringmap_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        if (stringlist_append(list, stringmap_key_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_from_stringmap_values(StringMap other) {
    StringList list = stringlist_create_reserve(stringmap_size(other), NULL);
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        if (stringlist_append(list, stringmap_value_of(other, it))) {
            stringlist_destroy(list);
            return NULL;
        }
    }

    return list;
}

StringList stringlist_create_filled(const char *item, size_t size) {
    return (StringList) genericlist_create_filled(item, size, (Compare) strcmp, (Copier) strdup, (Deleter) FREE);
}

int stringlist_fill(StringList list, const char *item, size_t size) {
    return genericlist_fill((GenericList) list, item, size);
}

int stringlist_resize(StringList list, size_t size, const char *empty_item) {
    return genericlist_resize((GenericList) list, size, empty_item);
}

int stringlist_append_list(StringList list, StringList other) {
    return genericlist_append_list((GenericList) list, (GenericList) other);
}

int stringlist_append_move(StringList list, char *item) {
    return genericlist_append_move((GenericList) list, item);
}

int stringlist_append(StringList list, const char *item) {
    return genericlist_append((GenericList) list, item);
}

int stringlist_append_n(StringList list, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_append_move((GenericList) list, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringlist_insert_list(StringList list, StringList other, size_t before_index) {
    return genericlist_insert_list((GenericList) list, (GenericList) other, before_index);
}

int stringlist_insert_move(StringList list, char *item, size_t before_index) {
    return genericlist_insert_move((GenericList) list, item, before_index);
}

int stringlist_insert(StringList list, const char *item, size_t before_index) {
    return genericlist_insert((GenericList) list, item, before_index);
}

int stringlist_insert_n(StringList list, const char *item, size_t item_len, size_t before_index) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_insert_move((GenericList) list, duplicate, before_index);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int stringlist_replace_move_at(StringList list, size_t index, char *item) {
    return genericlist_replace_move_at((GenericList) list, index, item);
}

int stringlist_replace_at(StringList list, size_t index, const char *item) {
    return genericlist_replace_at((GenericList) list, index, item);
}

int stringlist_replace_at_n(StringList list, size_t index, const char *item, size_t item_len) {
    if (memchr(item, 0, item_len) != NULL)
        return CC_EINVAL;

    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = genericlist_replace_move_at((GenericList) list, index, duplicate);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

size_t stringlist_remove_at(StringList list, size_t index) {
    return genericlist_remove_at((GenericList) list, index);
}

size_t stringlist_remove_one(StringList list, const char *item) {
    return genericlist_remove_one((GenericList) list, item);
}

size_t stringlist_remove_all(StringList list, const char *item) {
    return genericlist_remove_all((GenericList) list, item);
}

size_t stringlist_erase(StringList list, size_t begin_index, size_t end_index) {
    return genericlist_erase((GenericList) list, begin_index, end_index);
}

char **stringlist_array(StringList list) {
    return (char **) genericlist_array((GenericList) list);
}

int stringlist_contains(StringList list, const char *item) {
    return stringlist_find(list, item, 0) != SIZE_MAX;
}

size_t stringlist_find(StringList list, const char *item, size_t begin_index) {
    return genericlist_find((GenericList) list, item, begin_index);
}

size_t stringlist_rfind(StringList list, const char *item, size_t begin_index) {
    return genericlist_rfind((GenericList) list, item, begin_index);
}

int stringlist_compare(StringList list, StringList other) {
    return genericlist_compare((GenericList) list, (GenericList) other);
}

char *stringlist_joined_alloc(StringList list, const char *separator) {
    return strjoin_alloc((const char **) stringlist_array(list), stringlist_size(list), separator);
}

StringList stringlist_sorted(StringList list, int descending) {
    return (StringList) genericlist_sorted((GenericList) list, descending);
}

StringList stringlist_stable_sorted(StringList list, int descending) {
    return (StringList) genericlist_stable_sorted((GenericList) list, descending);
}

int stringlist_sort(StringList list, int descending) {
    return genericlist_sort((GenericList) list, descending);
}

int stringlist_stable_sort(StringList list, int descending) {
    return genericlist_stable_sort((GenericList) list, descending);
}

void stringlist_reserve(StringList list, size_t size) {
    genericlist_reserve((GenericList) list, size);
}

Iterator stringlist_begin(StringList list) {
    return genericlist_begin((GenericList) list);
}

Iterator stringlist_next(StringList list, Iterator it) {
    return genericlist_next((GenericList) list, it);
}

char *stringlist_value_of(StringList list, Iterator it) {
    return (char *) genericlist_value_of((GenericList) list, it);
}

size_t stringlist_size(StringList list) {
    return genericlist_size((GenericList) list);
}

StringCompare stringlist_get_compare_fn(StringList list) {
    return (StringCompare) genericlist_get_compare_fn((GenericList) list);
}

void stringlist_set_compare_fn(StringList list, StringCompare compare) {
    genericlist_set_compare_fn((GenericList) list, (Compare) compare);
}

void stringlist_clear(StringList list) {
    genericlist_clear((GenericList) list);
}

void stringlist_destroy(StringList list) {
    genericlist_destroy((GenericList) list);
}

static int binarylist_grow(BinaryList list, size_t added) {
    if (list->array_size+added >= list->array_capacity) {
        size_t new_capacity = MAX(list->array_capacity + (list->array_capacity / 2), list->array_size + added) + 1;
        size_t new_size = safe_multiply(new_capacity, sizeof(*list->array));
        if (!new_size || new_capacity < list->array_capacity)
            return CC_ENOMEM;

        Binary *new_array = REALLOC(list->array, new_size);
        if (!new_array)
            return CC_ENOMEM;

        list->array_capacity = new_capacity;
        list->array = new_array;
    }

    return 0;
}

BinaryList binarylist_create() {
    return binarylist_create_custom(NULL);
}

BinaryList binarylist_create_custom(BinaryCompare compare) {
    return binarylist_create_reserve(0, compare);
}

BinaryList binarylist_create_reserve(size_t reserve, BinaryCompare compare) {
    BinaryList list = CALLOC(1, sizeof(*list));
    if (!list)
        return NULL;

    list->array_capacity = MAX(8, reserve);
    list->array = MALLOC(list->array_capacity * sizeof(*list->array));
    list->array[0].data = NULL;
    list->array[0].length = 0;
    list->compare = compare? compare: binary_compare;

    return list;
}

BinaryList binarylist_split(const char *string, const char *separator, size_t separator_len, int keep_empty) {
    int done = 0;
    BinaryList list = binarylist_create();
    if (!list)
        return NULL;

    if (!separator_len) { /* Empty separator, separate every character */
        while (*string) {
            if (binarylist_append(list, string, 1))
                goto cleanup;

            ++string;
        }

        return list;
    }

    while (!done) {
        const char *separator_location = strstr(string, separator);
        if (separator_location == NULL) {
            done = 1;
            separator_location = string + strlen(string);
        }

        size_t item_len = separator_location - string;
        if (item_len || keep_empty) {
            if (binarylist_append(list, string, separator_location - string))
                goto cleanup;
        }

        string = separator_location + separator_len;
    }

    return list;

cleanup:
    binarylist_destroy(list);
    return NULL;
}

BinaryList binarylist_copy(BinaryList other) {
    Binary *array = binarylist_array(other);
    BinaryList list = binarylist_create_reserve(binarylist_size(other), binarylist_get_compare_fn(other));
    if (!list)
        return NULL;

    for (size_t i = 0; i < binarylist_size(other); ++i)
        if (binarylist_append(list, array[i].data, array[i].length)) {
            binarylist_destroy(list);
            return NULL;
        }

    return list;
}

BinaryList binarylist_concatenate(BinaryList left, BinaryList right) {
    BinaryList result = binarylist_create_reserve(binarylist_size(left) + binarylist_size(right),
                                                  binarylist_get_compare_fn(left));

    if (result == NULL)
        return NULL;

    Binary *array = binarylist_array(left);
    for (size_t i = 0; i < binarylist_size(left); ++i)
        if (binarylist_append_binary(result, array[i])) {
            binarylist_destroy(result);
            return NULL;
        }

    array = binarylist_array(right);
    for (size_t i = 0; i < binarylist_size(right); ++i)
        if (binarylist_append_binary(result, array[i])) {
            binarylist_destroy(result);
            return NULL;
        }

    return result;
}

BinaryList binarylist_from_stringlist(StringList other) {
    char **array = stringlist_array(other);
    BinaryList list = binarylist_create_reserve(stringlist_size(other), NULL);
    if (!list)
        return NULL;

    for (size_t i = 0; i < stringlist_size(other); ++i) {
        if (binarylist_append(list, array[i], strlen(array[i]))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_binaryset(BinarySet other) {
    BinaryList list = binarylist_create_reserve(binaryset_size(other), binaryset_get_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = binaryset_begin(other); it; it = binaryset_next(other, it)) {
        if (binarylist_append_binary(list, binaryset_value_of(other, it))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_genericmap_keys(GenericMap other) {
    BinaryList list = binarylist_create_reserve(genericmap_size(other), genericmap_get_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = genericmap_begin(other); it; it = genericmap_next(other, it)) {
        if (binarylist_append_binary(list, genericmap_key_of(other, it))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_from_stringmap_keys(StringMap other) {
    return binarylist_from_genericmap_keys((GenericMap) other);
}

BinaryList binarylist_from_stringmap_values(StringMap other) {
    BinaryList list = binarylist_create_reserve(stringmap_size(other), stringmap_get_compare_fn(other));
    if (!list)
        return NULL;

    for (Iterator it = stringmap_begin(other); it; it = stringmap_next(other, it)) {
        char *value = stringmap_value_of(other, it);
        if (binarylist_append(list, value, strlen(value))) {
            binarylist_destroy(list);
            return NULL;
        }
    }

    return list;
}

BinaryList binarylist_create_filled(const char *item, size_t item_len, size_t size) {
    BinaryList list = binarylist_create_reserve(size, NULL);
    if (!list || binarylist_fill(list, item, item_len, size)) {
        binarylist_destroy(list);
        return NULL;
    }

    return list;
}

BinaryList binarylist_create_filled_binary(const Binary item, size_t size) {
    return binarylist_create_filled(item.data, item.length, size);
}

int binarylist_fill(BinaryList list, const char *item, size_t item_len, size_t size) {
    size_t fill_size = MIN(binarylist_size(list), size);

    int error = binarylist_resize(list, size, item, item_len);
    if (error)
        return error;

    for (size_t i = 0; i < fill_size; ++i) {
        char *duplicate = MALLOC(item_len+1);
        if (duplicate == NULL && item != NULL)
            return CC_ENOMEM;

        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;

        FREE(list->array[i].data);
        list->array[i].data = duplicate;
        list->array[i].length = item_len;
    }

    return 0;
}

int binarylist_fill_binary(BinaryList list, const Binary item, size_t size) {
    return binarylist_fill(list, item.data, item.length, size);
}

int binarylist_resize(BinaryList list, size_t size, const char *empty_item, size_t empty_item_len) {
    size_t original_size = binarylist_size(list);

    if (size > original_size) {
        int error = binarylist_grow(list, size - original_size);
        if (error)
            return error;

        for (size_t i = original_size; i < size; ++i) {
            char *duplicate = MALLOC(empty_item_len+1);
            if (duplicate == NULL && empty_item != NULL) {
                list->array_size = i;
                goto cleanup;
            }

            memcpy(duplicate, empty_item, empty_item_len);
            duplicate[empty_item_len] = 0;

            list->array[i].data = duplicate;
            list->array[i].length = empty_item_len;
        }
    } else if (size < original_size) {
        for (size_t i = size; i < original_size; ++i) {
            FREE(list->array[i].data);
        }
    }

    list->array_size = size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return 0;

cleanup:
    for (size_t i = original_size; i < list->array_size; ++i) {
        FREE(list->array[i].data);
    }

    list->array_size = original_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return CC_ENOMEM;
}

int binarylist_append_list(BinaryList list, BinaryList other) {
    int err = 0;
    /* Sizes are stored in variables because if `list` and `other` point to the same list, the sizes will change when items are added */
    size_t original_size = binarylist_size(list);
    size_t other_size = binarylist_size(other);

    for (size_t i = 0; i < other_size; ++i)
        if ((err = binarylist_append_binary(list, binarylist_array(other)[i])) != 0)
            goto cleanup;

    return 0;

cleanup:
    for (size_t i = original_size; i < binarylist_size(list); ++i) {
        FREE(list->array[i].data);
    }
    list->array_size = original_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return err;
}

int binarylist_append_move(BinaryList list, char *item, size_t item_len) {
    int error = binarylist_grow(list, 1);
    if (error)
        return error;

    /* Insert into list */
    list->array[list->array_size].data = item;
    list->array[list->array_size].length = item_len;
    ++list->array_size;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_append(BinaryList list, const char *item, size_t item_len) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    /* Insert into list */
    int err = binarylist_append_move(list, duplicate, item_len);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_append_binary_move(BinaryList list, Binary item) {
    return binarylist_append_move(list, item.data, item.length);
}

int binarylist_append_binary(BinaryList list, const Binary item) {
    return binarylist_append(list, item.data, item.length);
}

int binarylist_insert_move(BinaryList list, char *item, size_t item_len, size_t before_index) {
    if (before_index > binarylist_size(list))
        before_index = binarylist_size(list);

    int error = binarylist_grow(list, 1);
    if (error)
        return error;

    /* Make space in list */
    memmove(list->array + before_index + 1, list->array + before_index, (binarylist_size(list) - before_index) * sizeof(*list->array));

    /* Insert into list */
    list->array[before_index].data = item;
    list->array[before_index].length = item_len;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;
    return 0;
}

int binarylist_insert(BinaryList list, const char *item, size_t item_len, size_t before_index) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = binarylist_insert_move(list, duplicate, item_len, before_index);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_insert_binary_move(BinaryList list, Binary item, size_t before_index) {
    return binarylist_insert_move(list, item.data, item.length, before_index);
}

int binarylist_insert_binary(BinaryList list, const Binary item, size_t before_index) {
    return binarylist_insert(list, item.data, item.length, before_index);
}

int binarylist_replace_move_at(BinaryList list, size_t index, char *item, size_t item_len) {
    if (index >= list->array_size)
        return CC_EINVAL;

    FREE(list->array[index].data);
    list->array[index].data = item;
    list->array[index].length = item_len;
    return 0;
}

int binarylist_replace_at(BinaryList list, size_t index, const char *item, size_t item_len) {
    /* Allocate duplicate */
    char *duplicate = MALLOC(item_len+1);
    if (!duplicate)
        return CC_ENOMEM;

    int err = binarylist_replace_move_at(list, index, duplicate, item_len);
    if (err)
        FREE(duplicate);
    else {
        /* Copy string */
        memcpy(duplicate, item, item_len);
        duplicate[item_len] = 0;
    }

    return err;
}

int binarylist_replace_binary_move_at(BinaryList list, size_t index, Binary item) {
    return binarylist_replace_move_at(list, index, item.data, item.length);
}

int binarylist_replace_binary_at(BinaryList list, size_t index, Binary item) {
    return binarylist_replace_at(list, index, item.data, item.length);
}

size_t binarylist_remove_at(BinaryList list, size_t index) {
    return binarylist_erase(list, index, index+1);
}

size_t binarylist_remove_one(BinaryList list, const char *item, size_t item_len) {
    size_t index = binarylist_find(list, item, item_len, 0);
    if (index == SIZE_MAX)
        return 0;

    return binarylist_remove_at(list, index);
}

size_t binarylist_remove_all(BinaryList list, const char *item, size_t item_len) {
    size_t count = 0;
    size_t index = 0;

    while (1) {
        index = binarylist_find(list, item, item_len, index);
        if (index == SIZE_MAX)
            return count;

        count += binarylist_remove_at(list, index);
    }
}

size_t binarylist_erase(BinaryList list, size_t begin_index, size_t end_index) {
    if (begin_index >= binarylist_size(list))
        return 0;

    if (end_index > binarylist_size(list))
        end_index = binarylist_size(list);

    size_t length = end_index - begin_index;
    for (size_t i = begin_index; i < end_index; ++i) {
        FREE(list->array[i].data);
    }

    memmove(list->array + begin_index, list->array + end_index, (binarylist_size(list) - end_index) * sizeof(*list->array));
    list->array_size -= length;
    list->array[list->array_size].data = NULL;
    list->array[list->array_size].length = 0;

    return length;
}

Binary *binarylist_array(BinaryList list) {
    return list->array;
}

int binarylist_contains(BinaryList list, const char *item, size_t item_len) {
    return binarylist_find(list, item, item_len, 0) != SIZE_MAX;
}

size_t binarylist_find(BinaryList list, const char *item, size_t item_len, size_t begin_index) {
    Binary b = {.data = (char *) item, .length = item_len};

    for (size_t i = begin_index; i < binarylist_size(list); ++i) {
        if (list->compare(&binarylist_array(list)[i], &b) == 0)
            return i;
    }

    return SIZE_MAX;
}

size_t binarylist_rfind(BinaryList list, const char *item, size_t item_len, size_t begin_index) {
    Binary b = {.data = (char *) item, .length = item_len};

    if (begin_index >= binarylist_size(list))
        begin_index = binarylist_size(list)-1;

    for (size_t i = begin_index; i != SIZE_MAX; --i) {
        if (list->compare(&binarylist_array(list)[i], &b) == 0)
            return i;
    }

    return SIZE_MAX;
}

int binarylist_compare(BinaryList list, BinaryList other) {
    size_t max = MIN(binarylist_size(list), binarylist_size(other));

    for (size_t i = 0; i < max; ++i) {
        int cmp = list->compare(&binarylist_array(list)[i], &binarylist_array(other)[i]);

        if (cmp)
            return cmp;
    }

    if (binarylist_size(other) != max)
        return -1;
    else if (binarylist_size(list) != max)
        return 1;

    return 0;
}

void binarylist_reserve(BinaryList list, size_t size) {
    if (size <= binarylist_size(list))
        return;

    binarylist_grow(list, size - binarylist_size(list));
}

Iterator binarylist_begin(BinaryList list) {
    return list->array;
}

Iterator binarylist_next(BinaryList list, Iterator it) {
    if (it == NULL || binarylist_size(list) == 0 || it == list->array + binarylist_size(list) - 1)
        return NULL;

    return ((Binary *) it) + 1;
}

Binary binarylist_value_of(BinaryList list, Iterator it) {
    UNUSED(list)

    return *((Binary *) it);
}

size_t binarylist_size(BinaryList list) {
    return list->array_size;
}

BinaryCompare binarylist_get_compare_fn(BinaryList list) {
    return list->compare;
}

void binarylist_set_compare_fn(BinaryList list, BinaryCompare compare) {
    list->compare = compare? compare: binary_compare;
}

void binarylist_clear(BinaryList list) {
    for (size_t i = 0; i < binarylist_size(list); ++i)
        FREE(list->array[i].data);

    list->array_size = 0;
    list->array[0].data = NULL;
    list->array[0].length = 0;
}

void binarylist_destroy(BinaryList list) {
    if (list) {
        for (size_t i = 0; i < binarylist_size(list); ++i)
            FREE(list->array[i].data);

        FREE(list->array);
        FREE(list);
    }
}
