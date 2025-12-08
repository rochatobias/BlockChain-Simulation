/*
 * transactions.c - Geração e Atualização de Transações
 * 
 * OTIMIZAÇÃO 1: Lista de candidatos mantida incrementalmente
 * - Em vez de percorrer todos os 256 endereços a cada transação,
 *   mantemos uma lista atualizada de quem tem saldo > 0
 * - Reduz de O(256×N) para O(N) onde N = número de transações
 * 
 * OTIMIZAÇÃO 3: Usa getSaldo() do storage em vez de carteira local
 * - Elimina duplicação de estado entre main.c e storage.c
 * - Fonte única de verdade para saldos
 */

#include <stdio.h>
#include <string.h> 
#include "mtwister.h"
#include "transactions.h"
#include "structs.h"
#include "storage.h"

#define TOTAL_ENDERECOS 256
#define TAMANHO_DATA 184
#define RECOMPENSA_MINERACAO 50
#define MAX_TRANSACOES 61
#define POSICAO_MINERADOR 183 

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

/**
 * Conta transações válidas em um bloco.
 * Uma transação é válida se valor > 0.
 * Termina quando encontra sequência de zeros.
 */
int contarTransacoesNoBloco(unsigned char dataBlock[]) {
    int count = 0;
    
    for (int i = 0; i < POSICAO_MINERADOR; i += 3) {
        unsigned char valor = dataBlock[i + 2];
        
        if (valor > 0) {
            count++;
        } else {
            // Verifica se é fim real (origem e destino também são 0)
            unsigned char origem = dataBlock[i];
            unsigned char destino = dataBlock[i + 1];
            if (origem == 0 && destino == 0) break;
        }
    }
    return count;
}

/**
 * Função de DEBUG para imprimir conteúdo de um bloco.
 */
void imprimirBlocoDebug(unsigned char dataBlock[]) {
    int qtd = contarTransacoesNoBloco(dataBlock);
    printf("\n--- DEBUG DO BLOCO ---\n");
    printf("Minerador (pos %d): %d\n", POSICAO_MINERADOR, dataBlock[POSICAO_MINERADOR]);
    
    if (qtd == 0 && dataBlock[0] != 0) {
        printf("Mensagem do Bloco: %s\n", dataBlock);
    } else {
        printf("Quantidade de Transações: %d\n", qtd);
        for(int i = 0; i < qtd; i++) {
            int idx = i * 3;
            printf("  [%02d] Origem: %3d | Destino: %3d | Valor: %3d\n", 
                   i+1, dataBlock[idx], dataBlock[idx+1], dataBlock[idx+2]);
        }
    }
    printf("----------------------\n");
}

// ============================================================================
// GERAÇÃO DE DADOS DO BLOCO (OTIMIZADO)
// ============================================================================

/**
 * Gera dados do bloco: minerador + transações aleatórias
 * 
 * OTIMIZAÇÃO 1 IMPLEMENTADA:
 * - Mantém lista de candidatos que é atualizada incrementalmente
 * - Antes: Para cada transação, varria 256 endereços → O(61 × 256) = O(15.616)
 * - Depois: Inicializa lista uma vez, atualiza conforme necessário → O(256 + 61) = O(317)
 * - Ganho: ~49x menos iterações por bloco
 * 
 * OTIMIZAÇÃO 3 IMPLEMENTADA:
 * - Usa getSaldo() do storage para obter saldos reais
 * - Elimina necessidade de manter cópia da carteira em main.c
 */
