CC     := gcc
CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c \
	mfs-lib-test.c \
	mfs-lib-test-server.c

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all
all: ${PROGS}

${PROGS} : % : %.o Makefile
	${CC} $< -o $@ udp.c libmfs.c

clean:
	rm -f ${PROGS} ${OBJS}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<
