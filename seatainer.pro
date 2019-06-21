TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -msse4.1 -msha -maes
QMAKE_CXXFLAGS += -std=c++11

DEFINES += CC_TYPE_MISMATCH_ABORT \
           CC_NO_MEM_ABORT \
           CC_BAD_PARAM_ABORT \
           CC_NO_SUCH_METHOD_ABORT \
           CC_IO_STATIC_INSTANCES=20 \
           CC_INCLUDE_NETWORK

win32 {
    LIBS += -lWs2_32
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
    main.cpp \
    IO/padding/bit.c \
    IO/padding/pkcs7.c

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
    IO/padding/pkcs7.h

DISTFILES += \
    README.md
