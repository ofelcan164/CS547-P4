#include "mfs.h"
#include "udp.h"

#define BUFFER_SIZE (1000)
#define HOST_NAME "localhost"
#define CLIENT_PORT (20000)
#define SERVER_PORT (10000)

int sd;
struct sockaddr_in addrSnd, addrRcv;

/**
 * Takes a host name and port number and uses those to find the server exporting the file system.
 * Returns 0 on success and -1 on failure.
 */
int MFS_Init(char *hostname, int port) {
    if (sd != NULL) {
        perror("Error: Connection to server already initialized and is still open.");
        return -1;
    }

    sd = UDP_Open(CLIENT_PORT);
    int rc = UDP_FillSockAddr(&addrSnd, HOST_NAME, SERVER_PORT);

    if (rc == -1) {
        perror("Error: Could not establish server connection.");
        return -1;
    }

    return 0;
}

/**
 * Takes the parent inode number (which should be the inode number of a directory) 
 * and looks up the entry name in it. 
 * The inode number of name is returned. Success: return inode number of name; failure: return -1. 
 * Failure modes: invalid pinum, name does not exist in pinum.
 */
int MFS_Lookup(int pinum, char *name) {

}

/**
 * returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m) {

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

}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name) {

}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). This interface will mostly be used for testing purposes.
 */
int MFS_Shutdown() {

}