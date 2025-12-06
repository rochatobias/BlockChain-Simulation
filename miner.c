#include <stdio.h>
#include <stdlib.h>
#include "mtwister.h"
#include "miner.h"
#include "structs.h"
#include <string.h>
#include <openssl/sha.h>

#define SHA256_LEN 32

void calcularHash(BlocoNaoMinerado *b, unsigned char hash[SHA256_LEN]){
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    SHA256_Update(&ctx, &b->numero, sizeof(b->numero));
    SHA256_Update(&ctx, &b->nonce, sizeof(b->nonce));
    SHA256_Update(&ctx, &b->data, sizeof(b->data));
    SHA256_Update(&ctx, &b->hashAnterior, SHA256_LEN);

    SHA256_Final(hash, &ctx);
}

void minerarBloco(BlocoNaoMinerado *b, unsigned char hash [SHA256_LEN]){
    b->nonce = 0;

    while(1){
        calcularHash(b, hash);

        if (hash[0] == 0){
            break;
        }

        b->nonce++;
    }
}

void atualizarHashAnt(BlocoNaoMinerado *prox, unsigned char hashAnterior[SHA256_LEN]){
    memcpy(prox->hashAnterior, hashAnterior, SHA256_LEN);
}

BlocoMinerado criarBlocoGenesis(){
    BlocoNaoMinerado bg;
    memset(&bg, 0, sizeof(BlocoNaoMinerado)); //setando os 0s

    bg.numero = 1; // numerando o  bloco

    BlocoMinerado blocoFinal;
    blocoFinal.bloco = bg;

    minerarBloco(&blocoFinal.bloco, blocoFinal.hash);

    return blocoFinal;
}

BlocoMinerado criarProxBloco(BlocoMinerado ant, unsigned int num){
    BlocoNaoMinerado novo;
    memset(&novo, 0, sizeof(novo));

    novo.numero = num;

    atualizarHashAnt(&novo, ant.hash);

    const char *msg = "Transacoes do bloco";\
    memcpy(novo.data, msg, strlen(msg));

    BlocoMinerado final;
    final.bloco = novo;

    minerarBloco(&final.bloco, final.hash);

    return final;
}
