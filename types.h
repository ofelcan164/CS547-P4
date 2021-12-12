#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)
#define REQ_SIZE (sizeof(struct request))
#define RESP_SIZE (sizeof(struct response))
#define NUM_IMAP_PIECES (256)
#define FILENAME_SIZE (28)
#define NUM_INODES_PER_PIECE (16)
#define NUM_POINTERS_PER_INODE (14)

enum req_type {LOOKUP, STAT, WRITE, READ, CREAT, UNLINK, SHUTDOWN};

struct request {
    enum req_type type;
    int pinum;
    char name[FILENAME_SIZE];
    int inum;
    char buffer[BUFFER_SIZE];
    int block;
    int file_type;
};

struct response {
    int rc;
    MFS_Stat_t m;
    char buffer[BUFFER_SIZE];
};

struct checkpoint_region {
    int log_end_ptr;
    int imap_piece_ptrs[NUM_IMAP_PIECES];
}

struct imap_piece {
    int inode_ptrs[NUM_INODES_PER_PIECE];
}

struct inode {
    int size;
    int type;
    int pointers[NUM_POINTERS_PER_INODE];
}
