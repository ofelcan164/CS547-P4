#include <stdio.h>
#include "udp.h"
#include "types.h"

#define BUFFER_SIZE (1000)

int main(int argc, char *argv[]) {
    int sd = UDP_Open(10000);
    assert(sd > -1);

    while (1) {
        struct sockaddr_in addr;
        char message[REQ_SIZE];

        printf("server:: waiting...\n");

        int rc = UDP_Read(sd, &addr, message, REQ_SIZE);

        struct request req;
        req = *((struct request *) message);

        printf("server:: read message [size:%d contents:(%s), type:(%d)]\n", rc, message, req.type);
        
        if (rc > 0) {
            struct response res;
            res.rc = 0;

            if (req.type == SHUTDOWN) {
                printf("Shutdown requested...\n");
            }

            if (req.type == READ) {
                printf("Request type: READ found.\n");
                char text[] = "test";
                strcpy(res.buffer, text);
                printf("conents: %s\n", res.buffer);
            }

            rc = UDP_Write(sd, &addr, (char *) &res, RESP_SIZE);
            printf("server:: reply\n");
        } 
    }
    return 0; 
}