int gerarDadosDoBloco(unsigned int numeroDoBloco, unsigned char dataBlock[], 
                       unsigned int carteiraOrigem[], MTRand *r) {
    
    // Limpa vetor de dados
    memset(dataBlock, 0, TAMANHO_DATA);

    // Define minerador aleatório
    unsigned char minerador = (unsigned char)(genRandLong(r) % 256);
    dataBlock[POSICAO_MINERADOR] = minerador;

    // Bloco Gênesis: apenas mensagem + minerador
    if (numeroDoBloco == 1) {
        const char *fraseGenesis = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        strcpy((char*)dataBlock, fraseGenesis);
        dataBlock[POSICAO_MINERADOR] = minerador; 
        return 0;
    }

    // --- OTIMIZAÇÃO 1: Inicialização única da lista de candidatos ---
    // Cria cópia local dos saldos para simulação das transações
    unsigned int saldoTemp[TOTAL_ENDERECOS];
    
    // OTIMIZAÇÃO 3: Obtém saldos do storage (fonte única de verdade)
    // Se carteiraOrigem for NULL, usa storage; senão usa a passada
    if (carteiraOrigem != NULL) {
        memcpy(saldoTemp, carteiraOrigem, sizeof(unsigned int) * TOTAL_ENDERECOS);
    } else {
        for (int i = 0; i < TOTAL_ENDERECOS; i++) {
            saldoTemp[i] = getSaldo((unsigned char)i);
        }
    }

    // Lista de candidatos: endereços com saldo > 0
    unsigned char candidatos[TOTAL_ENDERECOS];
    int totalCandidatos = 0;
    
    // OTIMIZAÇÃO 1: Inicializa lista UMA VEZ (não a cada transação)
    for (int k = 0; k < TOTAL_ENDERECOS; k++) {
        if (saldoTemp[k] > 0) {
            candidatos[totalCandidatos] = (unsigned char)k;
            totalCandidatos++;
        }
    }

    // Quantidade aleatória de transações (0 a 61)
    int qtdTransacoes = (int)(genRandLong(r) % (MAX_TRANSACOES + 1)); 
    int posicaoAtual = 0;
    int transacoesValidas = 0;

    for (int i = 0; i < qtdTransacoes; i++) {
        if (totalCandidatos == 0) break; // Ninguém mais tem saldo

        // Sorteia origem da lista de candidatos
        int indiceSorteado = (int)(genRandLong(r) % totalCandidatos);
        unsigned char origem = candidatos[indiceSorteado];
        
        // Destino pode ser qualquer endereço
        unsigned char destino = (unsigned char)(genRandLong(r) % 256); 

        // Valor: máximo é o saldo da origem, limitado a 50
        unsigned int maximoPossivel = saldoTemp[origem];
        if (maximoPossivel > 50) maximoPossivel = 50;

        unsigned char valor = (unsigned char)(genRandLong(r) % (maximoPossivel + 1));

        // Grava no vetor de dados
        dataBlock[posicaoAtual]     = origem;
        dataBlock[posicaoAtual + 1] = destino;
        dataBlock[posicaoAtual + 2] = valor;
        
        posicaoAtual += 3;
        transacoesValidas++;

        // Atualiza saldos temporários
        saldoTemp[origem] -= valor;
        saldoTemp[destino] += valor;

        // OTIMIZAÇÃO 1: Atualiza lista de candidatos incrementalmente
        // Se origem ficou sem saldo, remove da lista
        if (saldoTemp[origem] == 0) {
            // Remove trocando com o último elemento (O(1))
            candidatos[indiceSorteado] = candidatos[totalCandidatos - 1];
            totalCandidatos--;
        }
        
        // Se destino não estava na lista e agora tem saldo, adiciona
        // (Apenas se valor > 0 e destino era 0 antes)
        // Nota: Como simplificação, não adicionamos novos candidatos mid-loop
        // pois o destino só poderá ser origem em blocos futuros
    }
    
    return transacoesValidas;
}

// ============================================================================
// ATUALIZAÇÃO DA CARTEIRA (PÓS-MINERAÇÃO)
// ============================================================================

/**
 * Aplica as transações do bloco na carteira oficial.
 * Chamado após mineração bem-sucedida.
 */
void atualizarCarteira(unsigned int numeroDoBloco, unsigned char dataBlock[], 
                        unsigned int carteiraOficial[], int qtdTransacoes) {
    
    // 1. Recompensa do Minerador (+50 BTC)
    unsigned char minerador = dataBlock[POSICAO_MINERADOR];
    carteiraOficial[minerador] += RECOMPENSA_MINERACAO;

    // Gênesis não tem transações
    if (numeroDoBloco == 1) return;

    // Se não sabemos a quantidade, contamos
    if (qtdTransacoes == -1) {
        qtdTransacoes = contarTransacoesNoBloco(dataBlock);
    }

    // 2. Efetiva as transações
    int index = 0;
    for (int i = 0; i < qtdTransacoes; i++) {
        unsigned char origem = dataBlock[index];
        unsigned char destino = dataBlock[index + 1];
        unsigned char valor = dataBlock[index + 2];
        
        if (carteiraOficial[origem] >= valor) {
            carteiraOficial[origem] -= valor;
            carteiraOficial[destino] += valor;
        } else {
            printf("[ERRO] Bloco %u: Origem %d tem %u, tentou gastar %d.\n", 
                   numeroDoBloco, origem, carteiraOficial[origem], valor);
        }
        
        index += 3;
    }
}
