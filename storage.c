/*
 * SISTEMA DE ARMAZENAMENTO DA BLOCKCHAIN
 * 
 * Sistema híbrido: Disco (persistência) + RAM (índices rápidos)
 * - Hash Table de Nonces: busca O(1)
 * - Índice de Mineradores: listagem cronológica O(1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

// CONSTANTES DE CONFIGURAÇÃO

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

// ESTRUTURAS GLOBAIS (static = visível apenas neste arquivo)

// Índice 1: Hash Table para busca rápida por nonce
static NoHash *tabelaNonce[TAM_HASH];

// Índice 2: Listas de blocos por minerador (ordem cronológica)
static NoMinerador *indiceMinerador[NUM_ENDERECOS]; 
static NoMinerador *fimMinerador[NUM_ENDERECOS];

// Arquivo de disco e buffer de escrita
static FILE *arquivoAtual = NULL;
static BlocoMinerado buffer[BUFFER_SIZE];
static int contadorBuffer = 0;

// Estatísticas do sistema
static Estatisticas stats;

// FUNÇÕES AUXILIARES - HASH E ÍNDICES

// Hash multiplicativo de Knuth - distribuição uniforme
static unsigned int hashFunction(unsigned int nonce) 
{
    unsigned int hash = nonce * KNUTH_CONST;
    return hash >> SHIFT_AMOUNT;
}

// Insere nonce no índice (início da lista para O(1))
static void inserirNonce(unsigned int nonce, unsigned int idBloco) 
{
    unsigned int pos = hashFunction(nonce);
    NoHash *novo = verifica_malloc(sizeof(NoHash), "inserirNonce");

    novo->nonce = nonce;
    novo->idBloco = idBloco;
    novo->prox = tabelaNonce[pos];
    tabelaNonce[pos] = novo;
}

// Insere bloco no índice de minerador (fim da lista para manter ordem)
static void inserirMinerador(unsigned char endereco, unsigned int idBloco) 
{
    NoMinerador *novo = verifica_malloc(sizeof(NoMinerador), "inserirMinerador");
    novo->idBloco = idBloco;
    novo->prox = NULL;

    if (indiceMinerador[endereco] == NULL)
        indiceMinerador[endereco] = novo;
    else 
        fimMinerador[endereco]->prox = novo;
    
    fimMinerador[endereco] = novo;
}

// FUNÇÕES AUXILIARES - BUFFER E DISCO

static void flushBuffer() // Grava buffer no disco (reduz syscalls escrevendo 16 blocos de uma vez)
{
    if (contadorBuffer > 0 && arquivoAtual != NULL) 
    {
        fseek(arquivoAtual, 0, SEEK_END);
        fwrite(buffer, sizeof(BlocoMinerado), contadorBuffer, arquivoAtual);
        fflush(arquivoAtual);
        contadorBuffer = 0;
    }
}

// FUNÇÕES AUXILIARES - LIMPEZA DE MEMÓRIA

static void resetarIndices() // Libera toda memória alocada (hash table + índice de mineradores)
{
    // Libera hash table
    for (int i = 0; i < TAM_HASH; i++) 
    {
        NoHash *atual = tabelaNonce[i];
        while (atual != NULL) 
        {
            NoHash *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        tabelaNonce[i] = NULL;
    }
    
    // Libera índice de mineradores
    for (int i = 0; i < NUM_ENDERECOS; i++) 
    {
        NoMinerador *atual = indiceMinerador[i];
        while (atual != NULL) 
        {
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

// FUNÇÕES AUXILIARES - RECONSTRUÇÃO

static void reconstruirIndicesDoDisco()  // Reconstrói índices lendo blocos do disco (restaura estado após reabrir arquivo) 
{
    BlocoMinerado lote[READ_LOTE];
    size_t blocosLidos;
    unsigned int idCalculado = 1;

    rewind(arquivoAtual);

    // Lê blocos em lotes de 256 para otimizar I/O
    while ((blocosLidos = fread(lote, sizeof(BlocoMinerado), READ_LOTE, arquivoAtual)) > 0) 
    {
        for (size_t i = 0; i < blocosLidos; i++)
        {
            unsigned char minerador = lote[i].bloco.data[MINERADOR_OFFSET];

            inserirNonce(lote[i].bloco.nonce, idCalculado);
            inserirMinerador(minerador, idCalculado);
            
            stats.totalBlocos = idCalculado;
            idCalculado++;
        }
    }
    printf("Sistema restaurado. %u blocos indexados na RAM.\n", stats.totalBlocos);
}

// FUNÇÕES AUXILIARES - ANÁLISE DE BLOCOS

static int contarTransacoes(BlocoMinerado *bloco) // Conta transações em um bloco (0-61 possíveis)
{
    if (bloco->bloco.numero == 1) return 0; // Bloco Gênesis

    int contagem = 0;
    for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) 
    {
        unsigned char origem = bloco->bloco.data[i];
        unsigned char destino = bloco->bloco.data[i+1];
        unsigned char valor = bloco->bloco.data[i+2];

        if (valor > 0) 
            contagem++;
        else if (origem == 0 && destino == 0) 
            break;
    }
    return contagem;
}


// FUNÇÕES AUXILIARES PRIVADAS - LEITURA DE BLOCOS

void *verifica_malloc(size_t tamanho, const char *contexto) // Wrapper seguro para malloc - termina programa se falhar
{
    void *ptr = malloc(tamanho);
    if (ptr == NULL) 
    {
        fprintf(stderr, "ERRO FATAL: malloc falhou (%zu bytes em %s)\n", tamanho, contexto);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

// FUNÇÕES PÚBLICAS - INICIALIZAÇÃO E FINALIZAÇÃO

void inicializarStorage(const char *nomeArquivo) // Inicializa sistema (abre/cria arquivo e reconstrói índices)
{
    arquivoAtual = fopen(nomeArquivo, "rb+");

    if (arquivoAtual == NULL) 
    {
        arquivoAtual = fopen(nomeArquivo, "wb+");
        if (!arquivoAtual) 
        {
            perror("Erro fatal ao criar blockchain!");
            exit(1);
        }
        resetarIndices();
    } 
    else 
    {
        resetarIndices();
        reconstruirIndicesDoDisco();
    }
}

void adicionarBloco(BlocoMinerado *bloco) // Adiciona bloco ao sistema (atualiza índices e buffer)
{
    stats.totalBlocos++;
    unsigned char minerador = bloco->bloco.data[MINERADOR_OFFSET];
    
    inserirNonce(bloco->bloco.nonce, stats.totalBlocos);
    inserirMinerador(minerador, stats.totalBlocos);

    buffer[contadorBuffer] = *bloco;
    contadorBuffer++;

    if (contadorBuffer == BUFFER_SIZE) 
        flushBuffer();
}

void finalizarStorage() // Finaliza sistema (salva buffer, fecha arquivo, libera RAM)
{
    flushBuffer();

    if (arquivoAtual) fclose(arquivoAtual);
        resetarIndices();
}

// FUNÇÕES PÚBLICAS - VISUALIZAÇÃO

void imprimirBlocoCompleto(BlocoMinerado *b) // Imprime bloco formatado com metadados, hashes e transações
{
    printf("==================================================\n");
    printf("BLOCO #%u\n", b->bloco.numero);
    printf("==================================================\n");
    
    printf("Nonce:         %u\n", b->bloco.nonce);
    printf("Minerador:     %u (Endereço)\n", b->bloco.data[MINERADOR_OFFSET]); 
    
    printf("Hash:          ");
    for(int i=0; i<SHA256_DIGEST_LENGTH; i++) 
        printf("%02x", b->hash[i]);
    printf("\n");

    printf("Hash Anterior: ");
    for(int i=0; i<SHA256_DIGEST_LENGTH; i++) 
        printf("%02x", b->bloco.hashAnterior[i]);
    printf("\n");

    printf("--------------------------------------------------\n");
    printf("TRANSAÇÕES (Interpretadas do vetor data):\n");
    
    if (b->bloco.numero == 1)
    {
        printf("  [Dados]: The Times 03/Jan/2009 Chancellor on brink...\n");
    }
    else 
    {
        int qtd = 0;
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) 
        {
            unsigned char origem  = b->bloco.data[i];
            unsigned char destino = b->bloco.data[i+1];
            unsigned char valor   = b->bloco.data[i+2];

            if (origem != 0 || destino != 0 || valor != 0) 
            {
                printf("  Tx %d: Origem %3u -> Destino %3u | Valor: %3u BTC\n", 
                       qtd+1, origem, destino, valor);
                qtd++;
            }
        }
        
        if (qtd == 0) 
            printf("  (Nenhuma transação neste bloco)\n");
        
        printf("Total Transações: %d\n", qtd);
    }
    printf("==================================================\n");
}

// FUNÇÕES PÚBLICAS - BUSCA E ACESSO

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida) // Lê bloco por ID (verifica buffer E disco)
{
    if (id < 1 || id > stats.totalBlocos) return 0;

    unsigned int blocosPersistidos = stats.totalBlocos - contadorBuffer;

    // Bloco no buffer (ainda não gravado)
    if (id > blocosPersistidos) 
    {
        unsigned int idx = id - blocosPersistidos - 1;
        if (idx < (unsigned int)contadorBuffer) 
        {
            *saida = buffer[idx];
            return 1;
        }
        return 0;
    }

    // Bloco no disco - lê por offset
    long offset = (long)(id - 1) * sizeof(BlocoMinerado);

    if (arquivoAtual == NULL) 
        return 0;
    if (fseek(arquivoAtual, offset, SEEK_SET) != 0) 
        return 0;
    if (fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual) != 1) 
        return 0;
    return 1;
}

int buscarNonce(unsigned int nonce, BlocoMinerado *saida) // Busca bloco pelo nonce usando hash table
{
    unsigned int pos = hashFunction(nonce);
    NoHash *atual = tabelaNonce[pos];

    while (atual != NULL) 
    {
        if (atual->nonce == nonce) 
        {
            if (lerBlocoPorId(atual->idBloco, saida))
                return 1;
            return 0;
        }
        atual = atual->prox;
    }
    return 0;
}

void listarBlocosMinerador(unsigned char endereco, int n) // Lista blocos de um minerador em ordem cronológica
{
    NoMinerador *atual = indiceMinerador[endereco];
    
    if (atual == NULL) 
    {
        printf("Nenhum bloco encontrado para o minerador %d.\n", endereco);
        return;
    }

    printf("--- Primeiros %d Blocos do Minerador %d ---\n", n, endereco);
    
    int contagem = 0;
    while (atual != NULL && contagem < n) 
    {
        BlocoMinerado temp;
        if (!lerBlocoPorId(atual->idBloco, &temp))
        {
            printf("Erro ao ler bloco %u\n", atual->idBloco);
            return;
        }

        imprimirBlocoCompleto(&temp);
        contagem++;
        atual = atual->prox;
    }
}

void imprimirBlocoPorNumero(unsigned int numero)  // Imprime bloco por número (wrapper de lerBlocoPorId + imprimirBlocoCompleto)
{
    if (numero < 1 || numero > stats.totalBlocos) 
    {
        printf("Erro: Bloco %u nao existe. (Total: %u)\n", numero, stats.totalBlocos);
        return;
    }
    
    BlocoMinerado temp;
    if (lerBlocoPorId(numero, &temp)) 
        imprimirBlocoCompleto(&temp);
    else 
        printf("Erro de leitura.\n");
}

// FUNÇÕES PÚBLICAS - RELATÓRIOS

static BlocoMinerado* carregarBlocos(unsigned int n) // Carrega N blocos do disco/buffer para array
{
    BlocoMinerado* blocos = verifica_malloc(n * sizeof(BlocoMinerado), "relatorio_blocos");
    
    for (unsigned int i = 0; i < n; i++) 
    {
        if (!lerBlocoPorId(i + 1, &blocos[i])) 
        {
            fprintf(stderr, "Erro ao ler bloco %u\n", i + 1);
            free(blocos);
            return NULL;
        }
    }
    return blocos;
}

static void organizarBuckets(BlocoMinerado *blocos, unsigned int n, int *next, int *buckets) // Organiza blocos em buckets por quantidade de transações (bucket sort)
{
    if (blocos == NULL || next == NULL || buckets == NULL || n == 0) 
    {
        fprintf(stderr, "Erro: parâmetros inválidos em organizarBuckets\n");
        return;
    }
    // Inicializa buckets vazios
    for (int i = 0; i < NUM_BUCKETS; i++) 
        buckets[i] = -1;
    
    // Insere blocos nos buckets (de trás pra frente para manter ordem)
    for (int i = (int)n - 1; i >= 0; i--) 
    {
        int qtd = contarTransacoes(&blocos[i]);
        if (qtd > MAX_TRANSACOES) qtd = MAX_TRANSACOES;
        
        next[i] = buckets[qtd];
        buckets[qtd] = i;
    }
}

static void imprimirRelatorioBuckets(BlocoMinerado *blocos, unsigned int n, int *next, int *buckets) // Imprime relatório de blocos ordenados por transações
{
    printf("--- Relatorio (Ordenado por Transacoes) ---\n");
    printf("Total analisado: %u blocos\n\n", n);
    
    unsigned int totalImpresso = 0;
    
    for (int trans = 0; trans < NUM_BUCKETS; trans++) 
    {
        if (buckets[trans] == -1) continue;
        
        printf("\n=== Blocos com %d transacao(oes) ===\n", trans);
        
        for (int idx = buckets[trans]; idx != -1; idx = next[idx]) 
        {
            imprimirBlocoCompleto(&blocos[idx]);
            totalImpresso++;
        }
    }
    
    printf("\n--- Fim do Relatorio ---\n");
    printf("Blocos impressos: %u\n", totalImpresso);
}

void relatorioTransacoes(unsigned int n) // Gera relatório de blocos ordenados por quantidade de transações
{
    if (n > stats.totalBlocos) n = stats.totalBlocos;
    if (n == 0) return;
    
    // Carrega blocos do disco/buffer
    BlocoMinerado* blocos = carregarBlocos(n);
    if (blocos == NULL) return;
    
    // Prepara estruturas para bucket sort
    int *next = verifica_malloc(n * sizeof(int), "relatorio_next");
    int buckets[NUM_BUCKETS];
    
    // Organiza blocos em buckets
    organizarBuckets(blocos, n, next, buckets);
    
    // Imprime relatório ordenado
    imprimirRelatorioBuckets(blocos, n, next, buckets);
    
    // Limpa
    free(next);
    free(blocos);
}

void exibirEstatisticas() // Exibe estatísticas gerais do sistema
{
    printf("Total de Blocos Minerados: %u\n", stats.totalBlocos);
    printf("## Riqueza e Saldo\n");
    printf("* Endereço com Maior Saldo: %u\n", stats.enderecoMaisRico);
    printf("* Saldo Máximo Encontrado: %u bitcoins\n", stats.saldoMaisRico);
    printf("## Transações por Bloco\n");
    printf("* Máximo de Transações em um Bloco: %u\n", stats.maxTransacoes);
    printf("* Bloco com Máximo de Transações: %u\n", stats.blocoMaxTransacoes);
}

// ============================================================
// FUNÇÕES DE DEBUG
// ============================================================

// Analisa distribuição da hash table (colisões, fator de carga, etc)
void relatorioColisoes() 
{
    printf("\n=== DIAGNÓSTICO DE COLISÕES (HASH TABLE) ===\n");
    printf("Tamanho da Tabela: %d slots\n", TAM_HASH);
    printf("Total de Itens Inseridos: %u\n", stats.totalBlocos);

    unsigned int slotsVazios = 0;
    unsigned int maiorLista = 0;
    unsigned int somaTamanhos = 0;
    unsigned int histograma[21] = {0};

    for (int i = 0; i < TAM_HASH; i++) 
    {
        unsigned int contador = 0;
        NoHash *atual = tabelaNonce[i];
        
        while (atual != NULL) 
        {
            contador++;
            atual = atual->prox;
        }

        somaTamanhos += contador;
        if (contador == 0) slotsVazios++;
        if (contador > maiorLista) maiorLista = contador;

        if (contador >= 20) 
            histograma[20]++;
        else 
            histograma[contador]++;
    }

    double media = (double)somaTamanhos / (TAM_HASH - slotsVazios);
    double fatorCarga = (double)stats.totalBlocos / TAM_HASH;

    printf("Slots Vazios: %u (%.2f%%)\n", 
           slotsVazios, (slotsVazios * 100.0 / TAM_HASH));
    printf("Fator de Carga: %.2f itens/slot\n", fatorCarga);
    printf("Média Real (slots ocupados): %.2f itens/slot\n", media);
    printf("PIOR CASO (Maior Lista): %u itens\n", maiorLista);
    
    printf("\n--- Distribuição (Histograma) ---\n");
    for(int i = 0; i <= 20; i++) 
    {
        if(i == 20) 
            printf("%2d+ itens: %u slots\n", i, histograma[i]);
        else        
            printf("%2d  itens: %u slots\n", i, histograma[i]);
    }
    printf("===========================================\n");
}