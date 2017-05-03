#include <stdio.h>
#include <stdlib.h>
#include "ftree.h"

int main(int argc, char **argv) {
    if (argc != 4) {  
        printf("Usage:\n\trcopy_client SRC DEST host\n");
        return(1);
    }
    //int port = strtol(PORT, NULL, 10);

    int ret = fcopy_client(argv[1], argv[2], argv[3], PORT);
    if (ret == 0) {
    	printf("Copy completed successfully\n");
    	return(0);
    } else if (ret == 1) {
    	printf("Errors encountered during copy\n");
    	return(1);
    }
}
