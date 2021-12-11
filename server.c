#include <stdio.h>
#include "udp.h"
#include "types.h"

int lookup(int pinum, char* name) {

}

int stat(int inum) {

}

int write(int inum, char* buffer, int block) {

}

int read(int inum, int block) {

}

int create(int pinum, int type, char* name) {

}

int unlink(int pinum, char* name) {

}

int shutdown() {

}

// server code
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: prompt> server [portnum] [file-system-image]\n");
    }

    // TODO: Initialize / Setup FS

    int sd = UDP_Open(atoi(argv[1]));
    assert (sd > -1);

    while (1) {
        struct sockaddr_in addr;
        struct request req;
        int rc = UDP_Read(sd, &addr, (char *) &req, REQ_SIZE);

        if (rc > 0) {
            switch (req.type) {
            case LOOKUP:
                break;
            case STAT:
                break;
            case WRITE:
                break;
            case READ:
                break;
            case CREAT:
                break;
            case UNLINK:
                break;
            case SHUTDOWN:
                break;
            default:
                break;
            }
        }
    }

    return 0;
}