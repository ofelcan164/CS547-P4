#include <stdio.h>
#include <assert.h>
#include "mfs.h"


void test1() {
    char *hostName = "localhost";
    int port = 20000;
    int result = MFS_Init(hostName, port);
    assert(result == 0);
    port = port + 1;
    result = MFS_Init(hostName, port);
    assert (result != 0);
}

// Tests for lib
int main() {
    test1();
    return 0;
}
