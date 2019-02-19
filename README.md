# seatainer
C Generic container implementation, supporting nested containers

### Premise

C has a very apalling lack of containers. That's because it is inherently a low-level language, and has no template ability.
Seatainer attempts to remedy this somewhat by implementing typesafe generic containers in C. Unfortunately, there is a small performance hit since
the container types are checked and determined at runtime, but if you are using C99 or newer, the memory overhead in using Seatainer is near-optimal.

### How do I use Seatainer?

First, you'll need to know about Elements. Elements are the foundation for Seatainer.

### What version of C is supported?

Seatainer officially supports C89 and newer.

### Compile flags

 - `CC_TYPE_MISMATCH_ABORT` - Define to make container type mismatches spit out an error and abort the program. If a function works on a single element, it will not abort, since elements can contain any type. It only restricts the type when being used with containers.
 - `CC_NO_MEM_ABORT` - Define to make out-of-memory conditions spit out an error and abort the program. If a function returns a pointer, it will return NULL instead of aborting.
 - `CC_BAD_PARAM_ABORT` - Define to make procedures with bad parameters spit out an error and abort the program.
 - `CC_NO_SUCH_METHOD_ABORT` - Define to make procedures with no valid method to call spit out an error and abort the program.
