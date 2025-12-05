#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include "storage.h"
#include "structs.h"

#define HASH_BITS 19 // Número de bits para tabela hash
#define TAM_HASH (1 << HASH_BITS) // TAM_HASH: Potência de 2^14 = 16384 e 2^19 = 524288
#define SHIFT_AMOUNT (32 - HASH_BITS) // Para espalhar bits
#define KNUTH_CONST 2654435761u

// Tabela hash para nonces testados
NoHash *tabelaNonce[TAM_HASH];  
// Ponteiros para os inícios das listas de blocos minerados por cada minerador
NoMinerador *indiceMinerador[256]; 
// Ponteiro para o início da lista da quantidade de transações por bloco minerado
NoBucket *bucketTransacoes[62]; 
// static = variável global visível apenas neste arquivo
FILE *arquivoAtual = NULL;
// Buffer de blocos antes de gravar no disco
BlocoMinerado buffer[16];
// Estrutura para estatísticas
Estatisticas stats;
int contadorBuffer = 0;

// Função Hash com constante de Knuth para Nonce
static unsigned int hashFunction(unsigned int nonce) 
{
    unsigned int hash = nonce * KNUTH_CONST;
    return hash >> SHIFT_AMOUNT; // Retorna os bits mais significativos
}

// Insere no índice de Nonce (RAM)
static void inserirNonce(unsigned int nonce, unsigned int idBloco) 
{
    unsigned int pos = hashFunction(nonce);
    NoHash *novo = (NoHash*) malloc(sizeof(NoHash));
    novo->nonce = nonce;
    novo->idBloco = idBloco;
    novo->prox = tabelaNonce[pos]; // Insere no início
    tabelaNonce[pos] = novo;
}

// Insere no índice de Minerador (RAM)
static void inserirMinerador(unsigned char endereco, unsigned int idBloco) 
{
    NoMinerador *novo = (NoMinerador*) malloc(sizeof(NoMinerador));
    novo->idBloco = idBloco;
    novo->prox = indiceMinerador[endereco]; // Insere no início 
    indiceMinerador[endereco] = novo;
}

// Esvazia o buffer para o disco 
static void flushBuffer() {
    if (contadorBuffer > 0 && arquivoAtual != NULL) 
    {
        // Vai para o final do arquivo para garantir escrita correta
        fseek(arquivoAtual, 0, SEEK_END); 
        fwrite(buffer, sizeof(BlocoMinerado), contadorBuffer, arquivoAtual);
        contadorBuffer = 0;
        // fflush garante que o SO escreva fisicamente no disco agora
        fflush(arquivoAtual); 
    }
}

