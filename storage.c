/*
 * SISTEMA DE ARMAZENAMENTO DA BLOCKCHAIN (VERSÃO OTIMIZADA)
 * Integração: Persistência + Índices RAM + Gestão Financeira
 * 
 * TRADE-OFFS DE DESEMPENHO/MEMÓRIA:
 * 
 * 1. Hash Table (2^14 slots): ~64KB de ponteiros
 *    - Pro: Busca O(1) por nonce em média
 *    - Contra: Memória fixa mesmo se poucos nonces únicos
 * 
 * 2. Índice Mineradores: Array de 256 listas
 *    - Pro: Acesso direto O(1) por endereço
 *    - Contra: 256 ponteiros sempre alocados (2KB)
 * 
 * 3. Listas de Recordes: Dinâmicas para MAX/MIN transações
 *    - Pro: Sem limite de empates, libera automaticamente
 *    - Contra: Overhead de alocação por nó
 * 
 * 4. Buffer de escrita: 16 blocos
 *    - Pro: Reduz I/O em 16x (requisito ED2)
 *    - Contra: Perda de dados se crash antes do flush
 * 
 * 5. OTIMIZAÇÃO 4 - Cache de Contagem de Transações:
 *    - PROBLEMA: contarTransacoes() percorre 183 bytes (O(61)) a cada chamada
 *    - FREQUÊNCIA: Chamado 2x por bloco (estatísticas + relatórios)
 *    - SEM CACHE: 30.000 blocos × 2 chamadas × O(61) = 3.660.000 iterações
 *    - COM CACHE: 30.000 blocos × 1 cálculo + O(1) consultas = 1.830.000 iterações
 *    - GANHO: ~50% menos iterações, consultas O(1) em vez de O(61)
 *    - CUSTO: 30.000 × 1 byte = 30KB de RAM extra
 *    - CONCLUSÃO: Trade-off favorável (30KB RAM economiza milhões de iterações)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

// ============================================================================
// 1. CONFIGURAÇÕES E CONSTANTES
// ============================================================================

#define READ_LOTE 256       // Quantidade de blocos lidos por vez do disco
#define BUFFER_SIZE 16      // Blocos em memória antes de flush (requisito ED2)

// Hash Table de Nonces (2^14 = 16384 slots)
#define HASH_BITS 14
#define TAM_HASH (1 << HASH_BITS)
#define SHIFT_AMOUNT (32 - HASH_BITS)
#define KNUTH_CONST 2654435761u  // Constante de Knuth para hash multiplicativo

// Estrutura do Bloco
#define MINERADOR_OFFSET 183    // Posição fixa do minerador no vetor data
#define TRANSACAO_SIZE 3        // Cada transação = 3 bytes (origem, destino, valor)
#define MAX_TRANSACOES 61       // Máximo de transações por bloco
#define NUM_ENDERECOS 256       // Total de endereços possíveis (0-255)

// OTIMIZAÇÃO 4: Cache de contagem (cresce dinamicamente)
#define CACHE_INICIAL 1000      // Capacidade inicial do cache
#define CACHE_CRESCIMENTO 2     // Fator de crescimento quando cheio

// ============================================================================
// 2. ESTRUTURAS AUXILIARES
// ============================================================================

// Nó para lista de blocos com recordes de transações
typedef struct NoRecorde {
    unsigned int idBloco;
    struct NoRecorde *prox;
} NoRecorde;

// ============================================================================
// 3. VARIÁVEIS GLOBAIS (ESTADO DO SISTEMA)
// ============================================================================

// A. Índices de Busca Rápida
static NoHash *tabelaNonce[TAM_HASH];               // Hash table para busca por nonce O(1)
static NoMinerador *indiceMinerador[NUM_ENDERECOS]; // Início de cada lista de minerador
static NoMinerador *fimMinerador[NUM_ENDERECOS];    // Fim de cada lista (inserção O(1))

// B. Estado Financeiro e Estatístico
static unsigned int saldos[NUM_ENDERECOS];          // Saldo atual de cada carteira
static unsigned int blocosMinerados[NUM_ENDERECOS]; // Contador de blocos por minerador
static unsigned long long totalValorTransacionado = 0; // Soma de todos os valores (para média)

// C. Caches de Recordes (Listas Dinâmicas)
static unsigned int maiorSaldoAtual = 0;            // Cache do maior saldo
static unsigned int maiorQtdMinerada = 0;           // Cache da maior qtd minerada

static int maxTransacoesGlobal = -1;                // Recorde de MAX transações (-1 = não inicializado)
static NoRecorde *listaMaxTx = NULL;                // Lista de blocos com MAX transações

static int minTransacoesGlobal = 1000;              // Recorde de MIN transações (inicia alto)
static NoRecorde *listaMinTx = NULL;                // Lista de blocos com MIN transações

// D. Gestão de Arquivo
static FILE *arquivoAtual = NULL;
static BlocoMinerado buffer[BUFFER_SIZE];
static int contadorBuffer = 0;
static Estatisticas stats;

// E. OTIMIZAÇÃO 4: Cache de Contagem de Transações
/*
 * ANÁLISE DE TRADE-OFF:
 * 
 * MEMÓRIA:
 *   - Array de unsigned char (0-61 transações cabe em 1 byte)
 *   - 30.000 blocos × 1 byte = ~30KB
 *   - Cresce dinamicamente apenas quando necessário
 * 
 * TEMPO SEM CACHE:
 *   - contarTransacoes() percorre até 183 bytes (61 transações × 3 bytes)
 *   - Item H (relatório ordenado): Chama para todos os N blocos
 *   - Com N=30000: 30.000 × O(61) = até 1.830.000 comparações
 * 
 * TEMPO COM CACHE:
 *   - Cálculo feito UMA VEZ quando bloco entra no sistema
 *   - Consultas: O(1) - apenas acesso ao array
 *   - Economia: Todas as chamadas subsequentes são O(1)
 * 
 * QUANDO O CACHE É USADO:
 *   - atualizarEstatisticasGlobais(): conta e armazena
 *   - relatorioTransacoes() (Item H): usa cache em vez de recontar
 *   - Qualquer função que precise da contagem: O(1)
 * 
 * RESULTADO: 30KB de RAM economiza ~1.8 milhões de iterações por relatório
 */
