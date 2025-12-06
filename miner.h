#ifndef MINER_H
#define MINER_H

// Bibliotecas 
#include <stdio.h>
#include "structs.h"

// Protótipos das funções
void calcularHash(BlocoNaoMinerado *b, unsigned char hash[SHA256_LEN]);
void minerarBloco(BlocoNaoMinerado *b, unsigned char hash [SHA256_LEN]);
void atualizarHashAnt(BlocoNaoMinerado *prox, unsigned char hashAnterior[SHA256_LEN]);
BlocoMinerado criarBlocoGenesis();
BlocoMinerado criarProxBloco(BlocoMinerado ant, unsigned int num);

#endif 
