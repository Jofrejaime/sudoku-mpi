# FASE 0.5 — REVISÃO TÉCNICA DE IMPLEMENTABILIDADE

## ESTATUTO: ⚠️ PROBLEMAS CRÍTICOS ENCONTRADOS

---

## RESUMO EXECUTIVO

A arquitectura proposta é **conceitualmente sólida**, mas após análise rigorosa do código existente, foram identificados **3 problemas críticos** que requerem alterações obrigatórias antes da implementação:

| Problema | Severidade | Impacto | Solução |
|----------|-----------|--------|---------|
| #1: Detecção de sucesso | 🔴 CRÍTICO | Impossível saber se solução foi encontrada | Criar `is_solved()` |
| #2: Serialização do board | 🔴 CRÍTICO | int** não é contíguo na memória | Criar flatten/unflatten |
| #3: Output format | 🟡 MÉDIO | Possível incompatibilidade com formato esperado | Rever sudoku_omp.c |

---

## PONTO CRÍTICO #1 — DETECÇÃO DE SUCESSO DO SOLVER

### Análise Detalhada

**Assinatura actual de solve_omp():**
```c
void solve_omp(int **tb, int order)
{
    unsigned long long row_used[63];
    unsigned long long col_used[63];
    unsigned long long box_used[63];
    int found_solution;

    if (board_size(order) >= 64)
    {
        printf("Order too large to solve\n");
        return ;
    }
    if (!init_masks(tb, order, row_used, col_used, box_used))
    {
        printf("Invalid initial board\n");
        return ;
    }
    
    found_solution = 0;
    if (!backtrack_parallel(tb, order, row_used, col_used, box_used, 0, &found_solution))
        printf("Nenhuma solução\n");
}
```

**Observações Críticas:**

1. ✅ `solve_omp()` retorna `void` (não há return value)
2. ✅ Modifica `tb` **in-place** (board é modificado directamente)
3. ✅ Usa `found_solution` flag internamente para terminar cedo
4. ❌ Imprime "Nenhuma solução" se não encontra, mas **também imprime se encontra**

### Problema Principal

```
APÓS chamada solve_omp():
  - Se encontrou solução → tb contém solução, imprime NADA para stdout
  - Se não encontrou → tb contém board parcial, IMPRIME "Nenhuma solução"
  
QUESTÃO: Como saber se encontrou ou não?
  Opção 1: Verificar se "Nenhuma solução" foi impresso? (ERRADO, capturar output é difícil)
  Opção 2: Contar quantas zeros restam no board? (OK mas não é 100% seguro)
  Opção 3: Criar função validate_solution()? (CORRECTO)
```

### Validação do Board

**Estratégia 1: Contar Zeros**
```c
int count_zeros(int **tb, int order)
{
  // Conta quantas células são 0
  // Se retorna 0 → solução encontrada
  // Se retorna > 0 → no solution
}
```

**PROBLEMA**: Se o backtrack encontrou parcial e ficou preso, pode contar zeros incorrectamente.

**Estratégia 2: Validação Completa (RECOMENDADA)**
```c
int is_solved(int **tb, int order)
{
  // Verifica:
  // 1. Nenhuma célula é 0
  // 2. Todas as row constraints OK
  // 3. Todas as column constraints OK
  // 4. Todas as box constraints OK
  
  // Retorna 1 se válido, 0 caso contrário
}
```

### Impacto na Arquitectura

**NO NOVO sudoku_mpi.c:**

```c
// Rank 0:
solve_omp(first_subproblem, order);
if (is_solved(first_subproblem, order))
{
    found_solution = 1;
}

// Rank 1..N:
solve_omp(subproblem, order);
if (is_solved(subproblem, order))
{
    send_solution(subproblem, order);
    break;
}
```

### Solução Recomendada

```c
// ADICIONAR em sudoku.h:
int is_solved(int **tb, int order);

// IMPLEMENTAR em (por exemplo) general_utils/utils.c:
int is_solved(int **tb, int order)
{
    int size = order * order;
    
    // Verificação 1: Nenhum zero
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            if (tb[i][j] == 0)
                return (0);  // Encontrado zero, não está resolvido
        }
    }
    
    // Verificação 2: Validação de constraints
    // (podes usar os masks calculados internamente ou varrer novamente)
    
    return (1);  // Está resolvido
}
```

