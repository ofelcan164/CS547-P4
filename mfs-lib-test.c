#include <stdio.h>
#include <assert.h>
#include "mfs.h"
#include "types.h"

// Use asert for all tests so that tests passed message isn't
// seen if test fails.

void expectError() {
    printf("Expected error: ");
}


void test1() {
    char *hostName = "localhost";
    int port = 20000;
    int result = MFS_Init(hostName, port);
    
    assert(result == 0);
    
    port = port + 1;
    expectError();
    result = MFS_Init(hostName, port);
    assert (result != 0);
}

void test2() {
    expectError();
    int result = MFS_Shutdown();
    assert(result != 0);

    char *hostName = "localhost";
    int port = 20000;
    result = MFS_Init(hostName, port);

    assert(result == 0);

    result = MFS_Shutdown();

    assert(result == 0);
}

int main() {
    printf("\n\n***TESTING***\n\n");

    test2();
    test1();

    printf("\n\n***ALL TESTS PASSED***\n\n");
    return 0;
}
