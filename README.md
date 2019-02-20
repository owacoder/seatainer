# Seatainer

C Generic container implementation, supporting nested containers

### Premise

C has a very apalling lack of containers. That's because it is inherently a low-level language, and has no template ability.
Seatainer attempts to remedy this somewhat by implementing typesafe generic containers in C. Unfortunately, there is a small performance hit since
the container types are checked and determined at runtime, but if you are using C99 or newer, the memory overhead in using Seatainer is near-optimal.

### How do I use Seatainer?

#### Elements

##### Introduction

First, you'll need to know about Elements (implemented via the `HElementData` handle). Elements are the foundation for Seatainer.

Elements are a sort of variant type, allowing you to store:

 - The null type (not able to store any value)
 - All the basic C integral types: `char`, `short`, `int`, `long`, `long long`.
 - The basic C floating-point types: `float`, `double`. `long double` is not supported.
 - `void *`
 - The Seatainer container types

The ability to store other containers is what makes Seatainer so powerful. The enum values all start with `El_`
(i.e. `El_SignedInt`) to designate the element type.

##### So how do you use Elements?

Elements are the interface for interacting with *actual* elements in containers. An example of this is shown below:

    /* Create a "signed int" element */
    HElementData element = cc_el_init(El_SignedInt, NULL, NULL, NULL);

    /* Then assign a value to it (the int is initialized to 0 unless a custom constructor was provided).
     * The type will automatically changes to whatever is being assigned (with a few exceptions; please check the error return value of all functions) */
    cc_el_assign_signed_int(element, 42);

    /* Then you can retrieve the value with the get functions. Note that the get functions return pointers
     * to the actual data. If the return value is NULL, that means the element does not hold data of that type */
    printf("%d\n", *cc_el_get_signed_int(element)); /* Note that this is NOT SAFE if we can't guarantee this element contains a signed int */

    /* You can get the type of an element by calling cc_el_type() */
    ContainerElementType type = cc_el_type(element);

Elements can contain the stored value, or they can reference another storage location. This ability is critical to be able to access real elements in containers.

    /* Access the actual storage location of the type (in this case, a SignedInt) */
    signed int *signedIntLocation = cc_el_storage_location(element);

    /* Determine whether the location is local or an external reference
     * (cc_el_storage_location_ptr() is guaranteed to return non-NULL) */
    signed int *signedIntLocationPtr = *cc_el_storage_location_ptr(element);

    /* Now, if signedIntLocationPtr is NULL, the value is local to the Element.
     * If it is non-NULL, the value is stored externally, in some other data structure */
    if (signedIntLocationPtr)
        puts("signed int element references external location");
    else
        puts("signed int data is stored locally in element");

##### Overhead

Based on testing on x86 platforms, the memory overhead is 24 bytes/element on 32-bit platforms, and 32 bytes/element on 64-bit platforms.

#### Containers

##### Introduction

Containers allow you to group data in an easy-to-use way. They allow dynamic resizing, and even allow for constructors, copy-constructors, and destructors.
This ability allows you to nest containers, such as a vector of vectors of ints, without worrying about memory management.

##### Overhead



### What version of C is supported?

Seatainer officially supports C89 and newer.

### Compile flags

 - `CC_TYPE_MISMATCH_ABORT` - Define to make container type mismatches spit out an error and abort the program. If a function works on a single element, it will not abort, since elements can contain any type. It only restricts the type when being used with containers.
 - `CC_NO_MEM_ABORT` - Define to make out-of-memory conditions spit out an error and abort the program. If a function returns a pointer, it will return NULL instead of aborting.
 - `CC_BAD_PARAM_ABORT` - Define to make procedures with bad parameters spit out an error and abort the program.
 - `CC_NO_SUCH_METHOD_ABORT` - Define to make procedures with no valid method to call spit out an error and abort the program.
