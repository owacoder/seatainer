TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -mssse3 -msha

DEFINES += CC_TYPE_MISMATCH_ABORT \
           CC_NO_MEM_ABORT \
           CC_BAD_PARAM_ABORT \
           CC_NO_SUCH_METHOD_ABORT \

SOURCES += main.c \
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
    IO/sha1.c

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
    IO/sha1.h

DISTFILES += \
    README.md
