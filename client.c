#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "types.h"

// client code
int main(int argc, char *argv[]) {
    printf("ll;ajsdlfjl;kasdjf\n");
    int rc = MFS_Init("localhost", 9000); // TODO

    MFS_Creat(0, 1, "foo");

    MFS_Write(1, "test ", 14);

    MFS_Shutdown();

    return rc;
}

