#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include "mtwister.h" 

// Protótipos das suas funções

// Gera as transações aleatórias e preenche o vetor data
int gerarDadosDoBloco(unsigned int numeroDoBloco, unsigned char dataBlock[], unsigned int carteiraOficial[], MTRand *r);

// Atualiza o saldo da carteira (paga minerador e transfere valores)
void atualizarCarteira(unsigned int numeroDoBloco, unsigned char dataBlock[], unsigned int carteiraOficial[], int qtdTransacoes);

// Conta quantas transações existem num bloco (útil para leitura de disco do Tobias)
int contarTransacoesNoBloco(unsigned char dataBlock[]);

// Função auxiliar para imprimir o que tem no bloco (útil para menus)
void imprimirBlocoDebug(unsigned char dataBlock[]);

#endif