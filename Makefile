#
#
CC	= cc
CFLAGS	= -g -W -Wall -Werror -Wstrict-prototypes -Wpointer-arith \
	-Wmissing-prototypes -Wsign-compare -std=c99 -pedantic -pipe \
	-DNEED_STAT64
LDFLAGS	=
#
RM	= /bin/rm
#

all: fist

fist:
	$(CC) fist.c $(LDFLAGS) -o $@

clean:
	@$(RM) -f *.o fist

