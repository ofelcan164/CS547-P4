#include <stdio.h>
#include "udp.h"

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
    // MFS_Stat_t TODO
};

// server code
int main(int argc, char *argv[]) {
    int sd = UDP_Open(10000);
    assert(sd > -1);
    while (1) {
	struct sockaddr_in addr;
	char message[BUFFER_SIZE];
	printf("server:: waiting...\n");
	int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
	printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
	if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
	    printf("server:: reply\n");
	} 
    }
    return 0; 
}
    


 
