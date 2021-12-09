#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

enum msg_type {INIT, LOOKUP, STAT, WRITE, READ, CREAT, UNLINK, SHUTDOWN};

struct message {
    enum msg_type type;
    char hostname[50];
    int port;
    int pinum;
    char name[28];
    int inum;
    char buffer[BUFFER_SIZE];
    int block;
    int file_type;
    MFS_Stat_t m;
};