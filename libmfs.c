#include "mfs.h"
#include "udp.h"

#define BUFFER_SIZE (1000)
int sd;
struct sockaddr_in addrSnd, addrRcv;

enum msg_type {INIT, LOOKUP, STAT, WRITE, READ, CREAT, UNLINK, SHUTDOWN};

struct message {
    enum msg_type type;
    char *hostname[50];
    int port;
    int pinum;
    char *name[28];
    int inum;
    char *buffer[BUFFER_SIZE];
    int block;
    int file_type;
    MFS_Stat_t *m
}

/**
 * Takes a host name and port number and uses those to find the server exporting the file system.
 */
int MFS_Init(char *hostname, int port) {

}

/**
 * Takes the parent inode number (which should be the inode number of a directory) 
 * and looks up the entry name in it. 
 * The inode number of name is returned. 
 * Success: return inode number of name; failure: return -1. 
 * Failure modes: invalid pinum, name does not exist in pinum.
 */
int MFS_Lookup(int pinum, char *name) {
    // Check name
    if (strlen(name) > 28)
        return -1;

    // Fill struct to send
    struct message msg;
    msg.type = LOOKUP;
    msg.pinum = pinum;
    msg.name = name;

    // Copy to buffer
    char *buf[BUFFER_SIZE];
    memcpy((lookup_msg*)buf, &msg, sizeof(msg));

    // Send message
    UDP_Write(sd, &addrSnd, msg, sizeof(msg));

    // Receive inode number (or -1 if failed)
    int inum;
    UDP_Read(sd, &addrRcv, &inum, sizeof(inum));

    return inum;
}

/**
 * Returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m) {
    // Probably have some sort of struct containing args
    // OR USE GENERERIC which has a msg type field
    struct stat_msg {
        int inum;
    };

    // Fill struct to send
    struct stat_msg msg;
    msg.inum = inum;

    // Send message
    UDP_Write(sd, &addrSnd, msg, sizeof(msg));

    // Receive the stats of the inode
    UDP_Read(sd, &addrRcv, &m, sizeof(&m));

    // Return success or failure based on returned stats
    if (m->size == -1) 
        return -1;

    return 0;
}

/**
 * Writes a block of size 4096 bytes at the block offset specified by block. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: invalid inum, invalid block, not a regular file (because you can't write to directories).
 */
int MFS_Write(int inum, char *buffer, int block) {

}

/**
 * Reads a block specified by block into the buffer from file specified by inum. 
 * The routine should work for either a file or directory; directories should return data in the format specified by MFS_DirEnt_t. 
 * Success: 0, failure: -1. 
 * Failure modes: invalid inum, invalid block.
 */
int MFS_Read(int inum, char *buffer, int block) {

}

/**
 * Makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY) in the parent directory specified by pinum of name name. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success (think about why).
 */
int MFS_Creat(int pinum, int type, char *name) {
    // Probably have some sort of struct containing args
    // OR USE GENERERIC which has a msg type field
    struct creat_msg {
        int pinum;
        int type;
        char *name;
    };

    // Check name
    if (strlen(name) > 28)
        return -1;

    // Fill struct to send
    struct creat_msg msg;
    msg.pinum = pinum;
    msg.type = type;
    msg.name = name;

    // Send message
    UDP_Write(sd, &addrSnd, msg, sizeof(msg));

    // Receive success or failure
    int rc;
    UDP_Read(sd, &addrRcv, &rc, sizeof(rc));

    // Return based on what server returned
    return rc;

}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name) {
    // Probably have some sort of struct containing args
    // OR USE GENERERIC which has a msg type field
    struct unlink_msg {
        int pinum;
        char *name;
    };

    // Fill struct to send
    struct unlink_msg msg;
    msg.pinum = pinum;
    msg.name = name;

    // Send message
    UDP_Write(sd, &addrSnd, msg, sizeof(msg));

    // Receive success or failure
    int rc;
    UDP_Read(sd, &addrRcv, &rc, sizeof(rc));

    // Return based on what server returned
    return rc;
}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). This interface will mostly be used for testing purposes.
 */
int MFS_Shutdown() {

}