static unsigned char *cacheContagemTx = NULL;  // Cache: cacheContagemTx[idBloco-1] = qtd transações
static unsigned int cacheTamanho = 0;           // Tamanho atual do array
static unsigned int cacheCapacidade = 0;        // Capacidade alocada

// ============================================================================
// 4. PROTÓTIPOS INTERNOS
// ============================================================================

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida);
static void liberarListaRecorde(NoRecorde **lista);
static void adicionarRecorde(NoRecorde **lista, unsigned int idBloco);
static void expandirCacheTx();
static void adicionarAoCache(unsigned int idBloco, unsigned char qtdTx);
void imprimirBlocoCompleto(BlocoMinerado *b);

// ============================================================================
// 5. FUNÇÕES AUXILIARES DE MEMÓRIA
// ============================================================================

// Libera todos os nós de uma lista de recordes
static void liberarListaRecorde(NoRecorde **lista) {
    NoRecorde *atual = *lista;
    while (atual != NULL) {
        NoRecorde *temp = atual;
        atual = atual->prox;
        free(temp);
    }
    *lista = NULL;
}

// Adiciona um bloco à lista de recordes (inserção no início = O(1))
static void adicionarRecorde(NoRecorde **lista, unsigned int idBloco) {
    NoRecorde *novo = verifica_malloc(sizeof(NoRecorde), "adicionarRecorde");
    novo->idBloco = idBloco;
    novo->prox = *lista;
    *lista = novo;
}

// ============================================================================
// 6. OTIMIZAÇÃO 4: CACHE DE CONTAGEM DE TRANSAÇÕES
// ============================================================================

// Expande o cache quando necessário
static void expandirCacheTx() {
    unsigned int novaCapacidade = cacheCapacidade == 0 ? CACHE_INICIAL : cacheCapacidade * CACHE_CRESCIMENTO;
    
    unsigned char *novoCache = realloc(cacheContagemTx, novaCapacidade * sizeof(unsigned char));
    if (!novoCache) {
        fprintf(stderr, "Erro ao expandir cache de transações\n");
        exit(1);
    }
    
    // Inicializa novas posições com 0
    memset(novoCache + cacheCapacidade, 0, (novaCapacidade - cacheCapacidade) * sizeof(unsigned char));
    
    cacheContagemTx = novoCache;
    cacheCapacidade = novaCapacidade;
}

