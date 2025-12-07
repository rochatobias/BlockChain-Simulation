/*
 * main.c - Simulação de Blockchain (Projeto Final ED2)
 * Integração: Gabriel Henrique
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mtwister.h"
#include "structs.h"
#include "miner.h"
#include "transactions.h"
#include "storage.h"

// CONSTANTES DO PROJETO
// O PDF pede 30.000 para ED2. Para testes rápidos, use 1000.
#define TOTAL_BLOCOS_SIMULACAO 30000 
#define ARQUIVO_BLOCKCHAIN "blockchain.bin"

// ESTADO GLOBAL (RAM)
// A carteira precisa ficar na RAM para validar transações rapidamente
unsigned int carteira[256];      
unsigned int blocosMinerados[256]; // Contador auxiliar para o relatório (b)
MTRand r; // Gerador de números aleatórios

// ============================================================================
// FUNÇÕES AUXILIARES DA MAIN
// ============================================================================

void inicializarEstado() {
    memset(carteira, 0, sizeof(carteira));
    memset(blocosMinerados, 0, sizeof(blocosMinerados));
    // Semente fixa exigida pelo PDF (mtwister)
    r = seedRand(1234567); 
}

// Reconstrói o saldo da carteira lendo o arquivo (caso o programa seja fechado e reaberto)
void reconstruirEstadoPeloDisco(unsigned int totalBlocosExistentes) {
    printf("Blockchain encontrada com %u blocos.\n", totalBlocosExistentes);
    printf("Reconstruindo saldos da carteira... aguarde.\n");
    
    BlocoMinerado temp;
    
    for (unsigned int i = 1; i <= totalBlocosExistentes; i++) {
        // Usa a função pública que criamos no storage.h
        if (buscarBlocoPorId(i, &temp)) { 
            // Reaplica as transações na carteira (recompensa + transferências)
            atualizarCarteira(temp.bloco.numero, temp.bloco.data, carteira, -1);
            
            // Atualiza contagem de quem minerou (posição 183 fixa)
            unsigned char minerador = temp.bloco.data[183]; 
            blocosMinerados[minerador]++;
        }
        
        if (i % 5000 == 0) printf("Processados %u blocos...\n", i);
    }
    printf("Estado restaurado com sucesso!\n");
}

void rodarSimulacao() {
    printf("Iniciando mineracao de %d blocos...\n", TOTAL_BLOCOS_SIMULACAO);
    
    BlocoMinerado anterior;
    unsigned char dadosBuffer[184];
    
    // --- BLOCO 1 (GÊNESIS) ---
    // 1. Gera dados (Mensagem "The Times...")
    gerarDadosDoBloco(1, dadosBuffer, carteira, &r); 
    
    // 2. Minera Gênesis (Passando os dados gerados!)
    BlocoMinerado genesis = criarBlocoGenesis(dadosBuffer); 
    
    // 3. Atualiza Carteira e Salva no Disco
    atualizarCarteira(1, genesis.bloco.data, carteira, -1);
    adicionarBloco(&genesis);
    
    // 4. Atualiza estado local
    blocosMinerados[genesis.bloco.data[183]]++;
    anterior = genesis;

    printf("Bloco 1 (Genesis) minerado.\n");

    // --- BLOCOS 2 até N ---
    for (unsigned int i = 2; i <= TOTAL_BLOCOS_SIMULACAO; i++) {
        
        // 1. Gera Transações válidas (baseadas no saldo atual da carteira)
        gerarDadosDoBloco(i, dadosBuffer, carteira, &r);
        
        // 2. Minera o bloco (Passando os dados gerados!)
        BlocoMinerado novo = criarProxBloco(anterior, i, dadosBuffer);
        
        // 3. Efetiva as transações na carteira (Fundamental para o próximo bloco ser válido)
        atualizarCarteira(i, novo.bloco.data, carteira, -1);
        
        // 4. Grava no disco (Storage cuida do buffer de 16 em 16)
        adicionarBloco(&novo);
        
        // 5. Atualiza estado local e prepara para o próximo
        blocosMinerados[novo.bloco.data[183]]++;
        anterior = novo;

        // Feedback visual
        if (i % 1000 == 0) {
            printf("Bloco %u minerado... (%.1f%%)\n", i, (float)i/TOTAL_BLOCOS_SIMULACAO*100);
        }
    }
    
    printf("Simulacao concluida!\n");
}

// Funções para os relatórios que dependem da Carteira (RAM)
// Os outros relatórios já estão no storage.c

void relatorioMaisRico() { // Item (a)
    unsigned int maiorSaldo = 0;
    for(int i=0; i<256; i++) {
        if(carteira[i] > maiorSaldo) maiorSaldo = carteira[i];
    }
    
    printf("\n--- Endereco(s) com mais Bitcoins (Item a) ---\n");
    printf("Saldo Maximo: %u BTC\n", maiorSaldo);
    printf("Endereco(s): ");
    for(int i=0; i<256; i++) {
        if(carteira[i] == maiorSaldo) printf("%d ", i);
    }
    printf("\n");
}

void relatorioMaiorMinerador() { // Item (b)
    unsigned int maiorQtd = 0;
    for(int i=0; i<256; i++) {
        if(blocosMinerados[i] > maiorQtd) maiorQtd = blocosMinerados[i];
    }
    
    printf("\n--- Endereco(s) que mais minerou (Item b) ---\n");
    printf("Qtd Blocos: %u\n", maiorQtd);
    printf("Minerador(es): ");
    for(int i=0; i<256; i++) {
        if(blocosMinerados[i] == maiorQtd) printf("%d ", i);
    }
    printf("\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    // 1. Configuração Inicial
    inicializarEstado();
    inicializarStorage(ARQUIVO_BLOCKCHAIN);
    
    // 2. Verificação de Persistência (ED2)
    unsigned int totalBlocosDisco = obterTotalBlocos();
    
    if (totalBlocosDisco < TOTAL_BLOCOS_SIMULACAO) {
        if (totalBlocosDisco > 0) {
            printf("AVISO: Blockchain incompleta (%u/%d). Reiniciando do zero para consistencia.\n", 
                   totalBlocosDisco, TOTAL_BLOCOS_SIMULACAO);
            // Fecha e reabre truncando o arquivo (apaga tudo)
            finalizarStorage();
            remove(ARQUIVO_BLOCKCHAIN);
            inicializarStorage(ARQUIVO_BLOCKCHAIN);
        }
        rodarSimulacao();
    } else {
        // Se já tem tudo minerado, apenas reconstrói a memória
        reconstruirEstadoPeloDisco(totalBlocosDisco);
    }

    // 3. Menu Interativo
    int opcao;
    do {
        printf("\n============================================\n");
        printf("       MENU BLOCKCHAIN SIMPLIFICADA\n");
        printf("============================================\n");
        printf("1. [a] Endereco com mais Bitcoins\n");
        printf("2. [b] Endereco que minerou mais blocos\n");
        printf("3. [c] Bloco com MAIS transacoes\n");
        printf("4. [d] Bloco com MENOS transacoes\n");
        printf("5. [e] Media de Bitcoins por bloco\n");
        printf("6. [f] Imprimir bloco por numero\n");
        printf("7. [g] Imprimir N primeiros blocos de um minerador\n");
        printf("8. [h] Imprimir N blocos (Ord. por transacoes)\n");
        printf("9. [i] Buscar blocos por Nonce\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        if (scanf("%d", &opcao) != 1) {
            // Limpa buffer em caso de letra digitada para evitar loop infinito
            while(getchar() != '\n'); 
            opcao = -1;
        }

        unsigned int num, nonce;
        int n;
        unsigned char end;

        switch(opcao) {
            case 1: relatorioMaisRico(); break;
            case 2: relatorioMaiorMinerador(); break;
            case 3: relatorioMaxTransacoes(); break;       // Chama do storage.c
            case 4: relatorioMinTransacoes(); break;       // Chama do storage.c
            case 5: calcularMediaBitcoinsPorBloco(); break; // Chama do storage.c
            case 6: 
                printf("Digite o numero do bloco: ");
                scanf("%u", &num);
                imprimirBlocoPorNumero(num); 
                break;
            case 7:
                printf("Endereco do minerador (0-255): ");
                scanf("%hhu", &end);
                printf("Quantidade de blocos (N): ");
                scanf("%d", &n);
                listarBlocosMinerador(end, n);
                break;
            case 8:
                printf("Quantidade de blocos para analisar (N): ");
                scanf("%u", &num);
                relatorioTransacoes(num); 
                break;
            case 9:
                printf("Digite o Nonce: ");
                scanf("%u", &nonce);
                listarBlocosPorNonce(nonce); 
                break;
            case 0:
                printf("Finalizando sistema e salvando indices...\n");
                break;
            default:
                printf("Opcao invalida!\n");
        }
    } while(opcao != 0);

    // 4. Encerramento
    finalizarStorage();
    return 0;
}