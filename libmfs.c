#include "mfs.h"
#include "udp.h"
#include "types.h"

#define CLIENT_PORT (20000)

int sd = -2; // should not be set to this in UDP_Open().
struct sockaddr_in addrSnd, addrRcv;

/**
 * Takes a host name and port number and uses those to find the server exporting the file system.
 * Returns 0 on success and -1 on failure.
 */
int MFS_Init(char *hostname, int port) {
    if (sd != -2) {
        printf("Error: Connection to server already initialized and is still open.\n");
        return -1;
    }

    sd = UDP_Open(CLIENT_PORT);

    if (sd == -1) {
        sd = -2; // reset to unitialized status
        printf("Could not open connection on port: %d\n", CLIENT_PORT);
        return -1;
    }

    int rc = UDP_FillSockAddr(&addrSnd, hostname, port);

    if (rc == -1) {
        printf("Error: Could not establish server connection.\n");
        return -1;
    }

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
    struct request req;
    req.type = LOOKUP;
    req.pinum = pinum;
    strcpy(req.name, name);

    // Send request
    int rc = UDP_Write(sd, &addrSnd, (char *)&req, REQ_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Lookup -> UDP_Write()\n");
        return rc;
    }

    // Receive inode number (or -1 if failed)
    char buffer[RESP_SIZE];
    rc = UDP_Read(sd, &addrRcv, buffer, RESP_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Lookup -> UDP_Read()\n");
        return rc;
    }

    // Cast response
    struct response resp;
    resp = *((struct response *)buffer);

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
    struct request req;
    req.type = STAT;
    req.inum = inum;

    // Send request
    int rc = UDP_Write(sd, &addrSnd, (char *)&req, REQ_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Stat -> UDP_Write()\n");
        return rc;
    }

    // Receive the stats of the inode
    char buffer[RESP_SIZE];
    rc = UDP_Read(sd, &addrRcv, buffer, RESP_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Stat -> UDP_Read()\n");
        return rc;
    }
    
    // Cast response
    struct response resp;
    resp = *((struct response *)buffer);

    // Return success or failure based on returned stats
    m->size = resp.m.size;
    m->type = resp.m.type;
    return resp.rc;
}

/**
 * Writes a block of size 4096 bytes at the block offset specified by block. 
 * Returns 0 on success, -1 on failure. 
 * Failure modes: invalid inum, invalid block, not a regular file (because you can't write to directories).
 */
int MFS_Write(int inum, char *buffer, int block) {
    struct request req;
    req.type = WRITE;
    req.inum = inum;
    req.block = block;
    strcpy(req.buffer, buffer);

    int writeResult = UDP_Write(sd, &addrSnd, (char *) &req, REQ_SIZE);

    if (writeResult == -1) {
        printf("Error occured while writing. MFS_Write() -> UDP_Write()\n");
        return -1;
    }

    char response_message[RESP_SIZE];

    int readResult = UDP_Read(sd, &addrRcv, response_message, RESP_SIZE);

    if (readResult == -1) {
        printf("Error occured while reading. MFS_Write() -> UDP_Read()\n");
        return -1;
    }

    struct response res;
    res = *((struct response *) response_message);

    if (res.rc != 0) {
        printf("Server error occured while writing. Reponse code: %d\n", res.rc);
        return -1;
    }

    return res.rc;
}

/**
 * Reads a block specified by block into the buffer from file specified by inum. 
 * The routine should work for either a file or directory; directories should return data in the format specified by MFS_DirEnt_t. 
 * Success: 0, failure: -1. 
 * Failure modes: invalid inum, invalid block.
 */
int MFS_Read(int inum, char *buffer, int block) {
    struct request req;
    req.type = READ;
    req.inum = inum;
    req.block = block;

    int writeResult = UDP_Write(sd, &addrSnd, (char *) &req, REQ_SIZE);
    
    if (writeResult == -1) {
        printf("Error occured while writing. MFS_Read() -> UDP_Write()\n");
        return -1;
    }

    char res_message[RESP_SIZE];

    int readResult = UDP_Read(sd, &addrRcv, res_message, RESP_SIZE);

    if (readResult == -1) {
        printf("Error occured while reading. MFS_Read() -> UDP_Read()\n");
        return -1;
    }

    struct response res;
    res = *((struct response *) res_message);

    if (res.rc != 0) {
        printf("Server error occured while reading. Reponse code: %d\n", res.rc);
        return -1;
    }

    strcpy(buffer, res.buffer); //TODO: want to make sure room to copy and no overflow happening here.

    return res.rc;
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
    struct request req;
    req.type = CREAT;
    req.pinum = pinum;
    req.file_type = type;
    strcpy(req.name, name);

    // Send request
    int rc = UDP_Write(sd, &addrSnd, (char *)&req, REQ_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Creat -> UDP_Write()\n");
        return rc;
    }

    // Receive success or failure
    char buffer[RESP_SIZE];
    rc = UDP_Read(sd, &addrRcv, buffer, RESP_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Creat -> UDP_Read()\n");
        return rc;
    }
    
    // Cast response
    struct response resp;
    resp = *((struct response *)buffer);

    // Return based on what server returned   
    return resp.rc;
}

/**
 * Removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
 */
int MFS_Unlink(int pinum, char *name) {
    // Fill struct to send
    struct request req;
    req.type = UNLINK;
    req.pinum = pinum;
    strcpy(req.name, name);

    // Send request
    int rc = UDP_Write(sd, &addrSnd, (char *)&req, REQ_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Unlink -> UDP_Write()\n");
        return rc;
    }

    // Receive success or failure
    char buffer[RESP_SIZE];
    rc = UDP_Read(sd, &addrRcv, buffer, RESP_SIZE);
    if (rc < 0) {
        printf("Error occured in MFS_Unlink -> UDP_Read()\n");
        return rc;
    }
    
    // Cast response
    struct response resp;
    resp = *((struct response *)buffer);

    // Return based on what server returned
    return resp.rc;
}

/**
 * Just tells the server to force all of its data structures to disk and shutdown by calling exit(0). This interface will mostly be used for testing purposes.
 * Returns 0 on success and -1 on failure.
 */
int MFS_Shutdown() {
    if (sd == -2) {
        printf("Error: Can't shutdown. Connection was never initialized.\n");
        return -1;
    }

    struct request req;
    req.type = SHUTDOWN;

    int writeResult = UDP_Write(sd, &addrSnd, (char *) &req, REQ_SIZE);

    if (writeResult == -1) {
        printf("Error occured in Shutdown() -> UDP_Write().\n");
        return -1;
    }

    int rc = UDP_Close(sd);
    
    if (rc != 0) {
        printf("Error: unable to close connection to server.\n");
        return -1;
    }

    sd = -2;
    memset(&addrRcv, 0, sizeof(addrRcv));
    memset(&addrSnd, 0, sizeof(addrSnd));

    return 0;
}