// Adiciona contagem ao cache
static void adicionarAoCache(unsigned int idBloco, unsigned char qtdTx) {
    // Expande se necessário
    while (idBloco > cacheCapacidade) {
        expandirCacheTx();
    }
    
    cacheContagemTx[idBloco - 1] = qtdTx;
    if (idBloco > cacheTamanho) {
        cacheTamanho = idBloco;
    }
}

// Obtém contagem do cache (O(1))
static unsigned char obterContagemDoCache(unsigned int idBloco) {
    if (idBloco == 0 || idBloco > cacheTamanho) {
        return 0;
    }
    return cacheContagemTx[idBloco - 1];
}

// ============================================================================
// 7. LÓGICA DE NEGÓCIO (CONTAGEM E ESTATÍSTICAS)
// ============================================================================


// Atualiza todas as estatísticas quando um bloco entra no sistema
static void atualizarEstatisticasGlobais(BlocoMinerado *b) {
    unsigned char minerador = b->bloco.data[MINERADOR_OFFSET];
    
    // 1. Recompensa do minerador (+50 BTC)
    saldos[minerador] += 50;
    blocosMinerados[minerador]++;
    
    // 2. Atualiza caches de máximos
    if (saldos[minerador] > maiorSaldoAtual) 
        maiorSaldoAtual = saldos[minerador];
    if (blocosMinerados[minerador] > maiorQtdMinerada) 
        maiorQtdMinerada = blocosMinerados[minerador];

    // 3. Processa transações (exceto Gênesis)
    int txNoBloco = 0;
    if (b->bloco.numero > 1) {
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) {
            unsigned char origem = b->bloco.data[i];
            unsigned char destino = b->bloco.data[i + 1];
            unsigned char valor = b->bloco.data[i + 2];

            if (valor > 0) {
                if (saldos[origem] >= valor) {
                    saldos[origem] -= valor;
                    saldos[destino] += valor;
                    totalValorTransacionado += valor;
                    txNoBloco++;
                } else {
                    fprintf(stderr, "AVISO: Tx inválida no bloco %u (origem %d tem %u, tentou %u)\n", 
                            b->bloco.numero, origem, saldos[origem], valor);
                }
            } else if (origem == 0 && destino == 0) {
                break;
            }
        }
    }
    
    // OTIMIZAÇÃO 4: Armazena contagem no cache
    adicionarAoCache(b->bloco.numero, (unsigned char)txNoBloco);
    
    // 4. Atualiza recordes de MAX transações
    if (txNoBloco > maxTransacoesGlobal) {
        liberarListaRecorde(&listaMaxTx);
        maxTransacoesGlobal = txNoBloco;
        adicionarRecorde(&listaMaxTx, b->bloco.numero);
    } else if (txNoBloco == maxTransacoesGlobal && maxTransacoesGlobal >= 0) {
        adicionarRecorde(&listaMaxTx, b->bloco.numero);
    }
    
    // 5. Atualiza recordes de MIN transações (ignora Gênesis)
    if (b->bloco.numero > 1) {
        if (txNoBloco < minTransacoesGlobal) {
            liberarListaRecorde(&listaMinTx);
            minTransacoesGlobal = txNoBloco;
            adicionarRecorde(&listaMinTx, b->bloco.numero);
        } else if (txNoBloco == minTransacoesGlobal) {
            adicionarRecorde(&listaMinTx, b->bloco.numero);
        }
    }
}

// ============================================================================
// 8. FUNÇÕES DE HASH TABLE E ÍNDICES
// ============================================================================

static unsigned int hashFunction(unsigned int nonce) {
    return (nonce * KNUTH_CONST) >> SHIFT_AMOUNT;
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

// ============================================================================
// 9. FUNÇÕES DE ARQUIVO
// ============================================================================

static void flushBuffer() {
    if (contadorBuffer > 0 && arquivoAtual != NULL) {
        fseek(arquivoAtual, 0, SEEK_END);
        fwrite(buffer, sizeof(BlocoMinerado), contadorBuffer, arquivoAtual);
        fflush(arquivoAtual);
        contadorBuffer = 0;
    }
}

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida) {
    if (id < 1 || id > stats.totalBlocos) return 0;
    
    unsigned int blocosPersistidos = stats.totalBlocos - contadorBuffer;

    if (id > blocosPersistidos) {
        *saida = buffer[id - blocosPersistidos - 1];
        return 1;
    }
    
    long offset = (long)(id - 1) * sizeof(BlocoMinerado);
    if (fseek(arquivoAtual, offset, SEEK_SET) != 0) return 0;
    if (fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual) != 1) return 0;
    return 1;
}

