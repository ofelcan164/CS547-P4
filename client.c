#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "types.h"

// client code
int main(int argc, char *argv[]) {
    MFS_Init("localhost", 9000); // TODO

    printf("%d\n", MFS_Creat(0, 1, "foo"));

    printf("%d\n", MFS_Write(1, "testasdasdadasdasdasdfweonw ffbibw wibwef wefej webfw efwebf ", 0));
    printf("%d\n", MFS_Lookup(0, "foo"));

    MFS_Shutdown();

    return 0;
}

