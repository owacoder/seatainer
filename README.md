# Seatainer

C generic library implementation, including generic containers, generic IO interface, directory support, and support for external process management.

### Premise

C has a very apalling lack of containers. That's because it is inherently a low-level language, and has no template ability.
Seatainer attempts to remedy this somewhat by implementing generic and specialized containers in C. Unfortunately, there is a small performance hit due
to an additional layer of indirection, but the performance is near optimal for generic C code, and the containers are easy to use.

Seatainer also contains a generic IO interface. The usage is not much different than the internal `FILE *` type, so it should be very easy to use.
Seatainer's IO system even includes HTTP support. HTTPS support is available if OpenSSL is available too.

Directory, path, and URL support is also included.

There is also a module for external process management, both using native handles and using a `Process` type.

### How do I use Seatainer?

Check back for usage examples.

### What version of C is supported?

Seatainer officially supports C99 and newer.

### Compile flags

 - `CC_IO_STATIC_INSTANCES` - Define to a non-negative integer to allow fast allocation of a limited number of IO instances. If this limit is reached, the subsequent devices will be dynamically allocated. If this value is not defined, all allocations will be dynamic.
 - `CC_INCLUDE_NETWORK` - Define to specify that network access through sockets should be built.
 - `CC_INCLUDE_ZLIB` - Define to specify that the ZLib wrapper should be built.

### IO devices

A number of IO devices are supported currently:

 - AES - Actually two separate devices (one encryption, one decryption) that support AES encryption of a stream (although the stream must be in 16-byte blocks), and allow various cipher modes, IVs, and all the AES key sizes. Hardware acceleration is used where available.
 - CryptoRand - A device that reads from the system CSPRNG. The bytes returned from reading this function are available for use as a cryptographically secure random number generator. This device is not seekable.
 - Hex - Actually two separate devices (one encoding, one decoding) that support Hex encoding of a stream. These devices are seekable.
 - Md5 - A device that computes the MD5 hash of its input. This device is not seekable, but if opened for reading and writing, a rolling hash may be computed.
 - Net - Actually two separate devices (one TCP, one UDP) that support network interfacing. `io_net_init()` should be called before using any Net device (just once for the program), and `io_net_deinit()` should be called when no Net devices are needed any longer.
 - Sha1 - A device that computes the SHA-1 hash of its input. This device is not seekable, but if opened for reading and writing, a rolling hash may be computed. Hardware acceleration is used where available.
 - Tee - A device that duplicates any data written to it to two outputs. This device is not seekable.
