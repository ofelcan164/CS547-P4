#include "mfs.h"
#include "udp.h"
#include "types.h"

int sd;
struct sockaddr_in addrSnd, addrRcv;

/**
 * Takes a host name and port number and uses those to find the server exporting the file system.
 */
int MFS_Init(char *hostname, int port) {
    return 0;
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
    struct resp msg;
    msg.type = LOOKUP;
    msg.pinum = pinum;
    strcpy(msg.name, name);

    // Copy to buffer
    char msg_content[MSG_SIZE];
    memcpy((struct resp*)msg_content, &msg, REQ_SIZE);

    // Send request
    int rc = UDP_Write(sd, &addrSnd, msg_content, REQ_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Lookup -> UDP_Write()\n");
        return rc;
    }

    // Receive inode number (or -1 if failed)
    struct response resp = {};
    rc = UDP_Read(sd, &addrRcv, msg_content, RESP_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Lookup -> UDP_Read()\n");
        return rc;
    }

    // Cast response to message
    memcpy(&msg, (struct response*)msg_content, RESP_SIZE));

    return resp.rc;
}

/**
 * Returns some information about the file specified by inum. 
 * Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist.
 */
int MFS_Stat(int inum, MFS_Stat_t *m) {
    // Fill struct to send
    struct resp msg;
    msg.type = STAT;
    msg.inum = inum;
    msg.m = *m;

    // Copy to buffer
    char msg_content[MSG_SIZE];
    memcpy((struct resp*)msg_content, &msg, REQ_SIZE));

    // Send request
    int rc = UDP_Write(sd, &addrSnd, msg_content, REQ_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Stat -> UDP_Write()\n");
        return rc;
    }

    // Receive the stats of the inode
    struct response resp = {};
    rc = UDP_Read(sd, &addrRcv, msg_content, RESP_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Stat -> UDP_Read()\n");
        return rc;
    }

    // Cast response
    memcpy(&msg, (struct response*)msg_content, RESP_SIZE));

    // Return success or failure based on returned stats
    if (resp.rc < 0) 
        return -1;

    m->size = resp.m.size;
    m->type = resp.m.type;
    return 0;
}

/**
 * Writes a block of size 4096 bytes at the block offset specified by block. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: invalid inum, invalid block, not a regular file (because you can't write to directories).
 */
int MFS_Write(int inum, char *buffer, int block) {
    return 0;
}

/**
 * Reads a block specified by block into the buffer from file specified by inum. 
 * The routine should work for either a file or directory; directories should return data in the format specified by MFS_DirEnt_t. 
 * Success: 0, failure: -1. 
 * Failure modes: invalid inum, invalid block.
 */
int MFS_Read(int inum, char *buffer, int block) {
    return 0;
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
    struct resp msg;
    msg.type = CREAT;
    msg.pinum = pinum;
    msg.file_type = type;
    strcpy(msg.name, name);

    // Copy to buffer
    char msg_content[REQ_SIZE];
    memcpy((struct resp*)msg_content, &msg, REQ_SIZE));

    // Send request
    int rc = UDP_Write(sd, &addrSnd, msg_content, REQ_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Creat -> UDP_Write()\n");
        return rc;
    }

    // Receive success or failure
    struct response resp = {};
    rc = UDP_Read(sd, &addrRcv, msg_content, RESP_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Creat -> UDP_Read()\n");
        return rc;
    }

    // Cast response
    memcpy(&msg, (struct response*)msg_content, RESP_SIZE));

    // Return based on what server returned
    if (resp.rc < 0)
        return -1;
    
    return 0;
}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name) {
    // Fill struct to send
    struct resp msg;
    msg.type = UNLINK;
    msg.pinum = pinum;
    strcpy(msg.name, name);

    // Copy to buffer
    char msg_content[MSG_SIZE];
    memcpy((struct resp*)msg_content, &msg, REQ_SIZE));

    // Send request
    int rc = UDP_Write(sd, &addrSnd, msg_content, REQ_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Unlink -> UDP_Write()\n");
        return rc;
    }

    // Receive success or failure
    struct response resp = {};
    rc = UDP_Read(sd, &addrRcv, msg_content, RESP_SIZE);
    if (rc < 0) {
        perror("Error occured in MFS_Unlink -> UDP_Read()\n");
        return rc;
    }

    // Cast response to message
    memcpy(&msg, (struct response*)msg_content, RESP_SIZE));

    // Return based on what server returned
    return resp.rc;
}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). This interface will mostly be used for testing purposes.
 */
int MFS_Shutdown() {
    return 0;
}