**Alternativa: Usar backtrack_parallel() como referência**

Notar que `backtrack_parallel()` já valida tudo via masks. Logo, se `tb` tem 0 células zero E `backtrack_parallel()` terminou normalmente, é solução válida.

---

## PONTO CRÍTICO #2 — SERIALIZAÇÃO DOS SUBPROBLEMAS

### Análise Detalhada

**Representação actual do board:**

```c
typedef struct s_sudoku
{
    int **tab;           // ← Pointer to array of pointers
    int order;
}   t_sudoku;
```

**Memory layout:**

```
sudoku->tab:
  ├─ sudoku->tab[0] → malloc'd array [row 0]
  ├─ sudoku->tab[1] → malloc'd array [row 1]
  ├─ sudoku->tab[2] → malloc'd array [row 2]
  └─ ... → malloc'd array [row N-1]

PROBLEMA: Não é contíguo! Cada row é um malloc separado.
```

### Por que é Problema para MPI?

```
MPI_Send() requer buffer contíguo na memória.

Não é possível fazer:
  MPI_Send(board, size*size, MPI_INT, ...)  ← ERRADO!
  
Porquê? Porque board é int**, as arrays não são contíguas.

Solução: FLATTEN em 1D antes de send, UNFLATTEN em 2D após recv.
```

### Algoritmo de Flatten

```c
void flatten_board(int **board, int size, int *buffer)
{
    int idx = 0;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            buffer[idx++] = board[i][j];
        }
    }
}
```

### Algoritmo de Unflatten

```c
int **unflatten_board(int *buffer, int size)
{
    int **board = malloc(sizeof(int *) * size);
    if (!board)
        return NULL;
    
    for (int i = 0; i < size; i++)
    {
        board[i] = malloc(sizeof(int) * size);
        if (!board[i])
        {
            // erro: limpar allocation anterior
            for (int j = 0; j < i; j++)
                free(board[j]);
            free(board);
            return NULL;
        }
        
        for (int j = 0; j < size; j++)
        {
            board[i][j] = buffer[i * size + j];
        }
    }
    
    return board;
}
```

### Impacto na Arquitectura

**FUNCIONARÁ COM:**
```c
// Rank 0: enviar subproblem
int *buffer = malloc(sizeof(int) * size * size);
flatten_board(subproblem, size, buffer);
MPI_Send(buffer, size * size, MPI_INT, dest_rank, TAG_SUBPROBLEM);
free(buffer);

// Rank 1..N: receber subproblem
int *buffer = malloc(sizeof(int) * size * size);
MPI_Recv(buffer, size * size, MPI_INT, 0, TAG_SUBPROBLEM, ...);
int **board = unflatten_board(buffer, size);
free(buffer);
```

### Overhead de Performance

```
Flatten: O(size²) = O(81) ou O(256) — negligenciável
Unflatten: O(size²) + malloc — negligenciável
```

---

## PONTO CRÍTICO #3 — CONFIGURAÇÃO AUTOMÁTICA OPENMP

### Análise Detalhada

**Código proposto na arquitectura:**
```c
void configure_openmp(int rank, int nprocs)
{
    int available = omp_get_num_procs();
    
    if (getenv("OMP_NUM_THREADS"))
    {
        // Usuário definiu, usar como está
    }
    else
    {
        int num_threads = available / nprocs;
        if (num_threads >= 1)
            omp_set_num_threads(num_threads);
    }
}
```

### Validação

**1. Detecção de OMP_NUM_THREADS**
```c
if (getenv("OMP_NUM_THREADS") != NULL)
```
✅ Correcto. `getenv()` retorna NULL se não definida.

**2. Chamada a omp_set_num_threads()**
✅ Correcto. Deve ser chamado no main thread ANTES de criar threads paralelas.

**3. omp_get_num_procs()**
✅ Retorna número de processadores disponíveis.

### Cenário: Oversubscription

```
Exemplo:
  16 CPUs disponíveis
  OMP_NUM_THREADS=8
  mpirun -np 4 ./sudoku_mpi
  
Resultado:
  4 ranks × 8 threads = 32 threads em 16 CPUs
  
Impacto:
  - Context switching excessivo
  - Performance degradada (possível slowdown)
  - Mas NÃO causa erro ou deadlock
```