// Limpa memória (inicialização ou finalização)
static void resetarIndices() 
{
    // Limpa Hash Table
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
    // Limpa Índice Mineradores
    for (int i = 0; i < 256; i++) 
    {
        NoMinerador *atual = indiceMinerador[i];
        while (atual != NULL) 
        {
            NoMinerador *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        indiceMinerador[i] = NULL;
    }
    stats.totalBlocos = 0;
    contadorBuffer = 0;
}

// Reconstrói índices lendo o arquivo existente 
static void reconstruirIndicesDoDisco() {
    BlocoMinerado blocoTemp;
    unsigned int idCalculado = 1;

    rewind(arquivoAtual); // Volta ao início do arquivo

    // Leitura sequencial rápida para povoar a RAM
    while(fread(&blocoTemp, sizeof(BlocoMinerado), 1, arquivoAtual) == 1) {
        // Recupera o minerador (último byte do data)
        unsigned char minerador = blocoTemp.bloco.data[183];
        
        // Reconstrói índices
        inserirNonce(blocoTemp.bloco.nonce, idCalculado);
        inserirMinerador(minerador, idCalculado);
        
        stats.totalBlocos = idCalculado;
        idCalculado++;
    }
    printf("Sistema restaurado. %u blocos indexados na RAM.\n", stats.totalBlocos);
}

void inicializarStorage(const char *nomeArquivo) 
{
    // Tenta abrir para leitura e escrita binária
    arquivoAtual = fopen(nomeArquivo, "rb+");

    if (arquivoAtual == NULL) 
    {
        // Se não existe, cria novo
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
        // Se existe, carrega para RAM [cite: 38]
        resetarIndices();
        reconstruirIndicesDoDisco();
    }
}

void adicionarBloco(BlocoMinerado *bloco) 
{
    // 1. Atualiza índices em RAM (Imediato)
    stats.totalBlocos++; // O próximo ID
    unsigned char minerador = bloco->bloco.data[183];
    
    inserirNonce(bloco->bloco.nonce, stats.totalBlocos);
    inserirMinerador(minerador, stats.totalBlocos);

    // 2. Adiciona ao Buffer
    buffer[contadorBuffer] = *bloco;
    contadorBuffer++;

    // 3. Se encheu o lote de 16, salva no disco 
    if (contadorBuffer == 16) 
    {
        flushBuffer();
    }
}

void finalizarStorage() 
{
    // Salva o que sobrou no buffer antes de sair
    flushBuffer();
    
    if (arquivoAtual) fclose(arquivoAtual);
    resetarIndices(); // Libera RAM
}

// Busca O(1) usando o índice Hash + Acesso direto ao Disco
int buscarNonce(unsigned int nonce, BlocoMinerado *saida) 
{
    unsigned int pos = hashFunction(nonce);
    NoHash *atual = tabelaNonce[pos];

    // 1. Busca na RAM (Hash Table)
    while (atual != NULL) 
    {
        if (atual->nonce == nonce) 
        {
            // ACHOU! Agora vamos buscar o corpo do bloco no disco.
            
            // Cálculo do Offset: (ID - 1) * Tamanho do Bloco
            long offset = (long)(atual->idBloco - 1) * sizeof(BlocoMinerado);
            
            fseek(arquivoAtual, offset, SEEK_SET); // Pula direto para o bloco
            fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual);
            
            return 1; // Encontrado
        }
        atual = atual->prox;
    }
    return 0; // Não encontrado
}

// Lista blocos de um minerador usando índice
void listarBlocosMinerador(unsigned char endereco) 
{
    NoMinerador *atual = indiceMinerador[endereco];
    BlocoMinerado temp;

    printf("--- Blocos do Minerador %d ---\n", endereco);
    
    while (atual != NULL) 
    {
        // Pula no disco para pegar o bloco
        long offset = (long)(atual->idBloco - 1) * sizeof(BlocoMinerado);
        fseek(arquivoAtual, offset, SEEK_SET);
        fread(&temp, sizeof(BlocoMinerado), 1, arquivoAtual);

        printf("Bloco %u | Nonce: %u\n", temp.bloco.numero, temp.bloco.nonce);
        
        atual = atual->prox;
    }
}

