#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"


char *hash(FILE *f) {
    char *hash_val = malloc(HASH_SIZE);
    char ch;
    int hash_index = 0;

    for (int index = 0; index < HASH_SIZE; index++) {
        hash_val[index] = '\0';
    }

    while(fread(&ch, 1, 1, f) != 0) {
        hash_val[hash_index] ^= ch;
        hash_index = (hash_index + 1) % HASH_SIZE;
    }

    return hash_val;
}