**Mitigation na arquitectura:**
```
O algoritmo adaptativo de backtrack_parallel() já tem heurística:

  if (level == 0 && num_options >= 2)
      should_parallelize = 1;
  else if (level > 0 && num_options >= 3)
      should_parallelize = 1;

Isto significa: Se há muitos threads e poucos trabalhos, 
a maioria fica idle (não problema grave).
```

### Recomendação

✅ **Arquitectura OK**. Adicionar validação opcional:

```c
void configure_openmp(int rank, int nprocs)
{
    int available = omp_get_num_procs();
    
    if (getenv("OMP_NUM_THREADS"))
    {
        int user_threads = atoi(getenv("OMP_NUM_THREADS"));
        fprintf(stderr, "Rank %d: Using OMP_NUM_THREADS=%d\n", rank, user_threads);
    }
    else
    {
        int num_threads = available / nprocs;
        num_threads = (num_threads >= 1) ? num_threads : 1;
        omp_set_num_threads(num_threads);
        fprintf(stderr, "Rank %d: Auto-configured %d threads\n", rank, num_threads);
    }
}
```

---

## PONTO CRÍTICO #4 — BALANCEAMENTO DE CARGA

### Análise da Estratégia Round-Robin

**Arquitectura proposta:**
```c
for (int i = 0; i < num_subproblems; i++)
{
    int dest_rank = i % nprocs;
    send_subproblem(subproblems[i], dest_rank);
}
```

### Problemas Potenciais

**1. Desequilíbrio em árvore de backtracking**

```
Exemplo:
  Profundidade 2 expansion pode gerar boards com complexidade MUITO diferente
  
  Subproblem 1: Fácil (solução em 0.1s)
  Subproblem 2: Muito difícil (solução em 30s)
  Subproblem 3: Fácil (solução em 0.1s)
  Subproblem 4: Muito difícil (solução em 30s)
  
  Round-robin:
    Rank 0: 1 (0.1s), 3 (0.1s) → total 0.2s
    Rank 1: 2 (30s), 4 (30s) → total 60s
    
  Resultado: Rank 0 ocioso por 59.8s, Rank 1 trabalhando
```

### Probabilidade e Impacto

```
Para 9x9: ~30 subproblemas, baixa probabilidade de desequilíbrio extremo
Para 16x16: ~256 subproblemas (potencialmente muito mais), maior probabilidade

Impacto esperado:
  - 9x9: Baixo (~5-10% degradação)
  - 16x16 difícil: Potencialmente alto (~20-30% degradação)
```

### Soluções Alternativas

**Opção 1: Dynamic Scheduling (Work Stealing)**
```
Complexidade: Alta (requer MPI polling, queue distribuída)
Ganho: +10-20% em 16x16
Recomendação: Não vale a pena para contexto académico
```

**Opção 2: Sorting dos Subproblemas**
```
Complexidade: Média (seriar, ordenar, depois distribuir)
Ganho: +5-15%
Recomendação: Possível melhoria futura
```

**Opção 3: Round-robin com estimativa de dificuldade**
```
Complexidade: Média (contar constraints preenchidas como heurística)
Ganho: +10-15%
Recomendação: Possível optimização
```

### Recomendação FASE 1

```
✅ MANTER round-robin simples por enquanto
   Razões:
   - Fácil de implementar
   - Sem risco de bugs
   - Ganho suficiente para demo académica
   
⏳ FUTURO (FASE 2+): Implementar work-stealing se needed
```

---

## PONTO CRÍTICO #5 — INTEGRAÇÃO COM solve_omp()

### Análise de Reutilização

**Propriedades de solve_omp():**

```c
void solve_omp(int **tb, int order)
```

✅ **Reutilizável?** SIM

| Propriedade | Validação | Impacto |
|------------|-----------|--------|
| Retorna void | ✅ OK (modifica in-place) | Nenhum |
| Variáveis globais | ✅ NENHUMA | Nenhum |
| Estados persistentes | ✅ NENHUM | Nenhum |
| Thread-safe | ✅ SIM (com MPI_THREAD_SERIALIZED) | Nenhum |
| Reutilizável múltiplas vezes | ✅ SIM | Nenhum |
| Pode ser chamado em MPI rank | ✅ SIM | Perfeito |

### Validação Detalhada

