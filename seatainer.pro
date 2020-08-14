TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -msse4.1 -msha -maes -nostdlib -std=c11
QMAKE_CXXFLAGS += -std=c++11

DEFINES += CC_TYPE_MISMATCH_ABORT \
           CC_NO_MEM_ABORT \
           CC_BAD_PARAM_ABORT \
           CC_NO_SUCH_METHOD_ABORT \
           CC_INCLUDE_NETWORK \
           PLATFORMS_CONFIG

win32 {
    LIBS += -lWs2_32 -ladvapi32
}

unix: !win32 {
    LIBS += -lz -lssl -lcrypto -lpthread
}

SOURCES += \
    Containers/binarylist.c \
    Containers/binaryset.c \
    Containers/common.c \
    Containers/genericlist.c \
    Containers/genericmap.c \
    Containers/impl/avl.c \
    Containers/stringlist.c \
    Containers/stringmap.c \
    Containers/stringset.c \
    Containers/variant.c \
    IO/base64.c \
    IO/io_core.c \
    container_io.c \
    utility.c \
    IO/crypto_rand.c \
    IO/hex.c \
    IO/md5.c \
    IO/sha1.c \
    IO/aes.c \
    IO/net.c \
    IO/tee.c \
    dir.c \
    IO/padding/bit.c \
    IO/padding/pkcs7.c \
    tinymalloc.c \
    IO/zlib_io.c \
    main.c \
    seaerror.c \
    IO/limiter.c \
    IO/concat.c \
    process.c \
    IO/sha256.c \
    IO/repeat.c \
    IO/buffer.c

HEADERS += \
    Containers/binarylist.h \
    Containers/binaryset.h \
    Containers/common.h \
    Containers/genericlist.h \
    Containers/genericmap.h \
    Containers/impl/avl.h \
    Containers/stringlist.h \
    Containers/stringmap.h \
    Containers/stringset.h \
    Containers/variant.h \
    IO/base64.h \
    IO/io_core.h \
    container_io.h \
    containers.h \
    utility.h \
    platforms.h \
    IO/crypto_rand.h \
    IO/hex.h \
    IO/md5.h \
    IO/sha1.h \
    IO/aes.h \
    IO/net.h \
    IO/tee.h \
    dir.h \
    IO/padding/bit.h \
    IO/padding/pkcs7.h \
    tinymalloc.h \
    platforms_config.h \
    IO/zlib_io.h \
    seaerror.h \
    IO/limiter.h \
    ccio.h \
    IO/concat.h \
    process.h \
    IO/sha256.h \
    IO/repeat.h \
    IO/buffer.h

DISTFILES += \
    README.md \
    Makefile
