# Análise de Complexidade - Itens A até I

Este documento detalha a complexidade Big O e o tempo médio de cada consulta do projeto.

**Notação utilizada**:
- `N` = Total de blocos na blockchain (30.000 para ED2)
- `M` = Total de endereços (256 fixo)
- `T` = Máximo de transações por bloco (61 fixo)
- `K` = Parâmetro de entrada do usuário (ex: número de blocos a listar)
- `H` = Tamanho da hash table (16.384 = 2^14)

---

## Item A: Endereço(s) com mais Bitcoins

### Consulta
```c
void relatorioMaisRico() {
    printf("Saldo Máximo: %u BTC\n", maiorSaldoAtual);  // O(1)
    for(int i = 0; i < NUM_ENDERECOS; i++) {            // O(M)
        if(saldos[i] == maiorSaldoAtual) printf("%d ", i);
    }
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(M)** = O(256) = **O(1)** constante |
| **Tempo médio** | ~256 comparações |
| **Espaço extra** | O(1) |

### Explicação

- `maiorSaldoAtual` é mantido como cache durante a mineração
- Atualização do cache: O(1) por bloco → O(N) total durante mineração
- Consulta: apenas percorre 256 endereços fixos
- Como M=256 é constante, considera-se **O(1)**

### Estrutura de Dados
- Array `saldos[256]`: acesso direto por índice
- Variável `maiorSaldoAtual`: cache do valor máximo

---

## Item B: Endereço(s) que minerou mais blocos

### Consulta
```c
void relatorioMaiorMinerador() {
    printf("Qtd Blocos: %u\n", maiorQtdMinerada);  // O(1)
    for(int i = 0; i < NUM_ENDERECOS; i++) {       // O(M)
        if(blocosMinerados[i] == maiorQtdMinerada) printf("%d ", i);
    }
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(M)** = O(256) = **O(1)** constante |
| **Tempo médio** | ~256 comparações |
| **Espaço extra** | O(1) |

### Explicação

- Mesmo padrão do Item A
- `maiorQtdMinerada` é cache atualizado durante mineração
- Array `blocosMinerados[256]` consultado linearmente
- M=256 constante → **O(1)** na prática

---

## Item C: Bloco(s) com MAIS transações + hash

### Consulta
```c
void relatorioMaxTransacoes() {
    printf("Quantidade: %d transações\n", maxTransacoesGlobal);  // O(1)
    for(NoRecorde *r = listaMaxTx; r != NULL; r = r->prox) {     // O(E)
        lerBlocoPorId(r->idBloco, &temp);                         // O(1)
        // imprime hash
    }
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(E)** onde E = empates |
| **Caso típico** | O(1) a O(poucos) - poucos empates |
| **Pior caso** | O(N) se todos blocos tiverem mesmo nº de transações |
| **Tempo médio** | ~1-10 blocos (estatisticamente improvável ter muitos empates) |
| **Espaço extra** | O(E) para lista de empates |

### Explicação

- `maxTransacoesGlobal` é cache O(1)
- Lista de empates é dinâmica
- Quando surge novo máximo, lista anterior é liberada → economia de memória
- [lerBlocoPorId()](file:///home/tobias/Documents/BlockChain-Simulation/storage.c#328-343) é O(1) por acesso direto no arquivo

### Estrutura de Dados
- Lista encadeada `listaMaxTx`: permite empates ilimitados
- Liberação O(E) quando novo recorde surge

---

## Item D: Bloco(s) com MENOS transações + hash

### Consulta
```c
void relatorioMinTransacoes() {
    // Mesma estrutura do Item C
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(E)** onde E = empates |
| **Caso típico** | O(muitos) - blocos com 0 transações são comuns |
| **Pior caso** | O(N) |
| **Tempo médio** | Centenas a milhares de blocos com 0 tx |
| **Espaço extra** | O(E) |

### Explicação

- Diferente do máximo, o mínimo tende a ter MUITOS empates
- Blocos com 0 transações são comuns (transações aleatórias 0-61)
- Pode haver milhares de blocos empatados → lista grande

### Possível Otimização
Se necessário limitar empates, poderia usar array fixo com limite.

---

## Item E: Média de bitcoins por bloco

### Consulta
```c
void calcularMediaBitcoinsPorBloco() {
    double media = (double)totalValorTransacionado / stats.totalBlocos;  // O(1)
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(1)** |
| **Tempo médio** | 1 divisão |
| **Espaço extra** | O(1) |

### Explicação

- `totalValorTransacionado` acumulado durante mineração
- `stats.totalBlocos` contador simples
- Consulta é apenas uma divisão

### Estrutura de Dados
- `unsigned long long totalValorTransacionado`: evita overflow (30k blocos × 61 tx × 50 BTC = ~91M)
- Divisão em ponto flutuante para precisão

---

## Item F: Imprimir bloco por número

### Consulta
```c
void imprimirBlocoPorNumero(unsigned int numero) {
    lerBlocoPorId(numero, &temp);  // O(1)
    imprimirBlocoCompleto(&temp);  // O(T)
}

static int lerBlocoPorId(unsigned int id, BlocoMinerado *saida) {
    long offset = (long)(id - 1) * sizeof(BlocoMinerado);  // O(1)
    fseek(arquivoAtual, offset, SEEK_SET);                  // O(1)
    fread(saida, sizeof(BlocoMinerado), 1, arquivoAtual);   // O(1)
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(1)** para busca + **O(T)** para impressão |
| **Total** | **O(T)** = O(61) = **O(1)** constante |
| **Tempo médio** | 1 seek + 1 read + 61 iterações |
| **Espaço extra** | O(1) |

### Explicação

- Blocos armazenados sequencialmente no arquivo binário
- Offset = [(id - 1) × sizeof(BlocoMinerado)](file:///home/tobias/Documents/BlockChain-Simulation/main.c#114-200) = cálculo direto
- `fseek` em arquivo é O(1) com offset conhecido
- Impressão percorre até 61 transações

### Estrutura de Dados
- Arquivo binário: acesso aleatório O(1)
- Buffer de 16 blocos: verifica se bloco está em memória primeiro

---

## Item G: N primeiros blocos de um minerador

### Consulta
```c
void listarBlocosMinerador(unsigned char endereco, int n) {
    NoMinerador *atual = indiceMinerador[endereco];  // O(1)
    while (atual != NULL && count < n) {              // O(K)
        lerBlocoPorId(atual->idBloco, &temp);         // O(1)
        imprimirBlocoCompleto(&temp);                  // O(T)
        atual = atual->prox;
        count++;
    }
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(K × T)** = O(K) pois T=61 é constante |
| **Tempo médio** | K × (1 seek + 1 read + 61 comparações) |
| **Espaço extra** | O(1) |

### Explicação

- `indiceMinerador[endereco]`: acesso direto O(1) ao endereço
- Lista encadeada percorrida até K elementos
- Cada bloco requer 1 leitura de disco

### Estrutura de Dados
- Array de 256 listas: `indiceMinerador[M]`
- Inserção no FIM para preservar ordem cronológica
- Ponteiro `fimMinerador[endereco]` para inserção O(1)

### Comparação com Alternativas

| Estrutura | Busca | Inserção | Espaço |
|-----------|-------|----------|--------|
| Array 256 listas (atual) | O(1) + O(K) | O(1) | O(N) |
| Hash table | O(1) médio | O(1) | O(N + H) |
| Árvore B | O(log N) | O(log N) | O(N) |

**Escolha justificada**: Com apenas 256 endereços possíveis, array de listas é ideal.

---

## Item H: N blocos ordenados por quantidade de transações

### Consulta
```c
void relatorioTransacoes(unsigned int n) {
    // 1. Carrega N blocos
    for (unsigned int i = 0; i < n; i++) {
        lerBlocoPorId(i + 1, &blocos[i]);  // O(1) cada
    }
    // Total: O(K)

    // 2. Bucket Sort com 62 buckets
    for (int i = (int)n - 1; i >= 0; i--) {
        int qtd = obterContagemDoCache(blocos[i].bloco.numero);  // O(1)
        next[i] = buckets[qtd];
        buckets[qtd] = i;
    }
    // Total: O(K)

    // 3. Impressão ordenada
    for (int t = 0; t < 62; t++) {
        for (int idx = buckets[t]; idx != -1; idx = next[idx]) {
            imprimirBlocoCompleto(&blocos[idx]);  // O(T)
        }
    }
    // Total: O(K × T)
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade** | **O(K)** para ordenação + **O(K×T)** para impressão |
| **Total** | **O(K)** pois T=61 é constante |
| **Tempo médio** | K leituras + K cache lookups + K impressões |
| **Espaço extra** | **O(K)** para array de blocos + O(K) para next |

### Explicação

1. **Leitura**: K acessos ao disco, cada um O(1)
2. **Bucket Sort**: 
   - 62 buckets (0 a 61 transações)
   - Distribuição O(K)
   - Coleta implícita na ordem dos buckets
3. **Cache de contagem**: Evita recontar transações (Otimização 4)

### Comparação com Alternativas

| Algoritmo | Complexidade | Estabilidade | Espaço |
|-----------|-------------|--------------|--------|
| Bucket Sort (atual) | **O(K)** | Sim | O(K + 62) |
| Quick Sort | O(K log K) | Não | O(log K) |
| Merge Sort | O(K log K) | Sim | O(K) |
| Counting Sort | O(K + 62) | Sim | O(62) |

**Escolha justificada**: Bucket Sort é O(K) quando o range é limitado (0-61).

---

## Item I: Blocos por nonce

### Consulta
```c
int listarBlocosPorNonce(unsigned int nonce) {
    unsigned int pos = hashFunction(nonce);     // O(1)
    NoHash *atual = tabelaNonce[pos];           // O(1)
    
    while (atual != NULL) {                      // O(λ) onde λ = comprimento da lista
        if (atual->nonce == nonce) {
            lerBlocoPorId(atual->idBloco, &temp); // O(1)
            imprimirBlocoCompleto(&temp);          // O(T)
            encontrados++;
        }
        atual = atual->prox;
    }
}
```

### Análise

| Métrica | Valor |
|---------|-------|
| **Complexidade média** | **O(λ)** onde λ = N/H = 30.000/16.384 ≈ 1.83 |
| **Complexidade prática** | **O(1)** a **O(poucos)** |
| **Pior caso** | O(N) se todos nonces colidem |
| **Tempo médio** | ~2 comparações por busca |
| **Espaço extra** | O(N) para hash table |

### Fator de Carga

```
λ = N / H = 30.000 / 16.384 ≈ 1.83
```

Com λ ≈ 2, cada slot tem em média 2 elementos → busca é praticamente O(1).

### Função de Hash

```c
static unsigned int hashFunction(unsigned int nonce) {
    return (nonce * KNUTH_CONST) >> SHIFT_AMOUNT;
}
// KNUTH_CONST = 2654435761 (número de Knuth)
// SHIFT_AMOUNT = 32 - 14 = 18
```

**Hash multiplicativo de Knuth**: distribuição uniforme para inteiros.

### Estrutura de Dados
- Hash table com encadeamento (chaining)
- 2^14 = 16.384 slots
- Memória: 16.384 × 8 bytes = ~128KB (ponteiros)
- Nós: N × sizeof(NoHash) = 30.000 × 12 bytes = ~360KB

### Comparação com Alternativas

| Estrutura | Busca | Inserção | Espaço |
|-----------|-------|----------|--------|
| Hash Table (atual) | O(1) médio | O(1) | O(N + H) |
| Árvore AVL | O(log N) | O(log N) | O(N) |
| Array ordenado | O(log N) | O(N) | O(N) |
| Lista linear | O(N) | O(1) | O(N) |

**Escolha justificada**: Hash table é ideal para buscas exatas frequentes.

---

## Resumo Geral de Complexidades

| Item | Operação | Big O | Tempo Médio (30k blocos) |
|------|----------|-------|-------------------------|
| **A** | Maior saldo | O(1) | 256 comparações |
| **B** | Maior minerador | O(1) | 256 comparações |
| **C** | Bloco max tx | O(E) | 1-10 blocos |
| **D** | Bloco min tx | O(E) | 100-1000 blocos |
| **E** | Média BTC | O(1) | 1 divisão |
| **F** | Busca por número | O(1) | 1 seek + 1 read |
| **G** | Blocos minerador | O(K) | K × (seek + read) |
| **H** | Blocos ordenados | O(K) | K × (read + cache) |
| **I** | Busca por nonce | O(1) médio | ~2 comparações |

---

## Memória Total Utilizada

| Componente | Cálculo | Total |
|------------|---------|-------|
| Saldos | 256 × 4 bytes | 1 KB |
| Blocos minerados | 256 × 4 bytes | 1 KB |
| Hash table ponteiros | 16.384 × 8 bytes | 128 KB |
| Hash table nós | 30.000 × 12 bytes | 360 KB |
| Índice mineradores | 256 × 16 bytes | 4 KB |
| Nós mineradores | 30.000 × 16 bytes | 480 KB |
| Cache contagem | 30.000 × 1 byte | 30 KB |
| **TOTAL** | | **~1 MB** |

Isso é eficiente considerando que evita carregar todos os blocos em RAM (7.5 MB).
