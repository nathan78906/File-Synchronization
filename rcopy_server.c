#include <stdio.h>
#include "ftree.h"

int main(int argc, char **argv) {
    if (argc != 1) {  
        printf("Usage:\n\trcopy_server \n");
        return 1;
    }
    //int port = strtol(PORT, NULL, 10);
    fcopy_server(PORT);
    return 0; // will never get here since we should keep server open for other clients
}
