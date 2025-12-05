#ifndef STORAGE_H
#define STORAGE_H

// Bibliotecas 
#include <stdio.h>
#include "structs.h"

// Protótipos das funções
void inicializarStorage(const char *nomeArquivo); // Prepara o arquivo, cria índices e carrega dados existentes
void finalizarStorage(); // Fecha arquivo, salva buffer pendente e limpaa memória RAM
void adicionarBloco(BlocoMinerado *bloco); // Atualiza Buffer, Disco e Índices
int buscarNonce(unsigned int nonce, BlocoMinerado *saida); // Busca por Nonce. Retorna 1 se achou, 0 se não
void listarBlocosMinerador(unsigned char endereco); // Lista blocos de um minerador
void relatorioTransacoes(unsigned int n); // Relatório ordenado por transações
void exibirEstatisticas(); // Exibe estatísticas

// Operações de exibição
void imprimirBloco(BlocoMinerado *bloco);
void imprimirHash(unsigned char *hash, int tamanho);

// apagar depois de testes
void relatorioColisoes();

#endif 