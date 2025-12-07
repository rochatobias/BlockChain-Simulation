#include <stdio.h>
#include <string.h> 
#include "mtwister.h"
#include "transactions.h"
#include "structs.h"

#define TOTAL_ENDERECOS 256
#define TAMANHO_DATA 184
#define RECOMPENSA_MINERACAO 50
#define MAX_TRANSACOES 61
#define POSICAO_MINERADOR 183 

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

/*
 * Conta transações em um bloco já preenchido.
 */
int contarTransacoesNoBloco(unsigned char dataBlock[]) {
    int count = 0;
    // Percorre de 3 em 3, parando antes de invadir a área do minerador
    for (int i = 0; i <= POSICAO_MINERADOR - 3; i += 3) {
        
        // Se encontrou uma tripla de zeros (possível fim ou transação válida 0->0)
        if (dataBlock[i] == 0 && dataBlock[i+1] == 0 && dataBlock[i+2] == 0) {
            
            // Lookahead: Verifica se os próximos bytes TAMBÉM são zero para confirmar o fim
            int ehFimMesmo = 1;
            for (int j = i + 3; j < i + 9 && j < POSICAO_MINERADOR; j++) {
                if (dataBlock[j] != 0) {
                    ehFimMesmo = 0; // Falso alarme: havia dados mais à frente
                    break;
                }
            }
            
            if (ehFimMesmo) break; // Confirmado: acabou o bloco
        }
        count++;
    }
    return count;
}

/*
 * Função de DEBUG para imprimir o que tem dentro do bloco.
 */
void imprimirBlocoDebug(unsigned char dataBlock[]) {
    int qtd = contarTransacoesNoBloco(dataBlock);
    printf("\n--- DEBUG DO BLOCO ---\n");
    printf("Minerador (pos %d): %d\n", POSICAO_MINERADOR, dataBlock[POSICAO_MINERADOR]);
    
    // Se for texto (Gênesis)
    if (qtd == 0 && dataBlock[0] != 0) {
        printf("Mensagem do Bloco: %s\n", dataBlock);
    } else {
        printf("Quantidade de Transacoes: %d\n", qtd);
        for(int i=0; i < qtd; i++) {
            int idx = i*3;
            printf("  [%02d] Origem: %3d | Destino: %3d | Valor: %3d\n", 
                   i+1, dataBlock[idx], dataBlock[idx+1], dataBlock[idx+2]);
        }
    }
    printf("----------------------\n");
}
//Funções Auxiliares

/*
 * FUNÇÃO 1: GERAR DADOS (Ocorre ANTES da mineração)
 */
int gerarDadosDoBloco(unsigned int numeroDoBloco, unsigned char dataBlock[], unsigned int carteiraOficial[], MTRand *r) {
    
    // faz a limpa do vetor inteiro com zeros
    memset(dataBlock, 0, TAMANHO_DATA);

    // define minerador aleatorio
    unsigned char minerador = (unsigned char)(genRandLong(r) % 256);
    dataBlock[POSICAO_MINERADOR] = minerador;

    // logica para o bloco gênesis
    if (numeroDoBloco == 1) {
        const char *fraseGenesis = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        strcpy((char*)dataBlock, fraseGenesis);
        dataBlock[POSICAO_MINERADOR] = minerador; 
        return 0;
    }

    // --- TRANSAÇÕES (Blocos >= 2) ---
    
    // Cria cópia da carteira para validação local
    unsigned int saldoTemp[TOTAL_ENDERECOS];
    memcpy(saldoTemp, carteiraOficial, sizeof(unsigned int) * TOTAL_ENDERECOS);

    // Quantidade aleatória de transações (0 a 61)
    int qtdTransacoes = (int)(genRandLong(r) % (MAX_TRANSACOES + 1)); 
    int posicaoAtual = 0;
    int transacoesValidas = 0;

    for (int i = 0; i < qtdTransacoes; i++) {
        
        // faz a busca de candidatos com saldo na carteira temporária
        unsigned char candidatos[TOTAL_ENDERECOS];
        int totalCandidatos = 0;

        for (int k = 0; k < TOTAL_ENDERECOS; k++) {
            if (saldoTemp[k] > 0) {
                candidatos[totalCandidatos] = (unsigned char)k;
                totalCandidatos++;
            }
        }

        if (totalCandidatos == 0) break; //sem $$ no sistema

        //sorteios
        int indiceSorteado = (int)(genRandLong(r) % totalCandidatos);
        unsigned char origem = candidatos[indiceSorteado];
        unsigned char destino = (unsigned char)(genRandLong(r) % 256); 

        // Valor da transação: limite é o saldo do usuário, mas nunca superior a 50.
        unsigned int maximoPossivel = saldoTemp[origem];
        if (maximoPossivel > 50) maximoPossivel = 50; 

        unsigned char valor = (unsigned char)(genRandLong(r) % (maximoPossivel + 1));

        // grava no vetor
        dataBlock[posicaoAtual]   = origem;
        dataBlock[posicaoAtual+1] = destino;
        dataBlock[posicaoAtual+2] = valor;
        
        // atualiza controle
        posicaoAtual += 3;
        transacoesValidas++;

        // atualiza saldo temporário
        saldoTemp[origem] -= valor;
        saldoTemp[destino] += valor;
    }
    
    return transacoesValidas;
}

//função 2: atualizar a carteira, acontece depois da mineração;
void atualizarCarteira(unsigned int numeroDoBloco, unsigned char dataBlock[], unsigned int carteiraOficial[], int qtdTransacoes) {
    
    // 1. Recompensa Minerador (+50 BTC)
    unsigned char minerador = dataBlock[POSICAO_MINERADOR];
    carteiraOficial[minerador] += RECOMPENSA_MINERACAO;

    // Se for Gênesis (1), sai.
    if (numeroDoBloco == 1) return;

    // Se não sabemos a quantidade, contamos agora.
    if (qtdTransacoes == -1) {
        qtdTransacoes = contarTransacoesNoBloco(dataBlock);
    }

    // 2. Efetiva as transações
    int index = 0;
    for (int i = 0; i < qtdTransacoes; i++) {
        unsigned char origem = dataBlock[index];
        unsigned char destino = dataBlock[index+1];
        unsigned char valor = dataBlock[index+2];
        
        // segurança
        if (carteiraOficial[origem] >= valor) {
            carteiraOficial[origem] -= valor;
            carteiraOficial[destino] += valor;
        } else {
            printf("[ERRO CRITICO] Bloco %u: Transacao invalida! Origem %d tem %d mas tentou gastar %d.\n", 
                   numeroDoBloco, origem, carteiraOficial[origem], valor);
        }
        
        index += 3;
    }

}

