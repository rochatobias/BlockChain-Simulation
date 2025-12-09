#include <stdio.h>
#include <string.h> 
#include "mtwister.h"
#include "transactions.h"
#include "structs.h"
#include "storage.h"

#define TOTAL_ENDERECOS 256
#define TAMANHO_DATA 184
#define MAX_TRANSACOES 61
#define POSICAO_MINERADOR 183 

// GERAÇÃO DE DADOS DO BLOCO 

/**
 * Gera dados do bloco: minerador + transações aleatórias
 */
int gerarDadosDoBloco(unsigned int numeroDoBloco, unsigned char dataBlock[], unsigned int carteiraOrigem[], MTRand *r) 
{
    
    // Limpa vetor de dados
    memset(dataBlock, 0, TAMANHO_DATA);

    // Define minerador aleatório
    unsigned char minerador = (unsigned char)(genRandLong(r) % 256);
    dataBlock[POSICAO_MINERADOR] = minerador;

    if (numeroDoBloco == 1) 
    {
        const char *fraseGenesis = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        strcpy((char*)dataBlock, fraseGenesis);
        dataBlock[POSICAO_MINERADOR] = minerador; 
        return 0;
    }

    
    unsigned int saldoTemp[TOTAL_ENDERECOS];
    

    if (carteiraOrigem != NULL) 
        memcpy(saldoTemp, carteiraOrigem, sizeof(unsigned int) * TOTAL_ENDERECOS);
    else 
    {
        for (int i = 0; i < TOTAL_ENDERECOS; i++) 
            saldoTemp[i] = getSaldo((unsigned char)i);
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


        if (saldoTemp[origem] == 0) {
            candidatos[indiceSorteado] = candidatos[totalCandidatos - 1];
            totalCandidatos--;
        }
        
        // Se destino não estava na lista e agora tem saldo, adiciona
        // (Apenas se valor > 0 e destino era 0 antes)
    }
    
    return transacoesValidas;
}
