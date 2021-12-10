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
        memcpy(&req, (struct request*) message, REQ_SIZE);

        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        
        if (rc > 0) {
            char reply[RESP_SIZE];
            struct response res;
            res.rc = 0;

            if (req.type == READ) {
                char text[] = "test";
                memcpy(res.buffer, &text, 5);
            }

            memcpy(reply, &res, RESP_SIZE);
            rc = UDP_Write(sd, &addr, reply, RESP_SIZE);
            printf("server:: reply\n");
        } 
    }
    return 0; 
}