**1. Pode ser chamado múltiplas vezes?**
```
SIM. Cada chamada é independente.
Exemplo:
  for (int i = 0; i < 5; i++)
      solve_omp(boards[i], order);  // ← Sem problema
```

**2. Existem variáveis globais?**
```
NÃO. Tudo é local:
  - row_used[], col_used[], box_used[] são locais
  - found_solution é local
  - Sem globals compartilhadas
```

**3. Existem riscos ao chamar em MPI rank?**
```
NÃO, desde que:
  - MPI_Init_thread(..., MPI_THREAD_SERIALIZED)
  - Cada rank chama solve_omp() no seu próprio main thread
  - Não há MPI_Calls dentro da secção paralela OpenMP
  
Isto é GARANTIDO na arquitectura.
```

**4. Existem alterações necessárias em backtracking_omp.c?**
```
NÃO. Nenhuma alteração necessária.
```

### Conclusão Crítica

✅ **solve_omp() é 100% reutilizável conforme está.**

---

## PONTO CRÍTICO #6 — SERIALIZAÇÃO (Continuação)

### Overhead de Comunicação

```
Para 9x9:
  Tamanho do board: 81 ints = 324 bytes
  Overhead MPI: ~1000 bytes (header, metadata)
  Total msg: ~1324 bytes

Para 16x16:
  Tamanho do board: 256 ints = 1024 bytes
  Total msg: ~2024 bytes

Velocidade Ethernet: ~125 MB/s
Tempo transmissão: ~16 microseconds
(negligenciável comparado com tempo de solve ~1s)
```

### Recomendação

✅ **Flatten/unflatten é solução correcta e eficiente.**

---

## PONTO CRÍTICO #7 — GESTÃO DE MEMÓRIA

### Tabela Completa

| Componente | Alocação | Libertação | Dono | Risco |
|-----------|----------|-----------|------|-------|
| `sudoku` (load_sudoku) | load_sudoku() | free_sudoku() | Rank 0 | ✅ Baixo |
| `sudoku->tab` | alloc_grid() | free_sudoku() | Rank 0 | ✅ Baixo |
| `subproblems[]` array | generate_subproblems_mpi() | Rank 0 após uso | Rank 0 | ✅ Baixo |
| `subproblems[i]` (each) | alloc_board() via generate | free_board() antes print | Rank 0 | ✅ Baixo |
| `local_board` (Worker) | unflatten_board() | free_board() após solve | Rank N | ✅ Baixo |
| `recv_buffer` (MPI) | malloc antes recv | free após unflatten | Rank N | ✅ Baixo |
| `send_buffer` (MPI) | malloc antes send | free após send | Rank N | ✅ Baixo |
| OpenMP thread locals | malloc em thread | free em thread | Thread | ✅ OK (private per thread) |

### Double Free Potencial

```
CENÁRIO: Rank 0 liberta subproblems ENQUANTO Rank N usa?

PROTEÇÃO: Sincronização implícita via MPI_Bcast(TERMINATION)
  1. Rank 0 liberta após MPI_Bcast (bloqueante)
  2. Rank N recebe MPI_Bcast (bloqueante)
  3. Sincronização garantida

✅ Sem double free
```

### Memory Leaks Potenciais

```
RISCO 1: Se MPI_Recv falha, buffer não é libertado
MITIGAÇÃO: Adicionar error handling

RISCO 2: Se generate_subproblems_mpi falha a meio
MITIGAÇÃO: Implementar rollback / cleanup

RISCO 3: Se unflatten_board falha a meio da allocação
MITIGAÇÃO: Validar cada malloc e fazer rollback
```

### Recomendação

✅ **Padrão de memória é seguro**

⚠️ **Requer error handling cuidadoso em:**
- generate_subproblems_mpi()
- unflatten_board()
- Durante desalocação em caso de erro

---

## PONTO CRÍTICO #8 — COMPILAÇÃO

### Dependências Actuais

**Makefile actual (MPI target):**
```makefile
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o
	$(MPICC) $(CFLAGS_OPT) $^ -o $@
```

**PROBLEMA CRÍTICO:**
```
❌ Não inclui $(OBJ_DIR)/backtracking_omp.o
❌ sudoku_mpi.c precisa de solve_omp() de backtracking_omp.c
```

### Alteração Obrigatória

