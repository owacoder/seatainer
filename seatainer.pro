TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -msse4.1 -msha -maes -nostdlib
QMAKE_CXXFLAGS += -std=c++11

DEFINES += CC_TYPE_MISMATCH_ABORT \
           CC_NO_MEM_ABORT \
           CC_BAD_PARAM_ABORT \
           CC_NO_SUCH_METHOD_ABORT \
           CC_INCLUDE_NETWORK \
           PLATFORMS_CONFIG

win32 {
    LIBS += -lWs2_32
}

unix: !win32 {
    LIBS += -lz
}

SOURCES += \
    ccdbllst.c \
    cclnklst.c \
    ccvector.c \
    ccstring.c \
    element.c \
    utility.c \
    cchash.c \
    io.c \
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
    seaerror.c

HEADERS += \
    ccdbllst.h \
    cclnklst.h \
    ccvector.h \
    ccstring.h \
    element.h \
    utility.h \
    cchash.h \
    platforms.h \
    io.h \
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
    seaerror.h

DISTFILES += \
    README.md \
    Makefile
