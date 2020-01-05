appname := seatainer
path := $(PATH)

CC = gcc
LD = gcc # define to ld if you want freestanding behavior (not yet supported)
CFLAGS = -Wall -std=c99 # -maes -msse4.1 -msha
CXXFLAGS = -Wall -std=c++11
DEFINES = -D_POSIX_C_SOURCE=200809L -DCC_INCLUDE_NETWORK
LDLIBS = -lm -lz

SRCFILES = ccdbllst.c \
           ccstring.c \
           cchash.c \
           ccvector.c \
           cclnklst.c \
           element.c \
           IO/aes.c \
           IO/concat.c \
           IO/crypto_rand.c \
           IO/hex.c \
           IO/limiter.c \
           IO/md5.c \
           IO/sha1.c \
           IO/sha256.c \
           IO/net.c \
           IO/tee.c \
           IO/zlib_io.c \
           IO/padding/bit.c \
           IO/padding/pkcs7.c \
           ccio.c \
           dir.c \
           process.c \
           main.c \
           utility.c \
           seaerror.c \
           tinymalloc.c

HEADERFILES = ccdbllst.h \
              element.h \
              ccstring.h \
              cchash.h \
              ccvector.h \
              IO/aes.h \
              IO/concat.h \
              IO/crypto_rand.h \
              IO/hex.h \
              IO/limiter.h \
              IO/md5.h \
              IO/sha1.h \
              IO/sha256.h \
              IO/net.h \
              IO/tee.h \
              IO/zlib_io.h \
              IO/padding/bit.h \
              IO/padding/pkcs7.h \
              ccio.h \
              cclnklst.h \
              dir.h \
              process.h \
              platforms.h \
              utility.h \
              seaerror.h \
              tinymalloc.h

OBJS = $(subst .c,.o,$(SRCFILES))

all: $(appname)

$(appname): $(OBJS)
	$(LD) $(LDFLAGS) -o $(appname) $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -o $(subst .c,.o,$<) -c $<
       
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) -o $(subst .cpp,.o,$<) -c $<

.PHONY: clean
clean:
	$(RM) $(OBJS)

.PHONY: distclean
distclean:
	$(RM) $(appname)