```makefile
# ANTES:
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o

# DEPOIS:
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/backtracking_omp.o \  ← ADICIONAR
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o
```

### Headers Necessários

**Em sudoku_mpi.c:**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>          ← Para omp_get_num_procs(), omp_set_num_threads()
#include "sudoku.h"       ← Declarações de funções
```

**Flags de Compilação:**

| Flag | Necessária? | Propósito |
|------|-----------|----------|
| `-Wall -Wextra -Werror` | ✅ SIM | Warnings |
| `-I.` | ✅ SIM | Include paths |
| `-O3` | ✅ SIM | Optimização |
| `-fopenmp` | ✅ SIM | Suporte OpenMP |
| Usar `mpicc` | ✅ SIM | Compile com MPI |

### Validação de Link

```bash
# Comando link esperado:
mpicc -O3 -fopenmp -Wall -Wextra -Werror -I. \
    sudoku_mpi.o \
    backtracking_omp.o \
    get_next_line_mpi.o \
    utils_mpi.o \
    parser_mpi.o \
    loader_mpi.o \
    -o sudoku_mpi.exe

# Verificar symbols:
nm sudoku_mpi.exe | grep solve_omp  ← Deve aparecer
nm sudoku_mpi.exe | grep backtrack  ← Pode aparecer
```

### Lista de Alterações Obrigatórias

```
1. MAKEFILE:
   ├─ Adicionar $(OBJ_DIR)/backtracking_omp.o ao MPI link
   └─ Adicionar rule de compilação se não existir

2. sudoku_mpi.c (a criar):
   ├─ #include "sudoku.h"
   ├─ #include <mpi.h>
   ├─ #include <omp.h>
   ├─ Compilar com mpicc -fopenmp
   └─ Link com backtracking_omp.o