// Relatório dos N primeiros blocos ordenados por qtd transações 
// Le N blocos e usa Bucket Sort (pois transações são 0-61)
void relatorioTransacoes(unsigned int n) 
{
    if (n > stats.totalBlocos) n = stats.totalBlocos;
    if (n == 0) return;

    // Vetor de listas para Bucket Sort (transações variam de 0 a 61)
    // bucket[k] guardará os blocos que tem K transações
    typedef struct NoBucket 
    {
        unsigned int idBloco;
        unsigned int nTransacoes;
        struct NoBucket *prox;
    } NoBucket;
    
    NoBucket* buckets[62]; 
    for(int i=0; i<62; i++) buckets[i] = NULL;

    // Ler N blocos do disco sequencialmente
    rewind(arquivoAtual);
    BlocoMinerado temp;
    
    for (unsigned int i = 0; i < n; i++) 
    {
        fread(&temp, sizeof(BlocoMinerado), 1, arquivoAtual);
        
        // Conta transações (lógica simplificada: contar não-zeros no data)
        // Implementar a contagem correta baseada na sua lógica de transação
        int qtdTransacoes = 0; 
        // ... lógica de contagem aqui ...
        // Supondo qtdTransacoes calculada:
        
        // Insere no bucket correspondente
        if (qtdTransacoes > 61) qtdTransacoes = 61; // Segurança
        
        NoBucket *novo = (NoBucket*)malloc(sizeof(NoBucket));
        novo->idBloco = temp.bloco.numero;
        novo->nTransacoes = qtdTransacoes;
        novo->prox = buckets[qtdTransacoes];
        buckets[qtdTransacoes] = novo;
    }

    // Imprimir em ordem crescente (0 a 61)
    printf("--- Relatorio (Ordenado por Transacoes) ---\n");
    for (int i = 0; i < 62; i++) 
    {
        NoBucket *atual = buckets[i];
        while(atual != NULL) 
        {
            // Imprime o resumo:
            printf("Bloco %u: %u transacoes\n", atual->idBloco, atual->nTransacoes);
            
            // Limpeza
            NoBucket *lixo = atual;
            atual = atual->prox;
            free(lixo);
        }
    }
}

void exibirEstatisticas() 
{
    printf("Total de Blocos Minerados: %u\n", stats.totalBlocos);
    printf("## Riqueza e Saldo\n");
    printf("* Endereço com Maior Saldo: %u (Endereço é o índice do vetor saldos)\n", stats.enderecoMaisRico);
    printf("* Saldo Máximo Encontrado: %u bitcoins\n", stats.saldoMaisRico);
    printf("## Transações por Bloco\n");
    printf("* Máximo de Transações em um Bloco: %u transações\n", stats.maxTransacoes);
    printf("* Bloco com o Máximo de Transações: %u\n", stats.blocoMaxTransacoes);
}

/////// FUNCAO DE TESTE
void relatorioColisoes() {
    printf("\n=== DIAGNÓSTICO DE COLISÕES (HASH TABLE) ===\n");
    printf("Tamanho da Tabela: %d slots\n", TAM_HASH);
    printf("Total de Itens Inseridos: %u\n", stats.totalBlocos);

    unsigned int slotsVazios = 0;
    unsigned int maiorLista = 0;
    unsigned int somaTamanhos = 0;
    
    // Histograma para ver a distribuição
    // [0] = vazios, [1] = 1 item, ... [19] = 19 itens, [20] = 20 ou mais
    unsigned int histograma[21] = {0}; 

    for (int i = 0; i < TAM_HASH; i++) {
        unsigned int contador = 0;
        NoHash *atual = tabelaNonce[i];
        
        // Conta quantos itens tem nesta lista encadeada (profundidade)
        while (atual != NULL) {
            contador++;
            atual = atual->prox;
        }

        somaTamanhos += contador;
        
        if (contador == 0) slotsVazios++;
        if (contador > maiorLista) maiorLista = contador;

        // Preenche histograma
        if (contador >= 20) histograma[20]++;
        else histograma[contador]++;
    }

    double media = (double)somaTamanhos / (TAM_HASH - slotsVazios);
    double fatorCarga = (double)stats.totalBlocos / TAM_HASH;

    printf("Slots Vazios: %u (%.2f%%)\n", slotsVazios, (slotsVazios * 100.0 / TAM_HASH));
    printf("Fator de Carga (Ideal): %.2f itens por slot\n", fatorCarga);
    printf("Média Real (em slots ocupados): %.2f itens por slot\n", media);
    printf("PIOR CASO (Maior Lista): %u itens (Busca mais lenta)\n", maiorLista);
    
    printf("\n--- Distribuição (Histograma) ---\n");
    for(int i = 0; i <= 20; i++) {
        if(i == 20) printf("%2d+ itens: %u slots\n", i, histograma[i]);
        else        printf("%2d  itens: %u slots\n", i, histograma[i]);
    }
    printf("===========================================\n");
}