appname := seatainer
path := $(PATH)

CC = gcc
CFLAGS = -Wall -std=c99 # -maes -msse4.1 -msha
DEFINES = -D_POSIX_C_SOURCE=200809L

SRCFILES = ccdbllst.c \
           ccstring.c \
           cchash.c \
           ccvector.c \
           cclnklst.c \
           element.c \
           IO/aes.c \
           IO/crypto_rand.c \
           IO/hex.c \
           IO/md5.c \
           IO/sha1.c \
           IO/net.c \
           IO/tee.c \
           io.c \
           dir.c \
           main.c \
           utility.c \
           ccpair.c

HEADERFILES = ccpair.h \
              ccdbllst.h \
              element.h \
              ccstring.h \
              cchash.h \
              ccvector.h \
              IO/aes.h \
              IO/crypto_rand.h \
              IO/hex.h \
              IO/md5.h \
              IO/sha1.h \
              IO/net.h \
              IO/tee.h \
              io.h \
              cclnklst.h \
              dir.h \
              platforms.h \
              utility.h

OBJS = $(subst .c,.o,$(SRCFILES))
LDLIBS = -lm

all: $(appname)

$(appname): $(OBJS)
	$(CC) $(LDFLAGS) -o $(appname) $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -o $(subst .c,.o,$<) -c $<

.PHONY: clean
clean:
	$(RM) $(OBJS)

.PHONY: distclean
distclean:
	$(RM) $(appname)
