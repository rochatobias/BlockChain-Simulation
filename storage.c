#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

// Ponteiros para os inícios das listas de blocos minerados por cada minerador
NoMinerador *indiceMinerador[256]; 
// Ponteiro para o início da lista da quantidade de transações por bloco minerado
NoBucket *bucketTransacoes[62]; 

int main() {
    return 0;
}