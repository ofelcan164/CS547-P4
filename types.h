#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)
#define REQ_SIZE (sizeof(struct request))
#define RESP_SIZE (sizeof(struct response))
#define NUM_IMAP_PIECES (256)
#define FILENAME_SIZE (28)
#define NUM_INODES_PER_PIECE (16)
#define NUM_POINTERS_PER_INODE (14)
#define NUM_DIR_ENTRIES_PER_BLOCK (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t))
#define NUM_INODES (4096)
#define MAX_FILE_SIZE (57344) // 56 KB

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

struct imap_piece {
    int inode_ptrs[NUM_INODES_PER_PIECE];
};

struct checkpoint_region {
    int log_end_ptr;
    int imap_piece_ptrs[NUM_IMAP_PIECES];
};

struct inode {
    int size;
    int type;
    int pointers[NUM_POINTERS_PER_INODE];
};
