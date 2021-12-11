CC     := gcc
CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all
all: ${PROGS}

${PROGS} : % : %.o Makefile
	${CC} $< -o $@ udp.c -L. -lmfs

clean:
	rm -f ${PROGS} ${OBJS} libmfs.o libmfs.so

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<


lib:
	gcc -fPIC -g -c -Wall libmfs.c
	gcc -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o -lc
	export LD_LIBRARY_PATH=.