static void reconstruirIndicesDoDisco() {
    BlocoMinerado lote[READ_LOTE];
    size_t blocosLidos;
    unsigned int idCalculado = 1;

    rewind(arquivoAtual);

    while ((blocosLidos = fread(lote, sizeof(BlocoMinerado), READ_LOTE, arquivoAtual)) > 0) {
        for (size_t i = 0; i < blocosLidos; i++) {
            inserirNonce(lote[i].bloco.nonce, idCalculado);
            inserirMinerador(lote[i].bloco.data[MINERADOR_OFFSET], idCalculado);
            atualizarEstatisticasGlobais(&lote[i]);
            stats.totalBlocos = idCalculado;
            idCalculado++;
        }
    }
    printf("Sistema restaurado: %u blocos. Saldo máximo: %u BTC.\n", 
           stats.totalBlocos, maiorSaldoAtual);
}

static void resetarIndices() {
    // Limpa Hash Table de Nonces
    for (int i = 0; i < TAM_HASH; i++) {
        NoHash *atual = tabelaNonce[i];
        while (atual != NULL) {
            NoHash *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        tabelaNonce[i] = NULL;
    }
    
    // Limpa Índice de Mineradores
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
    
    // Limpa listas de recordes
    liberarListaRecorde(&listaMaxTx);
    liberarListaRecorde(&listaMinTx);
    
    // Limpa cache de contagem
    if (cacheContagemTx != NULL) {
        free(cacheContagemTx);
        cacheContagemTx = NULL;
    }
    cacheTamanho = 0;
    cacheCapacidade = 0;
    
    // Zera estado financeiro
    memset(saldos, 0, sizeof(saldos));
    memset(blocosMinerados, 0, sizeof(blocosMinerados));
    totalValorTransacionado = 0;
    maiorSaldoAtual = 0;
    maiorQtdMinerada = 0;
    maxTransacoesGlobal = -1;
    minTransacoesGlobal = 1000;
    
    stats.totalBlocos = 0;
    contadorBuffer = 0;
}

static void imprimirListaRecordes(NoRecorde *lista, const char *titulo, int valor) {
    printf("\n--- %s (%d tx) ---\n", titulo, valor); // Título dinâmico
    
    BlocoMinerado temp;
    for(NoRecorde *r = lista; r != NULL; r = r->prox) {
        if (lerBlocoPorId(r->idBloco, &temp)) {
            printf("Bloco %u | Hash: ", r->idBloco);
            for(int j = 0; j < 32; j++) printf("%02x", temp.hash[j]);
            printf("\n");
        }
    }
}

// ============================================================================
// 10. API PÚBLICA - INTEGRAÇÃO COM MINERAÇÃO
// ============================================================================

void *verifica_malloc(size_t tamanho, const char *contexto) {
    void *ptr = malloc(tamanho);
    if (!ptr) { 
        fprintf(stderr, "Erro malloc: %s\n", contexto); 
        exit(1); 
    }
    return ptr;
}

void inicializarStorage(const char *nomeArquivo) {
    arquivoAtual = fopen(nomeArquivo, "rb+");
    if (arquivoAtual == NULL) {
        arquivoAtual = fopen(nomeArquivo, "wb+");
        if (!arquivoAtual) { perror("Erro ao abrir arquivo"); exit(1); }
        resetarIndices();
    } else {
        resetarIndices();
        reconstruirIndicesDoDisco();
    }
}

unsigned int getSaldo(unsigned char endereco) {
    return saldos[endereco];
}

void getUltimoHash(unsigned char *bufferHash) {
    if (stats.totalBlocos == 0) {
        memset(bufferHash, 0, SHA256_DIGEST_LENGTH);
        return;
    }
    
    BlocoMinerado ultimo;
    if (lerBlocoPorId(stats.totalBlocos, &ultimo)) {
        memcpy(bufferHash, ultimo.hash, SHA256_DIGEST_LENGTH);
    } else {
        memset(bufferHash, 0, SHA256_DIGEST_LENGTH);
        fprintf(stderr, "ERRO: Falha ao ler último bloco\n");
    }
}

void adicionarBloco(BlocoMinerado *bloco) {
    stats.totalBlocos++;
    
    inserirNonce(bloco->bloco.nonce, stats.totalBlocos);
    inserirMinerador(bloco->bloco.data[MINERADOR_OFFSET], stats.totalBlocos);
    atualizarEstatisticasGlobais(bloco);

    buffer[contadorBuffer] = *bloco;
    contadorBuffer++;
    if (contadorBuffer == BUFFER_SIZE) flushBuffer();
}

void finalizarStorage() {
    flushBuffer();
    if (arquivoAtual) fclose(arquivoAtual);
    resetarIndices();
}

// ============================================================================
// 11. API PÚBLICA - FUNÇÕES AUXILIARES
// ============================================================================

unsigned int obterTotalBlocos() {
    return stats.totalBlocos;
}

int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida) {
    return lerBlocoPorId(id, saida);
}

