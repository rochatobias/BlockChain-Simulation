/*
 * main.c - Simulação de Blockchain (Projeto Final ED2)
 * 
 * OTIMIZAÇÃO 3 IMPLEMENTADA:
 * - Carteira local eliminada - usa getSaldo() do storage
 * - Fonte única de verdade para saldos
 * - Evita duplicação de estado e inconsistências
 * 
 * NOTA: Mantemos uma carteira local APENAS durante geração de transações
 * do bloco atual, pois precisamos simular múltiplas transações antes de
 * efetivar. Após mineração, o storage é a fonte de verdade.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mtwister.h"
#include "structs.h"
#include "miner.h"
#include "transactions.h"
#include "storage.h"

// ============================================================================
// CONSTANTES DO PROJETO
// ============================================================================

#define TOTAL_BLOCOS_SIMULACAO 30000   // ED2: 30.000 blocos
#define ARQUIVO_BLOCKCHAIN "blockchain.bin"

// ============================================================================
// ESTADO GLOBAL
// ============================================================================

MTRand r;  // Gerador Mersenne Twister

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

void inicializarEstado() {
    // Semente fixa exigida pelo PDF
    r = seedRand(1234567); 
}

/**
 * Executa a mineração de todos os blocos
 * 
 * OTIMIZAÇÃO 3: Não mantemos mais carteira duplicada.
 * A função gerarDadosDoBloco usa getSaldo() do storage.
 */
void rodarSimulacao() {
    printf("Iniciando mineração de %d blocos...\n", TOTAL_BLOCOS_SIMULACAO);
    
    BlocoMinerado anterior;
    unsigned char dadosBuffer[184];
    
    // --- BLOCO 1 (GÊNESIS) ---
    // Para o Gênesis, passamos NULL como carteira (usa storage internamente)
    gerarDadosDoBloco(1, dadosBuffer, NULL, &r); 
    BlocoMinerado genesis = criarBlocoGenesis(dadosBuffer); 
    
    // Storage atualiza saldos automaticamente ao adicionar bloco
    adicionarBloco(&genesis);
    anterior = genesis;

    printf("Bloco 1 (Gênesis) minerado.\n");

    // --- BLOCOS 2 até N ---
    for (unsigned int i = 2; i <= TOTAL_BLOCOS_SIMULACAO; i++) {
        // OTIMIZAÇÃO 3: Passa NULL para usar getSaldo() do storage
        gerarDadosDoBloco(i, dadosBuffer, NULL, &r);
        
        BlocoMinerado novo = criarProxBloco(anterior, i, dadosBuffer);
        
        // Storage atualiza saldos e estatísticas automaticamente
        adicionarBloco(&novo);
        
        anterior = novo;

        if (i % 1000 == 0) {
            printf("Bloco %u minerado... (%.1f%%)\n", i, (float)i/TOTAL_BLOCOS_SIMULACAO*100);
        }
    }
    
    printf("Simulação concluída!\n");
}

// ============================================================================
// MENU INTERATIVO
// ============================================================================

void exibirMenu() {
    printf("\n╔═══════════════════════════════════════════╗\n");
    printf("║    MENU BLOCKCHAIN SIMPLIFICADA           ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║ 1. [a] Endereço com mais Bitcoins         ║\n");
    printf("║ 2. [b] Endereço que minerou mais blocos   ║\n");
    printf("║ 3. [c] Bloco com MAIS transações          ║\n");
    printf("║ 4. [d] Bloco com MENOS transações         ║\n");
    printf("║ 5. [e] Média de Bitcoins por bloco        ║\n");
    printf("║ 6. [f] Imprimir bloco por número          ║\n");
    printf("║ 7. [g] Imprimir N blocos de um minerador  ║\n");
    printf("║ 8. [h] Imprimir N blocos (Ord. por tx)    ║\n");
    printf("║ 9. [i] Buscar blocos por Nonce            ║\n");
    printf("║ 10. Gerar Histograma Hash                 ║\n");
    printf("║ 0. Sair                                   ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("Escolha: ");
}

/* --- Adicionado: utilitário para calcular tempo em ms --- */
static double tempo_ms(struct timespec inicio, struct timespec fim) {
	// Retorna milissegundos com precisão sub-mili
	return (fim.tv_sec - inicio.tv_sec) * 1000.0 + (fim.tv_nsec - inicio.tv_nsec) / 1e6;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    // 1. Configuração Inicial
    inicializarEstado();
    inicializarStorage(ARQUIVO_BLOCKCHAIN);
    
    // 2. Verificação de Persistência
    unsigned int totalBlocosDisco = obterTotalBlocos();
    
    if (totalBlocosDisco < TOTAL_BLOCOS_SIMULACAO) {
        if (totalBlocosDisco > 0) {
            printf("AVISO: Blockchain incompleta (%u/%d). Reiniciando para consistência.\n", 
                   totalBlocosDisco, TOTAL_BLOCOS_SIMULACAO);
            finalizarStorage();
            remove(ARQUIVO_BLOCKCHAIN);
            inicializarStorage(ARQUIVO_BLOCKCHAIN);
        }
        rodarSimulacao();
    } else {
        // Storage já reconstruiu tudo ao inicializar
        printf("Blockchain completa carregada: %u blocos.\n", totalBlocosDisco);
    }

    // 3. Menu Interativo
    int opcao;
    do {
        exibirMenu();
        
        if (scanf("%d", &opcao) != 1) {
            while(getchar() != '\n');
            opcao = -1;
        }

        unsigned int num, nonce;
        int n;
        unsigned char end;

        // Variáveis de medição de tempo por opção
        struct timespec t_start, t_end;
        double t_elapsed;

        switch(opcao) {
            case 1:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                relatorioMaisRico();
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                t_elapsed = tempo_ms(t_start, t_end);
                printf("Tempo de execução: %.3f ms\n", t_elapsed);
                break;
            case 2:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                relatorioMaiorMinerador();
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 3:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                relatorioMaxTransacoes();
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 4:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                relatorioMinTransacoes();
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 5:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                calcularMediaBitcoinsPorBloco();
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 6:
                printf("Digite o número do bloco: ");
                scanf("%u", &num);
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                imprimirBlocoPorNumero(num);
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 7:
                printf("Endereço do minerador (0-255): ");
                scanf("%hhu", &end);
                printf("Quantidade de blocos (N): ");
                scanf("%d", &n);
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                listarBlocosMinerador(end, n);
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 8:
                printf("Quantidade de blocos para analisar (N): ");
                scanf("%u", &num);
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                relatorioTransacoes(num);
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 9:
                printf("Digite o Nonce: ");
                scanf("%u", &nonce);
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                listarBlocosPorNonce(nonce);
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            case 10:
                 clock_gettime(CLOCK_MONOTONIC, &t_start);
                 exibirHistogramaHash();
                 clock_gettime(CLOCK_MONOTONIC, &t_end);
                 printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                 break;
            case 0:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                printf("Finalizando sistema...\n");
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
                break;
            default:
                clock_gettime(CLOCK_MONOTONIC, &t_start);
                printf("Opção inválida!\n");
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                printf("Tempo de execução: %.3f ms\n", tempo_ms(t_start, t_end));
        }
    } while(opcao != 0);

    // 4. Encerramento
    finalizarStorage();
    return 0;
}