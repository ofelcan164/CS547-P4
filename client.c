#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "types.h"

// client code
int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 9000); // TODO

    MFS_Creat(0, 1, "foo");

    // printf("client:: send message [%s]\n", message);
    // rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    // if (rc < 0) {
	// printf("client:: failed to send\n");
	// exit(1);
    // }

    // printf("client:: wait for reply...\n");
    // rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    // printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    return rc;
}

