#ifndef STORAGE_H
#define STORAGE_H

#include "structs.h"

unsigned int obterTotalBlocos();
unsigned int getSaldo(unsigned char endereco);
int listarBlocosPorNonce(unsigned int nonce);
int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida);
void inicializarStorage(const char *nomeArquivo);
void finalizarStorage();
void getUltimoHash(unsigned char *bufferHash);
void adicionarBloco(BlocoMinerado *bloco);
void relatorioMaisRico();
void relatorioMaiorMinerador();
void relatorioMaxTransacoes();
void relatorioMinTransacoes();
void calcularMediaBitcoinsPorBloco();
void imprimirBlocoPorNumero(unsigned int numero);
void listarBlocosMinerador(unsigned char endereco, int n);
void relatorioTransacoes(unsigned int n);
void *verifica_malloc(size_t tamanho, const char *contexto);
void exibirHistogramaHash();

#endif