// ============================================================================
// 12. API PÚBLICA - RELATÓRIOS ESTATÍSTICOS (ITENS A, B, C, D, E)
// ============================================================================

void exibirRelatoriosEstatisticos() {
    if (stats.totalBlocos == 0) { 
        printf("Blockchain vazia.\n"); 
        return; 
    }
    
    printf("\n=== RELATÓRIOS GERAIS ===\n");
    printf("Total Blocos: %u\n", stats.totalBlocos);

    // ITEM A: Endereço(s) com maior saldo
    printf("\na) Endereço(s) com MAIOR SALDO (%u BTC):\n", maiorSaldoAtual);
    for(int i = 0; i < NUM_ENDERECOS; i++) 
        if(saldos[i] == maiorSaldoAtual) 
            printf("   - Carteira %d\n", i);

    // ITEM B: Endereço(s) que mais minerou
    printf("\nb) Endereço(s) que MAIS MINEROU (%u blocos):\n", maiorQtdMinerada);
    for(int i = 0; i < NUM_ENDERECOS; i++) 
        if(blocosMinerados[i] == maiorQtdMinerada) 
            printf("   - Carteira %d\n", i);

    // ITEM C: Bloco(s) com máximo de transações
    printf("\nc) Bloco(s) com MÁXIMO de transações (%d tx):\n", maxTransacoesGlobal);
    BlocoMinerado temp;
    for(NoRecorde *r = listaMaxTx; r != NULL; r = r->prox) {
        if (lerBlocoPorId(r->idBloco, &temp)) {
            printf("   - Bloco %u | Hash: ", r->idBloco);
            for(int j = 0; j < 32; j++) printf("%02x", temp.hash[j]);
            printf("\n");
        }
    }

    // ITEM D: Bloco(s) com mínimo de transações
    printf("\nd) Bloco(s) com MÍNIMO de transações (%d tx):\n", minTransacoesGlobal);
    for(NoRecorde *r = listaMinTx; r != NULL; r = r->prox) {
        if (lerBlocoPorId(r->idBloco, &temp)) {
            printf("   - Bloco %u | Hash: ", r->idBloco);
            for(int j = 0; j < 32; j++) printf("%02x", temp.hash[j]);
            printf("\n");
        }
    }

    // ITEM E: Média de valor por bloco
    double media = (double)totalValorTransacionado / stats.totalBlocos;
    printf("\ne) Média de valor por bloco: %.2f BTC\n", media);
}

void relatorioMaisRico() {
    printf("\n--- Endereço(s) com mais Bitcoins (Item A) ---\n");
    printf("Saldo Máximo: %u BTC\n", maiorSaldoAtual);
    printf("Endereço(s): ");
    for(int i = 0; i < NUM_ENDERECOS; i++) {
        if(saldos[i] == maiorSaldoAtual) printf("%d ", i);
    }
    printf("\n");
}

void relatorioMaiorMinerador() {
    printf("\n--- Endereço(s) que mais minerou (Item B) ---\n");
    printf("Qtd Blocos: %u\n", maiorQtdMinerada);
    printf("Minerador(es): ");
    for(int i = 0; i < NUM_ENDERECOS; i++) {
        if(blocosMinerados[i] == maiorQtdMinerada) printf("%d ", i);
    }
    printf("\n");
}

