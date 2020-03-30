TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -msse4.1 -msha -maes -nostdlib -std=gnu99
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
    ccdbllst.c \
    cclnklst.c \
    ccvector.c \
    ccstring.c \
    element.c \
    utility.c \
    cchash.c \
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
    ccio.c \
    IO/concat.c \
    process.c \
    IO/sha256.c \
    IO/repeat.c \
    IO/buffer.c

HEADERS += \
    ccdbllst.h \
    cclnklst.h \
    ccvector.h \
    ccstring.h \
    element.h \
    utility.h \
    cchash.h \
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
