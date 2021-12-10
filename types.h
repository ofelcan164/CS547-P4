#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)
#define REQ_SIZE (sizeof(struct request))
#define RESP_SIZE (sizeof(struct response))

enum req_type {INIT, LOOKUP, STAT, WRITE, READ, CREAT, UNLINK, SHUTDOWN};

struct request {
    enum req_type type;
    char hostname[50];
    int port;
    int pinum;
    char name[28];
    int inum;
    char buffer[BUFFER_SIZE];
    int block;
    int file_type;
};

struct response {
    int rc;
    char buffer[BUFFER_SIZE];
    MFS_Stat_t m;
};