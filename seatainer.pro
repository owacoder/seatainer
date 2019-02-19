TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += CC_TYPE_MISMATCH_ABORT \
           CC_NO_MEM_ABORT \
           CC_BAD_PARAM_ABORT \
           CC_NO_SUCH_METHOD_ABORT \

SOURCES += main.c \
    ccdbllst.c \
    cclnklst.c \
    ccvector.c \
    element.c

HEADERS += \
    ccdbllst.h \
    cclnklst.h \
    ccvector.h \
    element.h
