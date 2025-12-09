/*
 * 
 * TRADE-OFFS DE DESEMPENHO/MEMÓRIA:
 * 
 * Hash Table (2^14 slots): ~64KB de ponteiros
 *    - Pro: Busca O(1) por nonce em média
 *    - Contra: Memória fixa mesmo se poucos nonces únicos
 * 
 * Índice Mineradores: Array de 256 listas
 *    - Pro: Acesso direto O(1) por endereço
 *    - Contra: 256 ponteiros sempre alocados 
 * 
 * Listas de Recordes: Dinâmicas para MAX/MIN transações
 *    - Pro: Sem limite de empates, libera automaticamente
 *    - Contra: "Overhead" de alocação por nó
 * 
 * Buffer de escrita: 16 blocos
 *    - Pro: Reduz I/O em 16x 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

// CONSTANTES -> Uso de Static como "private" do arquivo

#define READ_LOTE 256       // Quantidade de blocos lidos por vez do disco
#define BUFFER_SIZE 16      // Blocos em memória antes de flush no disco

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

// Cache de contagem 
#define CACHE_INICIAL 1000      // Capacidade inicial do cache
#define CACHE_CRESCIMENTO 2     // Fator de crescimento quando cheio

// ESTRUTURAS AUXILIARES

// VARIÁVEIS GLOBAIS 

static NoHash *tabelaNonce[TAM_HASH];               // Hash table para busca por nonce O(1)
static NoMinerador *indiceMinerador[NUM_ENDERECOS]; // Início de cada lista de minerador
static NoMinerador *fimMinerador[NUM_ENDERECOS];    // Fim de cada lista (inserção O(1))

static unsigned int saldos[NUM_ENDERECOS];          // Saldo atual de cada carteira
static unsigned int blocosMinerados[NUM_ENDERECOS]; // Contador de blocos por minerador
static unsigned long long totalValorTransacionado = 0; // Soma de todos os valores (para média)

static unsigned int maiorSaldoAtual = 0;            // Cache do maior saldo
static unsigned int maiorQtdMinerada = 0;           // Cache da maior qtd minerada

static int maxTransacoesGlobal = -1;                // Recorde de MAX transações 
static NoRecorde *listaMaxTx = NULL;                // Lista de blocos com MAX transações
static int minTransacoesGlobal = 1000;              // Recorde de MIN transações 
static NoRecorde *listaMinTx = NULL;                // Lista de blocos com MIN transações

static FILE *arquivoAtual = NULL;
static BlocoMinerado buffer[BUFFER_SIZE];
static int contadorBuffer = 0;
static Estatisticas stats;

static unsigned char *cacheContagemTx = NULL;  
static unsigned int cacheTamanho = 0;           
static unsigned int cacheCapacidade = 0;        

// PROTÓTIPOS INTERNOS
static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida);
static void liberarListaRecorde(NoRecorde **lista);
static void adicionarRecorde(NoRecorde **lista, unsigned int idBloco);
static void expandirCacheTx();
static void adicionarAoCache(unsigned int idBloco, unsigned char qtdTx);
void imprimirBlocoCompleto(BlocoMinerado *b);

// FUNÇÕES AUXILIARES

// Libera todos os nós de uma lista de recordes
static void liberarListaRecorde(NoRecorde **lista) 
{
    NoRecorde *atual = *lista;

    while (atual != NULL) 
    {
        NoRecorde *temp = atual;
        atual = atual->prox;
        free(temp);
    }
    *lista = NULL;
}

// Adiciona um bloco à lista de recordes (inserção no início = O(1))
static void adicionarRecorde(NoRecorde **lista, unsigned int idBloco) 
{
    NoRecorde *novo = verifica_malloc(sizeof(NoRecorde), "adicionarRecorde");

    novo->idBloco = idBloco;
    novo->prox = *lista;
    *lista = novo;
}

// Expande o cache quando necessário
static void expandirCacheTx() 
{
    unsigned int novaCapacidade = cacheCapacidade == 0 ? CACHE_INICIAL : cacheCapacidade * CACHE_CRESCIMENTO;
    
    unsigned char *novoCache = realloc(cacheContagemTx, novaCapacidade * sizeof(unsigned char));

    if (!novoCache) 
    {
        fprintf(stderr, "Erro ao expandir cache de transações\n");
        exit(1);
    }
    
    // Inicializa novas posições com 0
    memset(novoCache + cacheCapacidade, 0, (novaCapacidade - cacheCapacidade) * sizeof(unsigned char));
    
    cacheContagemTx = novoCache;
    cacheCapacidade = novaCapacidade;
}

// Adiciona contagem no cache
static void adicionarAoCache(unsigned int idBloco, unsigned char qtdTx) 
{
    // Expande se necessário
    while (idBloco > cacheCapacidade)
        expandirCacheTx();
    
    cacheContagemTx[idBloco - 1] = qtdTx;
    if (idBloco > cacheTamanho) 
        cacheTamanho = idBloco;
}

// Pega contagem do cache
static unsigned char obterContagemDoCache(unsigned int idBloco) 
{
    if (idBloco == 0 || idBloco > cacheTamanho)
        return 0;
    return cacheContagemTx[idBloco - 1];
}

// CONTAGEM E ESTATÍSTICAS


// Atualiza todas as estatísticas quando um bloco entra no sistema
static void atualizarEstatisticasGlobais(BlocoMinerado *b)
{
    unsigned char minerador = b->bloco.data[MINERADOR_OFFSET];
    
    // Recompensa do minerador (+50 BTC)
    saldos[minerador] += 50;
    blocosMinerados[minerador]++;
    
    // Atualiza caches de máximos
    if (saldos[minerador] > maiorSaldoAtual) 
        maiorSaldoAtual = saldos[minerador];
    if (blocosMinerados[minerador] > maiorQtdMinerada) 
        maiorQtdMinerada = blocosMinerados[minerador];

    // Processa transações
    int txNoBloco = 0;
    if (b->bloco.numero > 1) 
    {
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) 
        {
            unsigned char origem = b->bloco.data[i];
            unsigned char destino = b->bloco.data[i + 1];
            unsigned char valor = b->bloco.data[i + 2];

            if (valor > 0) 
            {
                if (saldos[origem] >= valor) 
                {
                    saldos[origem] -= valor;
                    saldos[destino] += valor;
                    totalValorTransacionado += valor;
                    txNoBloco++;
                } 
                else 
                    fprintf(stderr, "AVISO: Tx inválida no bloco %u (origem %d tem %u, tentou %u)\n", b->bloco.numero, origem, saldos[origem], valor);
            } 
            else if (origem == 0 && destino == 0)
                break;
        }
    }
    
    // Armazena contagem no cache
    adicionarAoCache(b->bloco.numero, (unsigned char)txNoBloco);
    
    // Atualiza recordes de MAX transações
    if (txNoBloco > maxTransacoesGlobal) 
    {
        liberarListaRecorde(&listaMaxTx);
        maxTransacoesGlobal = txNoBloco;
        adicionarRecorde(&listaMaxTx, b->bloco.numero);
    } 
    else if (txNoBloco == maxTransacoesGlobal && maxTransacoesGlobal >= 0) 
        adicionarRecorde(&listaMaxTx, b->bloco.numero);
    
    // Atualiza recordes de MIN transações 
    if (b->bloco.numero > 1) 
    {
        if (txNoBloco < minTransacoesGlobal) 
        {
            liberarListaRecorde(&listaMinTx);
            minTransacoesGlobal = txNoBloco;
            adicionarRecorde(&listaMinTx, b->bloco.numero);
        } 
        else if (txNoBloco == minTransacoesGlobal) 
            adicionarRecorde(&listaMinTx, b->bloco.numero);
    }
}

// FUNÇÕES DE HASH TABLE E ÍNDICES

static unsigned int hashFunction(unsigned int nonce) 
{
    return (nonce * KNUTH_CONST) >> SHIFT_AMOUNT;
}

static void inserirNonce(unsigned int nonce, unsigned int idBloco) 
{
    unsigned int pos = hashFunction(nonce);
    NoHash *novo = verifica_malloc(sizeof(NoHash), "inserirNonce");
    novo->nonce = nonce;
    novo->idBloco = idBloco;
    novo->prox = tabelaNonce[pos];
    tabelaNonce[pos] = novo;
}

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


// FUNÇÕES DE ARQUIVO

static void flushBuffer() {
    if (contadorBuffer > 0 && arquivoAtual != NULL) 
    {
        fseek(arquivoAtual, 0, SEEK_END);
        fwrite(buffer, sizeof(BlocoMinerado), contadorBuffer, arquivoAtual);
        fflush(arquivoAtual);
        contadorBuffer = 0;
    }
}

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida) {

    if (id < 1 || id > stats.totalBlocos) 
        return 0;
    
    unsigned int blocosPersistidos = stats.totalBlocos - contadorBuffer;

    if (id > blocosPersistidos) 
    {
        *saida = buffer[id - blocosPersistidos - 1];
        return 1;
    }
    
    long offset = (long)(id - 1) * sizeof(BlocoMinerado);

    if (fseek(arquivoAtual, offset, SEEK_SET) != 0) 
        return 0;
    if (fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual) != 1) 
        return 0;

    return 1;
}

static void reconstruirIndicesDoDisco() 
{
    BlocoMinerado lote[READ_LOTE];
    size_t blocosLidos;
    unsigned int idCalculado = 1;

    rewind(arquivoAtual);

    while ((blocosLidos = fread(lote, sizeof(BlocoMinerado), READ_LOTE, arquivoAtual)) > 0) 
    {
        for (size_t i = 0; i < blocosLidos; i++) 
        {
            inserirNonce(lote[i].bloco.nonce, idCalculado);
            inserirMinerador(lote[i].bloco.data[MINERADOR_OFFSET], idCalculado);
            atualizarEstatisticasGlobais(&lote[i]);
            stats.totalBlocos = idCalculado;
            idCalculado++;
        }
    }
    printf("Sistema restaurado: %u blocos. Saldo máximo: %u BTC.\n", stats.totalBlocos, maiorSaldoAtual);
}

static void resetarIndices() 
{
    // Limpa Hash Table de Nonces
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
    
    // Limpa Índice de Mineradores
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
    
    // Limpa listas de recordes
    liberarListaRecorde(&listaMaxTx);
    liberarListaRecorde(&listaMinTx);
    
    // Limpa cache de contagem
    if (cacheContagemTx != NULL) 
    {
        free(cacheContagemTx);
        cacheContagemTx = NULL;
    }
    cacheTamanho = 0;
    cacheCapacidade = 0;
    
    // Zera financeiro
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

void exportarParaTexto(const char* nomeArquivoTxt) 
{
    printf("Gerando arquivo de texto (%s)... ", nomeArquivoTxt);
    
    FILE *arqBin = fopen("blockchain.bin", "rb"); // Abre o binário atual
    if (!arqBin) 
    {
        printf("Erro ao abrir binário para exportação.\n");
        return;
    }

    FILE *arqTxt = fopen(nomeArquivoTxt, "w");
    if (!arqTxt) 
    {
        printf("Erro ao criar arquivo de texto.\n");
        fclose(arqBin);
        return;
    }

    // Lê blocos em lotes de 100 para eficiência
    const int TAM_LOTE = 100;
    BlocoMinerado bufferLote[TAM_LOTE];
    size_t lidos;

    fprintf(arqTxt, "=== RELATÓRIO DA BLOCKCHAIN ===\n");
    fprintf(arqTxt, "Total de Blocos: %u\n\n", stats.totalBlocos);

    while ((lidos = fread(bufferLote, sizeof(BlocoMinerado), TAM_LOTE, arqBin)) > 0) 
    {
        for (size_t i = 0; i < lidos; i++) 
        {
            BlocoMinerado *b = &bufferLote[i];
            
            fprintf(arqTxt, "--------------------------------------------------\n");
            fprintf(arqTxt, "BLOCO %u\n", b->bloco.numero);
            fprintf(arqTxt, "Nonce: %u\n", b->bloco.nonce);
            fprintf(arqTxt, "Minerador: %u\n", b->bloco.data[183]); 
            
            fprintf(arqTxt, "Hash: ");
            for(int k=0; k<32; k++) fprintf(arqTxt, "%02x", b->hash[k]);
            fprintf(arqTxt, "\n");

            // Imprimir transações 
            if (b->bloco.numero == 1)
                fprintf(arqTxt, "Dados: %s\n", b->bloco.data);
            else 
            {
                fprintf(arqTxt, "Transações:\n");
                for (int k = 0; k < 183; k += 3) 
                {
                    unsigned char origem = b->bloco.data[k];
                    unsigned char destino = b->bloco.data[k+1];
                    unsigned char valor = b->bloco.data[k+2];
                    
                    if (valor > 0) 
                        fprintf(arqTxt, "   %d -> %d (%d BTC)\n", origem, destino, valor);
                    else if (origem == 0 && destino == 0) 
                        break; // Fim das transações
                }
            }
        }
    }

    fclose(arqTxt);
    fclose(arqBin);
    printf("Concluído!\n");
}

static void imprimirListaRecordes(NoRecorde *lista, const char *titulo, int valor) 
{
    printf("\n--- %s ---\n", titulo);
    printf("Quantidade de transações: %d\n", valor);
    
    int totalEmpates = 0;
    BlocoMinerado temp;
    
    for(NoRecorde *r = lista; r != NULL; r = r->prox) 
    {
        if (lerBlocoPorId(r->idBloco, &temp)) 
        {
            printf("   - Bloco %u | Hash: ", r->idBloco);
            for(int j = 0; j < 32; j++) printf("%02x", temp.hash[j]);
            printf("\n");
            totalEmpates++;
        }
    }

    if (totalEmpates > 1) 
        printf("Total de blocos empatados com esse valor: %d\n", totalEmpates);
}

// 
// FUNÇÕES PÚBLICAS

void *verifica_malloc(size_t tamanho, const char *contexto) 
{
    void *ptr = malloc(tamanho);
    if (!ptr) 
    { 
        fprintf(stderr, "Erro malloc: %s\n", contexto); 
        exit(1); 
    }
    return ptr;
}

void inicializarStorage(const char *nomeArquivo) 
{
    arquivoAtual = fopen(nomeArquivo, "rb+");
    if (arquivoAtual == NULL) 
    {
        arquivoAtual = fopen(nomeArquivo, "wb+");
        if (!arquivoAtual) 
        {
            perror("Erro ao abrir arquivo"); 
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

unsigned int getSaldo(unsigned char endereco) 
{
    return saldos[endereco];
}

void getUltimoHash(unsigned char *bufferHash) 
{
    if (stats.totalBlocos == 0) 
    {
        memset(bufferHash, 0, SHA256_DIGEST_LENGTH);
        return;
    }
    
    BlocoMinerado ultimo;
    if (lerBlocoPorId(stats.totalBlocos, &ultimo)) 
        memcpy(bufferHash, ultimo.hash, SHA256_DIGEST_LENGTH);
    else 
    {
        memset(bufferHash, 0, SHA256_DIGEST_LENGTH);
        fprintf(stderr, "ERRO: Falha ao ler último bloco\n");
    }
}

void adicionarBloco(BlocoMinerado *bloco) 
{
    stats.totalBlocos++;
    
    inserirNonce(bloco->bloco.nonce, stats.totalBlocos);
    inserirMinerador(bloco->bloco.data[MINERADOR_OFFSET], stats.totalBlocos);
    atualizarEstatisticasGlobais(bloco);

    buffer[contadorBuffer] = *bloco;
    contadorBuffer++;
    if (contadorBuffer == BUFFER_SIZE) 
        flushBuffer();
}

void finalizarStorage() 
{
    flushBuffer();

    // Fecha o arquivo binário ANTES de exportar para texto
    if (arquivoAtual) 
    {
        fclose(arquivoAtual);
        arquivoAtual = NULL;
    }

    // Agora exporta (abre o binário novamente para leitura)
    exportarParaTexto("blockchain.txt");
    
    resetarIndices();
}

unsigned int obterTotalBlocos() 
{
    return stats.totalBlocos;
}

int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida) 
{
    return lerBlocoPorId(id, saida);
}

// RELATÓRIOS ESTATÍSTICOS

void relatorioMaisRico() 
{
    
    unsigned int maxAtual = 0;
    for(int i = 0; i < NUM_ENDERECOS; i++) 
    {
        if(saldos[i] > maxAtual) 
            maxAtual = saldos[i];
    }

    printf("\n--- Endereço(s) com mais Bitcoins (Item A) ---\n");
    printf("Saldo Máximo: %u BTC\n", maxAtual);
    printf("Endereço(s): "); 
    
    int primeiro = 1;
    int encontrou = 0;
    
    for(int i = 0; i < NUM_ENDERECOS; i++) 
    {
        if(saldos[i] == maxAtual) 
        {
            if(!primeiro) printf(" | "); 
            printf("%d", i);
            primeiro = 0;
            encontrou = 1;
        }
    }
    
    if (!encontrou) printf("(Nenhum endereço com saldo > 0)");
        printf("\n");
}

void relatorioMaiorMinerador() 
{
    printf("\n--- Endereço(s) que mais minerou (Item B) ---\n");
    printf("Qtd Blocos: %u\n", maiorQtdMinerada);
    printf("Endereço(s): "); 
    
    int primeiro = 1;
    
    for(int i = 0; i < NUM_ENDERECOS; i++) 
    {
        if(blocosMinerados[i] == maiorQtdMinerada) 
        {
            if(!primeiro) printf(" | "); 
                printf("%d", i);
            primeiro = 0; 
        }
    }
    printf("\n");
}

void relatorioMaxTransacoes() 
{
    imprimirListaRecordes(listaMaxTx, "Bloco(s) com MAIS transações (Item C)", maxTransacoesGlobal);
}

void relatorioMinTransacoes() 
{
    imprimirListaRecordes(listaMinTx, "Bloco(s) com MENOS transações (Item D)", minTransacoesGlobal);
}

void calcularMediaBitcoinsPorBloco() 
{
    if (stats.totalBlocos == 0) 
    {
        printf("Blockchain vazia.\n");
        return;
    }
    double media = (double)totalValorTransacionado / stats.totalBlocos;
    printf("\n--- Média de Bitcoins por Bloco (Item E) ---\n");
    printf("Total transacionado: %llu BTC\n", totalValorTransacionado);
    printf("Total de blocos: %u\n", stats.totalBlocos);
    printf("Média: %.2f BTC/bloco\n", media);
}

// CONSULTAS 

void imprimirBlocoPorNumero(unsigned int numero) 
{
    BlocoMinerado temp;
    if (lerBlocoPorId(numero, &temp)) 
        imprimirBlocoCompleto(&temp);
    else
        printf("Bloco %u não encontrado.\n", numero);
}

void listarBlocosMinerador(unsigned char endereco, int n) 
{
    NoMinerador *atual = indiceMinerador[endereco];
    int count = 0;
    BlocoMinerado temp;
    
    printf("\n--- %d Primeiros Blocos do Minerador %d ---\n", n, endereco);
    
    while (atual != NULL && count < n) 
    {
        if (lerBlocoPorId(atual->idBloco, &temp)) 
            imprimirBlocoCompleto(&temp);
        atual = atual->prox;
        count++;
    }
    
    if (count == 0)
        printf("Minerador %d não possui blocos.\n", endereco);
}

void relatorioTransacoes(unsigned int n) 
{
    if (n > stats.totalBlocos) n = stats.totalBlocos;
    if (n == 0) return;

    // Carrega N blocos em memória
    BlocoMinerado *blocos = verifica_malloc(n * sizeof(BlocoMinerado), "relatorioTransacoes");

    for (unsigned int i = 0; i < n; i++) 
        lerBlocoPorId(i + 1, &blocos[i]);

    // Bucket Sort: 62 buckets (0 a 61 transações)
    int *next = verifica_malloc(n * sizeof(int), "next");
    int buckets[62];
    for(int i = 0; i < 62; i++) buckets[i] = -1;

    // Usa cache em vez de recontar
    for (int i = (int)n - 1; i >= 0; i--) 
    {
        // Consulta O(1) do cache em vez de O(61) de contarTransacoes
        int qtd = obterContagemDoCache(blocos[i].bloco.numero);
        if (qtd > 61) qtd = 61;
        next[i] = buckets[qtd];
        buckets[qtd] = i;
    }

    printf("\n--- Relatório Top %u Blocos (Ordenado por Transações) ---\n", n);
    
    for (int t = 0; t < 62; t++) 
    {
        if (buckets[t] == -1) 
            continue;

        printf("\n[ %d Transações ]\n", t);
        for (int idx = buckets[t]; idx != -1; idx = next[idx]) 
            imprimirBlocoCompleto(&blocos[idx]);
    }
    free(next);
    free(blocos);
}

int listarBlocosPorNonce(unsigned int nonce) 
{
    unsigned int pos = hashFunction(nonce);
    NoHash *atual = tabelaNonce[pos];
    int encontrados = 0;
    BlocoMinerado temp;

    printf("\n--- Buscando Blocos com Nonce %u ---\n", nonce);

    while (atual != NULL) 
    {
        if (atual->nonce == nonce) 
        {
            if (lerBlocoPorId(atual->idBloco, &temp)) 
            {
                imprimirBlocoCompleto(&temp);
                encontrados++;
            }
        }
        atual = atual->prox;
    }

    if (encontrados == 0) 
        printf("Nenhum bloco encontrado com o nonce %u.\n", nonce);
    else
        printf("Total de blocos encontrados: %d\n", encontrados);
    
    return encontrados;
}

// FUNÇÃO AUXILIAR DE IMPRESSÃO

void imprimirBlocoCompleto(BlocoMinerado *b) 
{
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("BLOCO %u | Minerador: %d | Nonce: %u\n", b->bloco.numero, b->bloco.data[MINERADOR_OFFSET], b->bloco.nonce);
    printf("Hash: ");
    for(int i = 0; i < 32; i++) 
        printf("%02x", b->hash[i]);
    printf("\n");
    
    // Mostra contagem do cache 
    printf("Transações: %d\n", obterContagemDoCache(b->bloco.numero));
    
    if (b->bloco.numero == 1) 
        printf("Dados (Gênesis): %s\n", b->bloco.data);
    else 
    {
        printf("Detalhes:\n");
        for (int i = 0; i < MINERADOR_OFFSET; i += TRANSACAO_SIZE) 
        {
            unsigned char origem = b->bloco.data[i];
            unsigned char destino = b->bloco.data[i + 1];
            unsigned char valor = b->bloco.data[i + 2];
            
            if (valor > 0) 
                printf("  %d → %d ($%d BTC)\n", origem, destino, valor); 
            else if (origem == 0 && destino == 0)
                break;
        }
    }
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

// HISTOGRAMA DA HASH TABLE

void exibirHistogramaHash() 
{
    // Array para contar quantos slots têm tamanho 0, 1, 2... até 19+
    int distribuicao[20] = {0}; 
    int maxComprimento = 0;
    int totalSlotsOcupados = 0;
    
    for (int i = 0; i < TAM_HASH; i++) 
    {
        int contador = 0;
        NoHash *atual = tabelaNonce[i];
        
        // Conta elementos na lista encadeada deste slot
        while (atual != NULL) 
        {
            contador++;
            atual = atual->prox;
        }

        // Estatísticas gerais
        if (contador > 0) 
            totalSlotsOcupados++;
        if (contador > maxComprimento) 
            maxComprimento = contador;

        // Atualiza a distribuição
        if (contador >= 19) 
            distribuicao[19]++;
        else 
            distribuicao[contador]++;
    }

    // Encontra a maior frequência para montar o gráfico
    int maxFreq = 0;
    for (int i = 0; i < 20; i++) 
    {
        if (distribuicao[i] > maxFreq) 
            maxFreq = distribuicao[i];
    }

    // Exibe o Relatório
    printf("\n=== ANÁLISE DE PERFORMANCE DA HASH TABLE ===\n");
    printf("Tamanho da Tabela: %d slots\n", TAM_HASH);
    printf("Total de Blocos:   %u\n", stats.totalBlocos);
    printf("Ocupação:          %d slots (%.1f%%)\n", totalSlotsOcupados, (float)totalSlotsOcupados/TAM_HASH*100);
    printf("Maior colisão:     %d elementos numa lista\n\n", maxComprimento);
    printf("Tam. Lista | Qtd. Slots | Distribuição\n");
    printf("-----------+------------+--------------------------------------------------\n");
    
    for (int i = 0; i <= (maxComprimento < 19 ? maxComprimento : 19); i++) 
    {
        printf("%9d  | %10d | ", i, distribuicao[i]);
        
        // Calcula tamanho da barra 
        int barra = 0;
        if (maxFreq > 0) 
            barra = (distribuicao[i] * 50) / maxFreq;

        // Desenha a barra
        for (int j = 0; j < barra; j++) 
            printf("█");
        // Se for maior que 0 mas a barra for muito pequena, põe um ponto
        if (distribuicao[i] > 0 && barra == 0) printf("."); 
            printf("\n");
    }
    printf("-----------+------------+--------------------------------------------------\n");
    printf("Legenda: 'Tam. Lista' é a quantidade de blocos que caíram no mesmo slot.\n");
    printf("         '0' indica slots vazios (desperdício de memória).\n");
}