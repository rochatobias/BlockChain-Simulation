#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <openssl/sha.h>

#define DATA_SIZE 184
#define SHA256_DIGEST_LENGTH 32

typedef struct {
    unsigned int numero;                      
    unsigned int nonce;                       
    unsigned char data[DATA_SIZE];           
    unsigned char hashAnterior[SHA256_DIGEST_LENGTH];
} BlocoNaoMinerado;

typedef struct {
    BlocoNaoMinerado bloco;
    unsigned char hash[SHA256_DIGEST_LENGTH];
} BlocoMinerado;

// Hash de cada nonce testado
typedef struct NoHash {
    unsigned int nonce;
    unsigned int idBloco;       // (qtd blocos: 1 - 30000)
    struct NoHash *prox;
} NoHash;

// Quais blocos cada minerador minerou
typedef struct NoMinerador {
    unsigned int idBloco;
    struct NoMinerador *prox;
} NoMinerador;

// Quantidade de transações por bloco minerado
typedef struct NoBucket {
    BlocoMinerado *idBloco;       
    struct NoBucket *proximo;
} NoBucket;

#endif 