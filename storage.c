/*
 * SISTEMA DE ARMAZENAMENTO DA BLOCKCHAIN
 * * Sistema híbrido: Disco (persistência) + RAM (índices rápidos)
 * - Hash Table de Nonces: busca O(1)
 * - Índice de Mineradores: listagem cronológica O(1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

// ============================================================================
// CONSTANTES E DEFINIÇÕES
// ============================================================================

// I/O e performance
#define READ_LOTE 256
#define BUFFER_SIZE 16

// Hash table
#define HASH_BITS 19
#define TAM_HASH (1 << HASH_BITS)
#define SHIFT_AMOUNT (32 - HASH_BITS)
#define KNUTH_CONST 2654435761u

// Estrutura de blocos
#define MINERADOR_OFFSET 183
#define TRANSACAO_SIZE 3
#define MAX_TRANSACOES 61
#define NUM_BUCKETS (MAX_TRANSACOES + 1)
#define NUM_ENDERECOS 256

// ============================================================================
// ESTRUTURAS GLOBAIS (static = visível apenas neste arquivo)
// ============================================================================

// Índice 1: Hash Table para busca rápida por nonce
static NoHash *tabelaNonce[TAM_HASH];

// Índice 2: Listas de blocos por minerador (ordem cronológica)
static NoMinerador *indiceMinerador[NUM_ENDERECOS]; 
static NoMinerador *fimMinerador[NUM_ENDERECOS];

// Arquivo de disco e buffer de escrita
static FILE *arquivoAtual = NULL;
static BlocoMinerado buffer[BUFFER_SIZE];
static int contadorBuffer = 0;

// Estatísticas do sistema (Usamos apenas totalBlocos aqui)
static Estatisticas stats;

// Protótipo local para uso interno
void imprimirBlocoCompleto(BlocoMinerado *b);

// ============================================================================
// FUNÇÕES AUXILIARES (MEMÓRIA E HASH)
// ============================================================================

// Hash multiplicativo de Knuth - distribuição uniforme
static unsigned int hashFunction(unsigned int nonce) {
    unsigned int hash = nonce * KNUTH_CONST;
    return hash >> SHIFT_AMOUNT;
}

void *verifica_malloc(size_t tamanho, const char *contexto) {
    void *ptr = malloc(tamanho);
    if (ptr == NULL) {
        fprintf(stderr, "ERRO FATAL: malloc falhou (%zu bytes em %s)\n", tamanho, contexto);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void inserirNonce(unsigned int nonce, unsigned int idBloco) {
    unsigned int pos = hashFunction(nonce);
    NoHash *novo = verifica_malloc(sizeof(NoHash), "inserirNonce");

    novo->nonce = nonce;
    novo->idBloco = idBloco;
    novo->prox = tabelaNonce[pos];
    tabelaNonce[pos] = novo;
}

static void inserirMinerador(unsigned char endereco, unsigned int idBloco) {
    NoMinerador *novo = verifica_malloc(sizeof(NoMinerador), "inserirMinerador");
    novo->idBloco = idBloco;
    novo->prox = NULL;

    if (indiceMinerador[endereco] == NULL)
        indiceMinerador[endereco] = novo;
    else 
        fimMinerador[endereco]->prox = novo;
    
    fimMinerador[endereco] = novo;
}

static void resetarIndices() {
    // Libera hash table
    for (int i = 0; i < TAM_HASH; i++) {
        NoHash *atual = tabelaNonce[i];
        while (atual != NULL) {
            NoHash *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        tabelaNonce[i] = NULL;
    }
    
    // Libera índice de mineradores
    for (int i = 0; i < NUM_ENDERECOS; i++) {
        NoMinerador *atual = indiceMinerador[i];
        while (atual != NULL) {
            NoMinerador *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        indiceMinerador[i] = NULL;
        fimMinerador[i] = NULL;
    }
    
    stats.totalBlocos = 0;
    contadorBuffer = 0;
}

// ============================================================================
// FUNÇÕES AUXILIARES (ARQUIVO E BUFFER)
// ============================================================================

static void flushBuffer() {
    if (contadorBuffer > 0 && arquivoAtual != NULL) {
        fseek(arquivoAtual, 0, SEEK_END);
        fwrite(buffer, sizeof(BlocoMinerado), contadorBuffer, arquivoAtual);
        fflush(arquivoAtual);
        contadorBuffer = 0;
    }
}

static void reconstruirIndicesDoDisco() {
    BlocoMinerado lote[READ_LOTE];
    size_t blocosLidos;
    unsigned int idCalculado = 1;

    rewind(arquivoAtual);

    while ((blocosLidos = fread(lote, sizeof(BlocoMinerado), READ_LOTE, arquivoAtual)) > 0) {
        for (size_t i = 0; i < blocosLidos; i++) {
            unsigned char minerador = lote[i].bloco.data[MINERADOR_OFFSET];

            inserirNonce(lote[i].bloco.nonce, idCalculado);
            inserirMinerador(minerador, idCalculado);
            
            stats.totalBlocos = idCalculado;
            idCalculado++;
        }
    }
    printf("Sistema restaurado. %u blocos indexados na RAM.\n", stats.totalBlocos);
}

// ============================================================================
// FUNÇÕES AUXILIARES (LÓGICA DE BLOCOS)
// ============================================================================

static int contarTransacoes(BlocoMinerado *bloco) {
    if (bloco->bloco.numero == 1) return 0; // Bloco Gênesis

    int contagem = 0;
    for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) {
        // Verifica fim das transações (tripla de zeros)
        if (bloco->bloco.data[i] == 0 && bloco->bloco.data[i+1] == 0 && bloco->bloco.data[i+2] == 0) {
             if (i + 3 < MINERADOR_OFFSET && bloco->bloco.data[i+3] == 0) break;
        }
        contagem++;
    }
    return contagem;
}

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida) {
    if (id < 1 || id > stats.totalBlocos) return 0;

    unsigned int blocosPersistidos = stats.totalBlocos - contadorBuffer;

    // Bloco no buffer
    if (id > blocosPersistidos) {
        unsigned int idx = id - blocosPersistidos - 1;
        if (idx < (unsigned int)contadorBuffer) {
            *saida = buffer[idx];
            return 1;
        }
        return 0;
    }

    // Bloco no disco
    long offset = (long)(id - 1) * sizeof(BlocoMinerado);

    if (arquivoAtual == NULL) return 0;
    if (fseek(arquivoAtual, offset, SEEK_SET) != 0) return 0;
    if (fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual) != 1) return 0;
    return 1;
}

// ============================================================================
// FUNÇÕES PÚBLICAS DE GERENCIAMENTO
// ============================================================================

void inicializarStorage(const char *nomeArquivo) {
    arquivoAtual = fopen(nomeArquivo, "rb+");

    if (arquivoAtual == NULL) {
        arquivoAtual = fopen(nomeArquivo, "wb+");
        if (!arquivoAtual) {
            perror("Erro fatal ao criar blockchain!");
            exit(1);
        }
        resetarIndices();
    } else {
        resetarIndices();
        reconstruirIndicesDoDisco();
    }
}

void adicionarBloco(BlocoMinerado *bloco) {
    stats.totalBlocos++;
    unsigned char minerador = bloco->bloco.data[MINERADOR_OFFSET];
    
    inserirNonce(bloco->bloco.nonce, stats.totalBlocos);
    inserirMinerador(minerador, stats.totalBlocos);

    buffer[contadorBuffer] = *bloco;
    contadorBuffer++;

    if (contadorBuffer == BUFFER_SIZE) 
        flushBuffer();
}

void finalizarStorage() {
    flushBuffer();
    if (arquivoAtual) fclose(arquivoAtual);
    resetarIndices();
}

// ============================================================================
// FUNÇÕES PÚBLICAS DE ACESSO (INTEGRAÇÃO COM MAIN)
// ============================================================================

unsigned int obterTotalBlocos() {
    return stats.totalBlocos;
}

int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida) {
    return lerBlocoPorId(id, saida);
}

void imprimirBlocoPorNumero(unsigned int numero) {
    BlocoMinerado temp;
    if (lerBlocoPorId(numero, &temp)) 
        imprimirBlocoCompleto(&temp);
    else 
        printf("Erro: Bloco %u nao encontrado.\n", numero);
}

void listarBlocosPorNonce(unsigned int nonce) {
    unsigned int pos = hashFunction(nonce);
    NoHash *atual = tabelaNonce[pos];
    int encontrados = 0;
    
    printf("\n--- Blocos com Nonce %u ---\n", nonce);
    while (atual != NULL) {
        if (atual->nonce == nonce) {
            BlocoMinerado temp;
            if (lerBlocoPorId(atual->idBloco, &temp)) {
                imprimirBlocoCompleto(&temp);
                encontrados++;
            }
        }
        atual = atual->prox;
    }
    
    if (encontrados == 0) printf("Nenhum bloco encontrado com esse nonce.\n");
    else printf("Total encontrado: %d\n", encontrados);
}

void listarBlocosMinerador(unsigned char endereco, int n) {
    NoMinerador *atual = indiceMinerador[endereco];
    if (atual == NULL) {
        printf("Nenhum bloco encontrado para o minerador %d.\n", endereco);
        return;
    }

    printf("--- Primeiros %d Blocos do Minerador %d ---\n", n, endereco);
    int contagem = 0;
    while (atual != NULL && contagem < n) {
        BlocoMinerado temp;
        if (lerBlocoPorId(atual->idBloco, &temp)) {
            imprimirBlocoCompleto(&temp);
            contagem++;
        }
        atual = atual->prox;
    }
}

// ============================================================================
// FUNÇÕES DE RELATÓRIO (ITENS DO MENU)
// ============================================================================

void relatorioMaxTransacoes() {
    if (stats.totalBlocos == 0) { printf("Blockchain vazia.\n"); return; }
    unsigned int maxTx = 0;

    // Passada 1: Máximo
    for (unsigned int i = 1; i <= stats.totalBlocos; i++) {
        BlocoMinerado temp;
        if (lerBlocoPorId(i, &temp)) {
            unsigned int qtd = contarTransacoes(&temp);
            if (qtd > maxTx) maxTx = qtd;
        }
    }

    printf("\n--- Bloco(s) com MAIS transacoes ---\n");
    printf("Quantidade Maxima: %u\n", maxTx);
    printf("Blocos IDs: ");
    
    // Passada 2: Imprimir IDs
    for (unsigned int i = 1; i <= stats.totalBlocos; i++) {
        BlocoMinerado temp;
        if (lerBlocoPorId(i, &temp)) {
            if ((unsigned int)contarTransacoes(&temp) == maxTx) printf("%u ", i);
        }
    }
    printf("\n");
}

void relatorioMinTransacoes() {
    if (stats.totalBlocos == 0) { printf("Blockchain vazia.\n"); return; }
    unsigned int minTx = MAX_TRANSACOES + 1;

    // Passada 1: Mínimo (ignora Gênesis)
    for (unsigned int i = 2; i <= stats.totalBlocos; i++) {
        BlocoMinerado temp;
        if (lerBlocoPorId(i, &temp)) {
            unsigned int qtd = contarTransacoes(&temp);
            if (qtd < minTx) minTx = qtd;
        }
    }

    printf("\n--- Bloco(s) com MENOS transacoes ---\n");
    printf("Quantidade Minima: %u\n", minTx);
    printf("Blocos IDs: ");
    
    for (unsigned int i = 2; i <= stats.totalBlocos; i++) {
        BlocoMinerado temp;
        if (lerBlocoPorId(i, &temp)) {
            if ((unsigned int)contarTransacoes(&temp) == minTx) printf("%u ", i);
        }
    }
    printf("\n");
}

void calcularMediaBitcoinsPorBloco() {
    if (stats.totalBlocos == 0) { printf("Blockchain vazia.\n"); return; }
    unsigned long long totalBTC = 0;
    
    for (unsigned int i = 1; i <= stats.totalBlocos; i++) {
        BlocoMinerado temp;
        if (lerBlocoPorId(i, &temp)) {
            totalBTC += 50; // Recompensa
            if (temp.bloco.numero > 1) {
                for (int j = 0; j < MINERADOR_OFFSET; j += TRANSACAO_SIZE) {
                    totalBTC += temp.bloco.data[j+2];
                }
            }
        }
    }

    double media = (double)totalBTC / stats.totalBlocos;
    printf("\n--- Media de Bitcoins ---\n");
    printf("Media por Bloco: %.2f BTC\n", media);
}

// Relatório Bucket Sort (Item h)
void relatorioTransacoes(unsigned int n) {
    if (n > stats.totalBlocos) n = stats.totalBlocos;
    if (n == 0) return;
    
    BlocoMinerado* blocos = verifica_malloc(n * sizeof(BlocoMinerado), "relatorio_blocos");
    for (unsigned int i = 0; i < n; i++) {
        if (!lerBlocoPorId(i + 1, &blocos[i])) {
            free(blocos); return;
        }
    }
    
    int *next = verifica_malloc(n * sizeof(int), "relatorio_next");
    int buckets[NUM_BUCKETS];
    for (int i = 0; i < NUM_BUCKETS; i++) buckets[i] = -1;
    
    // Bucket Sort
    for (int i = (int)n - 1; i >= 0; i--) {
        int qtd = contarTransacoes(&blocos[i]);
        if (qtd > MAX_TRANSACOES) qtd = MAX_TRANSACOES;
        next[i] = buckets[qtd];
        buckets[qtd] = i;
    }
    
    printf("--- Relatorio (Ordenado por Transacoes) ---\n");
    unsigned int totalImpresso = 0;
    for (int trans = 0; trans < NUM_BUCKETS; trans++) {
        if (buckets[trans] == -1) continue;
        printf("\n=== Blocos com %d transacao(oes) ===\n", trans);
        for (int idx = buckets[trans]; idx != -1; idx = next[idx]) {
            imprimirBlocoCompleto(&blocos[idx]);
            totalImpresso++;
        }
    }
    printf("Blocos impressos: %u\n", totalImpresso);
    
    free(next);
    free(blocos);
}

// ============================================================================
// VISUALIZAÇÃO E DEBUG
// ============================================================================

void imprimirBlocoCompleto(BlocoMinerado *b) {
    printf("==================================================\n");
    printf("BLOCO #%u\n", b->bloco.numero);
    printf("Minerador: %u | Nonce: %u\n", b->bloco.data[MINERADOR_OFFSET], b->bloco.nonce);
    
    printf("Hash: ");
    for(int i=0; i<SHA256_LEN; i++) printf("%02x", b->hash[i]);
    printf("\nHash Ant: ");
    for(int i=0; i<SHA256_LEN; i++) printf("%02x", b->bloco.hashAnterior[i]);
    printf("\n");

    printf("--------------------------------------------------\n");
    if (b->bloco.numero == 1) {
        printf("  [Dados]: %s\n", b->bloco.data);
    } else {
        int qtd = 0;
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) {
            unsigned char o = b->bloco.data[i];
            unsigned char d = b->bloco.data[i+1];
            unsigned char v = b->bloco.data[i+2];
            if (o!=0 || d!=0 || v!=0) {
                printf("  Tx %d: %3u -> %3u | $ %3u\n", qtd+1, o, d, v);
                qtd++;
            }
        }
        printf("Total Transações: %d\n", qtd);
    }
    printf("==================================================\n");
}

void relatorioColisoes() {
    printf("\n=== DIAGNÓSTICO DE COLISÕES ===\n");
    printf("Total Blocos: %u. Tamanho Hash: %d\n", stats.totalBlocos, TAM_HASH);
}