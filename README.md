# Simulação de Blockchain Simplificada em C

Este repositório contém um simulador de Blockchain inspirado no Bitcoin.

O sistema é capaz de minerar, validar e armazenar **30.000 blocos** de forma eficiente, utilizando criptografia real (SHA-256) e estruturas de dados avançadas para garantir consultas de alta performance em memória RAM.

---

## Funcionalidades Principais

* **Mineração Proof-of-Work (PoW):** Implementação de mineração com dificuldade dinâmica (simulada) e validação via hash SHA-256.
* **Persistência Binária:** Armazenamento dos blocos em arquivo binário (`.bin`) para acesso rápido e compacto, com buffer de escrita para reduzir I/O.
* **Gestão de Transações:** Geração e validação de transações financeiras entre 256 carteiras (endereços).
* **Relatórios Estatísticos:** Geração de relatórios financeiros e técnicos com complexidade otimizada.
* **Recuperação de Estado:** O sistema é capaz de reconstruir todos os índices em RAM a partir do arquivo em disco em caso de reinício.

---

## Estruturas de Dados e Otimizações

O foco do projeto foi a eficiência algorítmica. Para gerenciar 30.000 blocos sem lentidão, foram utilizadas as seguintes estratégias:

### 1. Indexação por Tabela Hash (Chaining)
Para a busca de blocos por *Nonce* (Item I), implementou-se uma **Hash Table** com tratamento de colisões por encadeamento.
* **Tamanho:** 2<sup>14</sup> (16.384 slots).
* **Performance:** Busca média em O(1) a O(L), onde L é o fator de colisão estatístico da mineração.

### 2. Bucket Sort (Ordenação Linear)
Para listar blocos ordenados por quantidade de transações (Item H), substituiu-se o QuickSort (O(N log N)) pelo **Bucket Sort**.
* Como o número de transações é limitado (0 a 61), o Bucket Sort permite ordenar todos os 30.000 blocos em tempo **O(N)**.

### 3. Índices Remissivos em RAM
* **Vetor de Listas:** Um array de 256 posições contendo listas encadeadas para acesso imediato (O(1)) aos blocos de qualquer minerador.
* **Cache "On-the-fly":** Estatísticas como "Maior Saldo" e "Bloco com Max Transações" são calculadas durante a inserção, tornando a consulta instantânea.

---

## 📊 Análise de Complexidade

| Operação | Estrutura Utilizada | Complexidade |
| :--- | :--- | :--- |
| **Buscar Bloco por ID** | Acesso Direto (fseek) | O(1) |
| **Relatório: Maior Saldo** | Cache Global | O(1) |
| **Relatório: Max Transações** | Lista de Recordes | O(1) |
| **Listar Blocos de Minerador** | Vetor de Listas | O(K) |
| **Listar Ordenado por Tx** | Bucket Sort | O(N) |
| **Buscar por Nonce** | Hash Table | O(1)* |

*\* Complexidade média, dependendo da distribuição estatística dos nonces.*

---

## Pré-requisitos e Instalação

O projeto foi desenvolvido em ambiente Linux (Ubuntu) e depende da biblioteca **OpenSSL** para as funções criptográficas.

### 1. Instalar Dependências
```bash
sudo apt update
sudo apt install libssl-dev build-essential
```

### 2. Compilar o Projeto
Utilize o gcc com a flag `-O3` para máxima performance de mineração:

```bash
gcc main.c storage.c miner.c transactions.c mtwister.c -o blockchain -O3 -lssl -lcrypto -Wall
```

---

## ▶️ Como Executar

Basta rodar o executável gerado:

```bash
./blockchain
```

> Na primeira execução, o sistema irá minerar os 30.000 blocos automaticamente e criar o arquivo `blockchain.bin`. Isso pode levar alguns segundos dependendo da sua CPU. Nas execuções seguintes, ele carregará os dados do disco instantaneamente.

---

## Menu

O sistema oferece as seguintes opções via terminal:

- **1.** Endereço com mais Bitcoins (Rich List).
- **2.** Endereço que mais minerou.
- **3.** Bloco com MAIS transações.
- **4.** Bloco com MENOS transações.
- **5.** Média de Bitcoins por bloco.
- **6.** Imprimir bloco por número (ID).
- **7.** Listar N blocos de um minerador.
- **8.** Listar N blocos ordenados por transações (Bucket Sort).
- **9.** Buscar blocos por Nonce (Hash Table).
- **10.** Histograma da Hash Table (Distribuição visual)
- **Exportar Relatório:** Gera o arquivo `blockchain.txt` legível.

---

## 📂 Estrutura de Arquivos

```
📦 blockchain-simulator
├── 📄 main.c             # Ponto de entrada e menu interativo
├── 📄 miner.c            # Lógica de Proof-of-Work e cálculo de hash SHA-256
├── 📄 storage.c          # Gerenciamento de memória, índices (Hash/Listas) e I/O
├── 📄 transactions.c     # Geração aleatória e validação de transações
├── 📄 structs.h          # Definições das estruturas de dados (Bloco, NoHash, etc.)
├── 📄 mtwister.c         # Gerador de números pseudoaleatórios (Mersenne Twister)
└── 📄 README.md          # Este arquivo
```

---

## 🎯 Destaques Técnicos

### Otimização de Nonce
Durante os testes, identificamos que iniciar o nonce em 0 causava alta concentração de colisões na Hash Table devido à baixa dificuldade de mineração. A solução foi inicializar o nonce com valores aleatórios (Mersenne Twister), o que melhorou significativamente a distribuição e reduziu o tempo de busca.

**Antes (Nonce = 0):**
- Alta concentração nos primeiros slots
- Múltiplas colisões → O(L) degradado

**Depois (Nonce aleatório):**
- Distribuição uniforme
- Tempo de busca próximo a O(1)
---

## 📈 Resultados e Performance

- ✅ Consultas instantâneas (< 1ms) para operações em cache
- ✅ Busca por Nonce em tempo médio O(1)
- ✅ Ordenação linear em O(N) para 30.000 elementos
- ✅ Arquivo binário compacto (~6-7 MB)

---

## Autores

Projeto Final desenvolvido para a disciplina de **Estruturas de Dados 2**, Universidade Tecnológica Federal do Paraná (UTFPR - Campus Ponta Grossa).

- **Tobias Rocha** - [GitHub]([https://github.com/tobiasrocha](https://github.com/rochatobias))
- **Gabriel Henrique Roldão de Souza** - [GitHub](https://github.com/gabrielhenrique-c)
- **Gabriel De Donno Laurindo** - [GitHub](https://github.com/NeruNeru367)

---
