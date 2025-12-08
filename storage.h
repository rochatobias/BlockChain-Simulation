#ifndef STORAGE_H
#define STORAGE_H

#include "structs.h"

// ============================================================================
// INICIALIZAÇÃO DO SISTEMA
// ============================================================================

/**
 * Inicializa o sistema de armazenamento
 * - Se arquivo existe: reconstrói índices e estado financeiro
 * - Se não existe: cria arquivo novo
 */
void inicializarStorage(const char *nomeArquivo);

/**
 * Finaliza o sistema: grava buffer pendente e libera memória
 */
void finalizarStorage();

// ============================================================================
// API PARA MINERAÇÃO
// ============================================================================

/**
 * Retorna saldo atual de um endereço (para validar transações)
 */
unsigned int getSaldo(unsigned char endereco);

/**
 * Copia o hash do último bloco minerado para o buffer
 * (usado para encadeamento na mineração)
 */
void getUltimoHash(unsigned char *bufferHash);

/**
 * Adiciona bloco minerado ao sistema:
 * - Atualiza índices (nonce, minerador)
 * - Atualiza saldos e estatísticas
 * - Grava no disco (bufferizado)
 */
void adicionarBloco(BlocoMinerado *bloco);

// ============================================================================
// API PARA CONSULTAS AUXILIARES
// ============================================================================

/**
 * Retorna o total de blocos minerados
 */
unsigned int obterTotalBlocos();

/**
 * Busca um bloco pelo seu ID (1 até totalBlocos)
 * Retorna 1 se encontrou, 0 se não existe
 */
int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida);

// ============================================================================
// API PARA RELATÓRIOS ESTATÍSTICOS (Itens A, B, C, D, E)
// ============================================================================

/**
 * Exibe todos os relatórios de uma vez (A até E)
 */
void exibirRelatoriosEstatisticos();

/**
 * Item A: Endereço(s) com mais bitcoins
 */
void relatorioMaisRico();

/**
 * Item B: Endereço(s) que minerou mais blocos
 */
void relatorioMaiorMinerador();

/**
 * Item C: Bloco(s) com mais transações (exibe hash)
 */
void relatorioMaxTransacoes();

/**
 * Item D: Bloco(s) com menos transações (exibe hash)
 */
void relatorioMinTransacoes();

/**
 * Item E: Média de bitcoins transacionados por bloco
 */
void calcularMediaBitcoinsPorBloco();

// ============================================================================
// API PARA CONSULTAS INTERATIVAS (Itens F, G, H, I)
// ============================================================================

/**
 * Item F: Imprime todos os campos de um bloco pelo seu número
 */
void imprimirBlocoPorNumero(unsigned int numero);

/**
 * Item G: Lista os N primeiros blocos minerados por um endereço
 * Usa índice de mineradores para acesso eficiente
 */
void listarBlocosMinerador(unsigned char endereco, int n);

/**
 * Item H: Lista N blocos ordenados por quantidade de transações
 * Usa Bucket Sort para ordenação O(n)
 */
void relatorioTransacoes(unsigned int n);

/**
 * Item I: Lista todos os blocos com um dado nonce
 * Usa Hash Table para busca O(1) em média
 * Retorna quantidade de blocos encontrados
 */
int listarBlocosPorNonce(unsigned int nonce);

// ============================================================================
// UTILITÁRIOS
// ============================================================================

/**
 * Wrapper seguro para malloc com verificação de erro
 */
void *verifica_malloc(size_t tamanho, const char *contexto);

#endif