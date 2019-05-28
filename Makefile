appname := seatainer
path := $(PATH)

CC = gcc
CFLAGS = -std=c99

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
	$(CC) $(CFLAGS) -I./.. -o $(subst .c,.o,$<) -c $<

clean:
	$(RM) $(OBJS)

distclean:
	$(RM) $(appname)