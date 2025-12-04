#include<stdio.h>
#include<stdlib.h>
#include "mtwister.h"

#define DATA_SIZE 1008
#define QTD_BLOCOS 100000 // quantidade de blocos de 4096 da simulação

// Criando struct de 8 + 8 + 1024 bytes -> cada escrita/leitura no arquivo será de 4 registros
// 4096 / 1024 = 4
typedef struct TRegistro
{
    unsigned long chave; // 8 bytes
    unsigned long naochave;
    unsigned char data[DATA_SIZE];
}TRegistro;

int main()
{
    FILE *fArq = fopen("exemplo.txt", "w+");

    if(fArq == NULL)
    {
        perror("Nao consegui abrir o arquivo\n");
        return -1;
    }

    MTRand r = seedRand(1234567);
    TRegistro bufferRAM[4];
    size_t qtdRegistrosEscritos = 0, qtdRegistrosLidos = 0;
    unsigned long contadorRegistros = 0;

    for(int i = 0; i < QTD_BLOCOS; i++) // laco do numero de blocos
    {
        // Para cada escrita produzimos 4 registros!
        for(int j = 0; j < 4; j++)
        {
            bufferRAM[j].chave = contadorRegistros;
            contadorRegistros++;
            bufferRAM[j].naochave = (unsigned long) genRandLong(&r);

            for(int p = 0; p < DATA_SIZE; p++)
            {
                bufferRAM[j].data[p] = (unsigned char) genRandLong(&r) % (126 - 33) + 33;
            }

            int *raiz = NULL; // Exemplo qualquer de de &raiz
            bufferRAM[j].data[DATA_SIZE - 1] = '\0'; // Tratando como string %s
            insereABP(&raiz, bufferRAM[j].naochave, i); // Arvore Binaria de Pesquisa para o campo NAO CHAVE
        }
        qtdRegistrosEscritos = fwrite(bufferRAM, sizeof(TRegistro), 4, fArq);
        // printf("Gravei %lu registros!!!\n", qtdRegistrosEscritos);
    }

    // Agora vamos imprimir o arquivo de 4 em 4 registros!!!
    rewind(fArq);
    for(unsigned long i = 0; i < QTD_BLOCOS; i++)
    {
        qtdRegistrosLidos = fread(bufferRAM, sizeof(TRegistro), 4, fArq);
        printf("Bloco Numero %lu\n", i);

        for(int j =0; j < qtdRegistrosLidos; j++)
        {
            printf("\t chave = %lu", bufferRAM[j].chave);
            printf("\t nao chave = %lu", bufferRAM[j].naochave);
            printf("\t data = %s", bufferRAM[j].data);
        }
    }

    // Se sabemos o numero do bloco, posicionamos o cabecote no inicio dele
    unsigned long numerobloco = 888;

    fseek(fArq, 4096 * numerobloco, SEEK_SET); // Avanca 4096 * numerobloco bytes
    qtdRegistrosLidos = fread(bufferRAM, sizeof(TRegistro), 4, fArq);

    for(int j =0; j < qtdRegistrosLidos; j++)
    {   // Se tivermos tambem o campo chave, é só fazer uma pesquisa
        printf("\t chave = %lu", bufferRAM[j].chave);
        printf("\t nao chave = %lu", bufferRAM[j].naochave);
        printf("\t data = %s", bufferRAM[j].data);
    }

    fclose(fArq);
    return 0;
}