void relatorioMaxTransacoes() {
    imprimirListaRecordes(listaMaxTx, "Bloco(s) com MAIS transações", maxTransacoesGlobal);
}

void relatorioMinTransacoes() {
    imprimirListaRecordes(listaMinTx, "Bloco(s) com MENOS transações", minTransacoesGlobal);
}

void calcularMediaBitcoinsPorBloco() {
    if (stats.totalBlocos == 0) {
        printf("Blockchain vazia.\n");
        return;
    }
    double media = (double)totalValorTransacionado / stats.totalBlocos;
    printf("\n--- Média de Bitcoins por Bloco (Item E) ---\n");
    printf("Total transacionado: %llu BTC\n", totalValorTransacionado);
    printf("Total de blocos: %u\n", stats.totalBlocos);
    printf("Média: %.2f BTC/bloco\n", media);
}

// ============================================================================
// 13. API PÚBLICA - CONSULTAS INTERATIVAS (ITENS F, G, H, I)
// ============================================================================

void imprimirBlocoPorNumero(unsigned int numero) {
    BlocoMinerado temp;
    if (lerBlocoPorId(numero, &temp)) {
        imprimirBlocoCompleto(&temp);
    } else {
        printf("Bloco %u não encontrado.\n", numero);
    }
}

void listarBlocosMinerador(unsigned char endereco, int n) {
    NoMinerador *atual = indiceMinerador[endereco];
    int count = 0;
    BlocoMinerado temp;
    
    printf("\n--- %d Primeiros Blocos do Minerador %d ---\n", n, endereco);
    
    while (atual != NULL && count < n) {
        if (lerBlocoPorId(atual->idBloco, &temp)) {
            imprimirBlocoCompleto(&temp);
        }
        atual = atual->prox;
        count++;
    }
    
    if (count == 0) {
        printf("Minerador %d não possui blocos.\n", endereco);
    }
}

/**
 * ITEM H: Lista N blocos ordenados por quantidade de transações
 * 
 * OTIMIZAÇÃO 4 APLICADA:
 * - Usa cache de contagem em vez de chamar contarTransacoes() para cada bloco
 * - Antes: O(N × 61) para contar transações
 * - Depois: O(N × 1) = O(N) para consultar cache
 * - Ganho: 61x menos operações no loop principal
 */
void relatorioTransacoes(unsigned int n) {
    if (n > stats.totalBlocos) n = stats.totalBlocos;
    if (n == 0) return;

    // Carrega N blocos em memória
    BlocoMinerado *blocos = verifica_malloc(n * sizeof(BlocoMinerado), "relatorioTransacoes");
    for (unsigned int i = 0; i < n; i++) {
        lerBlocoPorId(i + 1, &blocos[i]);
    }

    // Bucket Sort: 62 buckets (0 a 61 transações)
    int *next = verifica_malloc(n * sizeof(int), "next");
    int buckets[62];
    for(int i = 0; i < 62; i++) buckets[i] = -1;

    // OTIMIZAÇÃO 4: Usa cache em vez de recontar
    for (int i = (int)n - 1; i >= 0; i--) {
        // Consulta O(1) do cache em vez de O(61) de contarTransacoes
        int qtd = obterContagemDoCache(blocos[i].bloco.numero);
        if (qtd > 61) qtd = 61;
        next[i] = buckets[qtd];
        buckets[qtd] = i;
    }

    printf("\n--- Relatório Top %u Blocos (Ordenado por Transações) ---\n", n);
    
    for (int t = 0; t < 62; t++) {
        if (buckets[t] == -1) continue;
        printf("\n[ %d Transações ]\n", t);
        for (int idx = buckets[t]; idx != -1; idx = next[idx]) {
            imprimirBlocoCompleto(&blocos[idx]);
        }
    }
    
    free(next);
    free(blocos);
}

