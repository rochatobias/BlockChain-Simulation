#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "mtwister.h"
#include "miner.h"
#include "structs.h"

#define SHA256_LEN 32

void calcularHash(BlocoNaoMinerado *b, unsigned char hash[SHA256_LEN]){
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    SHA256_Update(&ctx, &b->numero, sizeof(b->numero));
    SHA256_Update(&ctx, &b->nonce, sizeof(b->nonce));
    SHA256_Update(&ctx, &b->data, sizeof(b->data)); // 184 bytes
    SHA256_Update(&ctx, &b->hashAnterior, SHA256_LEN);

    SHA256_Final(hash, &ctx);
}

void minerarBloco(BlocoNaoMinerado *b, unsigned char hash [SHA256_LEN]){
    b->nonce = 0;

    while(1){
        calcularHash(b, hash);
        // Verifica se os dois primeiros nibbles (primeiro byte) são 0
        if (hash[0] == 0){
            break;
        }
        b->nonce++;
    }
}

void atualizarHashAnt(BlocoNaoMinerado *prox, unsigned char hashAnterior[SHA256_LEN]){
    memcpy(prox->hashAnterior, hashAnterior, SHA256_LEN);
}

// FUNÇÃO ATUALIZADA
BlocoMinerado criarBlocoGenesis(unsigned char dados[]){
    BlocoNaoMinerado bg;
    memset(&bg, 0, sizeof(BlocoNaoMinerado)); // zera tudo

    bg.numero = 1; 

    // AQUI: Copia os dados recebidos (mensagem + minerador) para o bloco
    memcpy(bg.data, dados, 184); 

    BlocoMinerado blocoFinal;
    blocoFinal.bloco = bg;

    minerarBloco(&blocoFinal.bloco, blocoFinal.hash);

    return blocoFinal;
}

// FUNÇÃO ATUALIZADA
BlocoMinerado criarProxBloco(BlocoMinerado ant, unsigned int num, unsigned char dados[]){
    BlocoNaoMinerado novo;
    memset(&novo, 0, sizeof(novo));

    novo.numero = num;

    atualizarHashAnt(&novo, ant.hash);

    // AQUI: Copia as transações geradas para dentro do bloco
    memcpy(novo.data, dados, 184);

    BlocoMinerado final;
    final.bloco = novo;

    minerarBloco(&final.bloco, final.hash);

    return final;
}