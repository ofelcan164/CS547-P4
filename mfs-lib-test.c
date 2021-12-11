#include <stdio.h>
#include <assert.h>
#include "mfs.h"
#include "types.h"

// Use asert for all tests so that tests passed message isn't
// seen if test fails.

void expectError() {
    printf("Expected error: ");
}

void test_MFS_init() {
    char *hostName = "localhost";
    int port = 20000;
    int result = MFS_Init(hostName, port);
    
    assert(result == 0);
    
    port = port + 1;
    expectError();
    result = MFS_Init(hostName, port);
    assert (result != 0);
}

void test_MFS_shutdown() {
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

void test_MFS_Write() {
    int inum = 10;
    int block = 10;
    char *buffer = malloc(1000);

    assert (buffer != NULL);
    
    int result = MFS_Write(inum, buffer, block);

    assert(result == 0);
}

void test_MFS_Read() {
    int inum = 10;
    int block = 10;
    char *buffer = malloc(1000);
    
    assert (buffer != NULL);

    int result = MFS_Read(inum, buffer, block);

    assert (result == 0);

    char text[] = "test";
    assert (strcmp(buffer, text));
}

int main() {
    printf("\n\n***TESTING***\n\n");
    
    // test shutdown first so that we can test for shutting down an uninitialized connection.
    test_MFS_shutdown(); 

    test_MFS_init(); // <- leaves connection open.
    
    test_MFS_Write();

    test_MFS_Read();

    printf("\n\n***ALL TESTS PASSED***\n\n");
    return 0;
}
