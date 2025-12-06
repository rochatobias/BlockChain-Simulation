#ifndef STORAGE_H
#define STORAGE_H

// Bibliotecas 
#include <stdio.h>
#include "structs.h"

void inicializarStorage(const char *nomeArquivo); // Inicializa o sistema de armazenamento da blockchain
void finalizarStorage(); // Finaliza o sistema de armazenamento
void adicionarBloco(BlocoMinerado *bloco); // Adiciona um novo bloco minerado ao sistema
int buscarNonce(unsigned int nonce, BlocoMinerado *saida); // Busca um bloco pelo seu nonce usando hash table
void listarBlocosMinerador(unsigned char endereco, int n); // Lista os primeiros N blocos minerados por um endereço específico
void relatorioTransacoes(unsigned int n); // Gera relatório de blocos ordenados por número de transações
void exibirEstatisticas(); // Exibe estatísticas gerais do sistema

/**
 * [FUNÇÃO DE DEBUG] Analisa distribuição da hash table
 * - Mostra colisões, fator de carga, pior caso
 * - Útil para ajustar TAM_HASH e validar hash function
 */
void relatorioColisoes();

#endif 