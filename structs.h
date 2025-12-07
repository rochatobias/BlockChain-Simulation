#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <openssl/sha.h>

#define DATA_SIZE 184
#define SHA256_LEN 32

/**
 * Estrutura de um bloco ANTES de ser minerado
 * Representa os dados que serão hasheados durante a mineração
 */
typedef struct {
    unsigned int numero;                           // Número sequencial do bloco (1, 2, 3...)
    unsigned int nonce;                            // Número usado para variar o hash na mineração
    unsigned char data[DATA_SIZE];                 // 184 bytes: 183 bytes de transações + 1 byte (minerador)
    unsigned char hashAnterior[SHA256_LEN]; // Hash do bloco anterior (encadeamento)
} BlocoNaoMinerado;

/**
 * Estrutura de um bloco APÓS mineração bem-sucedida
 * Inclui o hash calculado que satisfez a dificuldade
 */
typedef struct {
    unsigned char hash[SHA256_LEN];      // Hash SHA-256 resultante da mineração
    BlocoNaoMinerado bloco;                        // Dados do bloco que foram hasheados
} BlocoMinerado;

/**
 * Nó da Hash Table para índice de Nonces
 * 
 * Estrutura de dados: Hash Table com encadeamento (chaining)
 * - Cada slot da tabela aponta para uma lista encadeada de nós
 * - Permite busca rápida O(1) em média de blocos pelo nonce
 * - 'prox' aponta para o próximo nó na mesma lista (colisões)
 */
typedef struct NoHash {
    unsigned int nonce;           // Nonce usado na mineração do bloco
    unsigned int idBloco;         // ID sequencial do bloco (1 a N)
    struct NoHash *prox;          // Ponteiro para próximo nó (lista encadeada)
} NoHash;

/**
 * Nó do Índice de Mineradores
 * 
 * Estrutura de dados: Array de 256 listas encadeadas
 * - Cada endereço (0-255) tem sua própria lista de blocos
 * - Inserção sempre no FIM da lista (ordem cronológica preservada)
 * - 'prox' aponta para o próximo bloco minerado pelo mesmo endereço
 */
typedef struct NoMinerador {
    unsigned int idBloco;         // ID do bloco minerado
    struct NoMinerador *prox;     // Próximo bloco do mesmo minerador
} NoMinerador;

/**
 * Estrutura de Estatísticas do Sistema
 * Mantém informações agregadas calculadas durante a operação
 */
typedef struct {

    unsigned int maxTransacoes;         // Maior número de transações em um bloco
    unsigned int blocoMaxTransacoes;    // ID do bloco com mais transações
    unsigned int totalBlocos;           // Contador total de blocos no sistema
} Estatisticas;

/**
 * Wrapper seguro para malloc com verificação de erro
 * - Aloca memória e verifica se a alocação foi bem-sucedida
 * - Em caso de falha, exibe mensagem de erro e encerra programa
 * - 'contexto' identifica qual parte do código chamou (para debug)
 */
void *verifica_malloc(size_t tamanho, const char *contexto);

#endif