#ifndef STORAGE_H
#define STORAGE_H

// Bibliotecas 
#include <stdio.h>
#include "structs.h"

// --- FUNÇÕES ORIGINAIS (MANTIDAS) ---
void inicializarStorage(const char *nomeArquivo); // Inicializa o sistema de armazenamento da blockchain
void finalizarStorage(); // Finaliza o sistema de armazenamento
void adicionarBloco(BlocoMinerado *bloco); // Adiciona um novo bloco minerado ao sistema
void listarBlocosMinerador(unsigned char endereco, int n); // Lista os primeiros N blocos minerados por um endereço específico
void relatorioTransacoes(unsigned int n); // Gera relatório de blocos ordenados por número de transações (Item h)
void exibirEstatisticas(); // Exibe estatísticas gerais do sistema
void relatorioColisoes(); // Função de Debug

// --- NOVAS FUNÇÕES NECESSÁRIAS (ADICIONADAS PARA O MENU/MAIN) ---

// Permite à main saber quantos blocos existem (para loops)
unsigned int obterTotalBlocos(); 

// Permite à main ler um bloco específico (para reconstruir a carteira)
int buscarBlocoPorId(unsigned int id, BlocoMinerado *saida); 

// Item (f): Busca e imprime um bloco pelo número (eficiente)
void imprimirBlocoPorNumero(unsigned int numero);

// Item (i): Busca e imprime TODOS os blocos com esse nonce (suporta colisões)
void listarBlocosPorNonce(unsigned int nonce);

// Itens (c), (d) e (e): Relatórios calculados sob demanda
void relatorioMaxTransacoes(); 
void relatorioMinTransacoes();
void calcularMediaBitcoinsPorBloco();

#endif