int listarBlocosPorNonce(unsigned int nonce) {
    unsigned int pos = hashFunction(nonce);
    NoHash *atual = tabelaNonce[pos];
    int encontrados = 0;
    BlocoMinerado temp;

    printf("\n--- Buscando Blocos com Nonce %u ---\n", nonce);

    while (atual != NULL) {
        if (atual->nonce == nonce) {
            if (lerBlocoPorId(atual->idBloco, &temp)) {
                imprimirBlocoCompleto(&temp);
                encontrados++;
            }
        }
        atual = atual->prox;
    }

    if (encontrados == 0) {
        printf("Nenhum bloco encontrado com o nonce %u.\n", nonce);
    } else {
        printf("Total de blocos encontrados: %d\n", encontrados);
    }
    
    return encontrados;
}

// ============================================================================
// 14. FUNÇÃO AUXILIAR DE IMPRESSÃO
// ============================================================================

void imprimirBlocoCompleto(BlocoMinerado *b) {
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("BLOCO %u | Minerador: %d | Nonce: %u\n", 
           b->bloco.numero, b->bloco.data[MINERADOR_OFFSET], b->bloco.nonce);
    printf("Hash: ");
    for(int i = 0; i < 32; i++) printf("%02x", b->hash[i]);
    printf("\n");
    
    // Mostra contagem do cache (para demonstrar funcionamento)
    printf("Transações: %d\n", obterContagemDoCache(b->bloco.numero));
    
    if (b->bloco.numero == 1) {
        printf("Dados (Gênesis): %s\n", b->bloco.data);
    } else {
        printf("Detalhes:\n");
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) {
            unsigned char origem = b->bloco.data[i];
            unsigned char destino = b->bloco.data[i + 1];
            unsigned char valor = b->bloco.data[i + 2];
            
            if (valor > 0) {
                printf("  %d → %d ($%d BTC)\n", origem, destino, valor);
            } else if (origem == 0 && destino == 0) {
                break;
            }
        }
    }
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

//////// DEBUGGGG

void exibirHistogramaHash() {
    // Array para contar quantos slots têm tamanho 0, 1, 2... até 19+
    int distribuicao[20] = {0}; 
    int maxComprimento = 0;
    int totalSlotsOcupados = 0;
    
    // Varre toda a Hash Table
    for (int i = 0; i < TAM_HASH; i++) {
        int contador = 0;
        NoHash *atual = tabelaNonce[i];
        
        // Conta elementos na lista encadeada deste slot
        while (atual != NULL) {
            contador++;
            atual = atual->prox;
        }

        // Estatísticas gerais
        if (contador > 0) totalSlotsOcupados++;
        if (contador > maxComprimento) maxComprimento = contador;

        // Atualiza a distribuição (limita a 19 para não estourar array)
        if (contador >= 19) distribuicao[19]++;
        else distribuicao[contador]++;
    }

    // Encontra a maior frequência para escalar o gráfico
    int maxFreq = 0;
    for (int i = 0; i < 20; i++) {
        if (distribuicao[i] > maxFreq) maxFreq = distribuicao[i];
    }

    // Exibe o Relatório
    printf("\n=== ANÁLISE DE PERFORMANCE DA HASH TABLE ===\n");
    printf("Tamanho da Tabela: %d slots\n", TAM_HASH);
    printf("Total de Blocos:   %u\n", stats.totalBlocos);
    printf("Ocupação:          %d slots (%.1f%%)\n", totalSlotsOcupados, (float)totalSlotsOcupados/TAM_HASH*100);
    printf("Maior colisão:     %d elementos numa lista\n\n", maxComprimento);

    printf("Tam. Lista | Qtd. Slots | Distribuição\n");
    printf("-----------+------------+--------------------------------------------------\n");
    
    for (int i = 0; i <= (maxComprimento < 19 ? maxComprimento : 19); i++) {
        printf("%9d  | %10d | ", i, distribuicao[i]);
        
        // Calcula tamanho da barra (max 50 caracteres)
        int barra = 0;
        if (maxFreq > 0) {
            barra = (distribuicao[i] * 50) / maxFreq;
        }

        // Desenha a barra
        for (int j = 0; j < barra; j++) printf("█");
        // Se for maior que 0 mas a barra for 0 (muito pequeno), põe um ponto
        if (distribuicao[i] > 0 && barra == 0) printf("."); 
        
        printf("\n");
    }
    printf("-----------+------------+--------------------------------------------------\n");
    printf("Legenda: 'Tam. Lista' é a quantidade de blocos que caíram no mesmo slot.\n");
    printf("         '0' indica slots vazios (desperdício de memória).\n");
}