```

---

## ANÁLISE: OUTPUT FORMAT

### Validação Atual

**sudoku_omp.c:**
```c
exec_time = -omp_get_wtime();
solve_omp(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
print(sudoku->tab, sudoku->order);
fprintf(stderr, "%.10f\n", exec_time);
```

**sudoku_serial.c:**
```c
exec_time = -omp_get_wtime();
solve(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
print(sudoku->tab, sudoku->order);
fprintf(stderr, "%.1f\n", exec_time);  ← Diferente (%.1f vs %.10f)!
```

### Problema Potencial

```
OMP usa %.10f (10 casas decimais)
SERIAL usa %.1f (1 casa decimal)

NOVO MPI deve usar qual?
```

### Recomendação

```
USE: %.10f (para consistência com OMP version)

Código:
  exec_time = -omp_get_wtime();
  // ... dispatch, solve, collect ...
  exec_time += omp_get_wtime();
  
  print(solution, order);  // → stdout
  fprintf(stderr, "%.10f\n", exec_time);  // → stderr
```

---

## ALTERAÇÕES OBRIGATÓRIAS RESUMIDAS

### Categoria A: CRÍTICAS (Sem estas, não funciona)

```
1. Criar função is_solved(int **tb, int order)
   Ficheiro: general_utils/utils.c (ou novo ficheiro utils_mpi.c)
   Finalidade: Validar se solução está completa

2. Criar flatten_board(int **board, int size, int *buffer)
   Ficheiro: sudoku_mpi.c
   Finalidade: Converter 2D → 1D para MPI_Send

3. Criar unflatten_board(int *buffer, int size)
   Ficheiro: sudoku_mpi.c
   Finalidade: Converter 1D → 2D após MPI_Recv

4. ATUALIZAR Makefile
   Adicionar $(OBJ_DIR)/backtracking_omp.o ao MPI link
   Sem isto: undefined reference to solve_omp
```

### Categoria B: RECOMENDADAS (Melhor prática)

```
1. Adicionar is_solved() a sudoku.h declarações

2. Adicionar error handling em generate_subproblems_mpi()

3. Adicionar logging/fprintf para debug

4. Adicionar sanity checks nos buffers MPI
```

### Categoria C: OPCIONAIS (Futuro)

```
1. Dynamic load balancing (work stealing)

2. Timeout global

3. Checkpointing

4. Compression de mensagens MPI
```

---

## VALIDAÇÃO FINAL

### Arquitectura Implementável?

| Aspecto | Status | Observações |
|--------|--------|-----------|
| **Decomposição** | ✅ SIM | Round-robin adequado para 9x9 e 16x16 |
| **MPI Communication** | ✅ SIM | Padrão master-worker bem estabelecido |
| **OpenMP Integration** | ✅ SIM | solve_omp() 100% reutilizável |
| **Thread Safety** | ✅ SIM | MPI_THREAD_SERIALIZED suficiente |
| **Memory Management** | ✅ SIM | Com error handling cuidado |
| **I/O Format** | ✅ SIM | Compatível com existing tools |

### solve_omp() Reutilizável?

**SIM, 100% reutilizável** — Desde que:
- ✅ Criar `is_solved()` para detecção de sucesso
- ✅ Usar flatten/unflatten para serialização
- ✅ Chamar em MPI rank com MPI_THREAD_SERIALIZED

### MPI Correcto?

**SIM** — Desde que:
- ✅ Usar polling não-bloqueante (MPI_Iprobe)
- ✅ Broadcast termination correctamente
- ✅ Adicionar backtracking_omp.o ao Makefile

### OpenMP Correcto?

**SIM** — Sem alterações em backtracking_omp.c

### Pronto para Fase 1?

| Pré-requisito | Status | Acção |
|--------------|--------|-------|
| Arquitectura documentada | ✅ COMPLETO | - |
| Problemas críticos identificados | ✅ COMPLETO | 3 problemas |
| Soluções propostas | ✅ COMPLETO | Implementação directa |
| Código existente analisado | ✅ COMPLETO | Sem surpresas |
| Makefile pronto | ⚠️ INCOMPLETO | Adicionar 1 linha |

---

## RECOMENDAÇÃO FINAL

### STATUS: ⚠️ IMPLEMENTÁVEL COM CORREÇÕES

**Arquitectura é SÓ LIGEIRAMENTE alterada:**

1. Adicionar `is_solved()` — 20 linhas
2. Adicionar flatten/unflatten — 40 linhas
3. Atualizar Makefile — 1 linha

**Cronograma FASE 1:**
- Dia 1: Preparar helper functions (is_solved, flatten, unflatten)
- Dia 2: Escrever sudoku_mpi.c estrutura
- Dia 3: Implementar funções MPI
- Dia 4: Testes e debugging

**Risco Global: BAIXO**

---

## ANEXOS

### A. Pseudo-código Completo de is_solved()

```c
int is_solved(int **tb, int order)
{
    int size = order * order;
    
    // Verificação 1: Nenhum zero
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            if (tb[i][j] == 0)
                return (0);  // Incompleto
            if (tb[i][j] < 1 || tb[i][j] > size)
                return (0);  // Inválido
        }
    }
    
    // Verificação 2: Cada número 1..size aparece exactly uma vez per row
    for (int i = 0; i < size; i++)
    {
        unsigned long long seen = 0;
        for (int j = 0; j < size; j++)
        {
            int val = tb[i][j];
            unsigned long long bit = (1ULL << (val - 1));
            if (seen & bit)
                return (0);  // Duplicado na row
            seen |= bit;
        }
    }
    
    // Verificação 3: Cada número 1..size aparece exactly uma vez per column
    for (int j = 0; j < size; j++)
    {
        unsigned long long seen = 0;
        for (int i = 0; i < size; i++)
        {
            int val = tb[i][j];
            unsigned long long bit = (1ULL << (val - 1));
            if (seen & bit)
                return (0);  // Duplicado na col
            seen |= bit;
        }
    }
    
    // Verificação 4: Cada número 1..size aparece exactly uma vez per box
    for (int box_row = 0; box_row < order; box_row++)
    {
        for (int box_col = 0; box_col < order; box_col++)
        {
            unsigned long long seen = 0;
            for (int i = 0; i < order; i++)
            {
                for (int j = 0; j < order; j++)
                {
                    int row = box_row * order + i;
                    int col = box_col * order + j;
                    int val = tb[row][col];
                    unsigned long long bit = (1ULL << (val - 1));
                    if (seen & bit)
                        return (0);  // Duplicado na box
                    seen |= bit;
                }
            }
        }
    }
    
    return (1);  // Válido e completo
}
```

---

**FIM DO RELATÓRIO TÉCNICO FASE 0.5**

**Pronto para Fase 1 após implementar 3 correções críticas.**

