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
};

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
    memcpy((struct message*)buf, &msg, sizeof(msg));

    // Send message
    UDP_Write(sd, &addrSnd, buf, BUFFER_SIZE);

    // Receive inode number (or -1 if failed)
    struct message ret_msg;
    UDP_Read(sd, &addrRcv, &buf, sizeof(ret_msg)); // TODO receive message in this way?

    return ret_msg.inum; // TODO
}

/**
 * Returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m) {
    // Fill struct to send
    struct message msg;
    msg.type = STAT;
    msg.inum = inum;
    msg.m = m

    // Copy to buffer
    char *buf[BUFFER_SIZE];
    memcpy((struct message*)buf, &msg, sizeof(msg));

    // Send message
    UDP_Write(sd, &addrSnd, buf, BUFFER_SIZE);

    // Receive the stats of the inode
    struct message ret_msg;
    UDP_Read(sd, &addrRcv, &buf, sizeof(ret_msg)); // TODO receive message in this way?

    // Return success or failure based on returned stats
    if (ret_msg.m.size == -1) 
        return -1;

    m->size = ret_msg.m->size;
    m->type = ret_msg.m->type;
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
    // Check name
    if (strlen(name) > 28)
        return -1;

    // Fill struct to send
    struct message msg;
    msg.type = CREAT;
    msg.pinum = pinum;
    msg.file_type = type;
    msg.name = name;

    // Copy to buffer
    char *buf[BUFFER_SIZE];
    memcpy((struct message*)buf, &msg, sizeof(msg));

    // Send message
    UDP_Write(sd, &addrSnd, buf, BUFFER_SIZE);

    // Receive success or failure
    struct message ret_msg;
    UDP_Read(sd, &addrRcv, &buf, sizeof(ret_msg)); // TODO receive message in this way?

    // Return based on what server returned
    return ret_msg.inum; // TODO
}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name) {
    // Fill struct to send
    msg.type = UNLINK;
    struct message msg;
    msg.pinum = pinum;
    msg.name = name;

    // Copy to buffer
    char *buf[BUFFER_SIZE];
    memcpy((struct message*)buf, &msg, sizeof(msg));

    // Send message
    UDP_Write(sd, &addrSnd, buf, BUFFER_SIZE);


    // Receive success or failure
    struct message ret_msg;
    UDP_Read(sd, &addrRcv, &buf, sizeof(ret_msg)); // TODO receive message in this way?

    // Return based on what server returned
    return ret_msg.inum; // TODO
}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). This interface will mostly be used for testing purposes.
 */
int MFS_Shutdown() {

}