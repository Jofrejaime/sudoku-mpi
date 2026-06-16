# PASSO 5.7 — ANÁLISE DE REFATORIZAÇÃO ANTES DO PASSO 6

**Data**: 2026-06-16  
**Objectivo**: Análise detalhada de dependências de `sudoku_mpi.c` antes de qualquer movimentação  
**Status**: ⚠️ **APENAS ANÁLISE** — NENHUMA ALTERAÇÃO SERÁ FEITA

---

## 1. ESTRUTURA ATUAL COMPLETA DO PROJETO

```
sudoku_mpi/
├── .git/                           # Controlo de versões Git
├── .gitignore                      # Ficheiros ignorados
├── Makefile                        # Build system (serial + OMP + MPI)
│
├── sudoku.h                        # Header principal (13 funções públicas)
│
├── sudoku_serial.c                 # Main serial (42 linhas)
├── sudoku_serial.exe               # Executável serial
│
├── sudoku_omp.c                    # Main OpenMP (42 linhas)
├── sudoku_omp.exe                  # Executável OpenMP
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (1063 linhas) ← ALVO
├── sudoku_mpi.exe                  # Executável MPI+OpenMP
│
├── backtracking/
│   ├── backtracking.c              # Solver serial (227 linhas)
│   └── backtracking_omp.c          # Solver OpenMP paralelo (456 linhas)
│
├── general_utils/
│   ├── utils.c                     # Utilitários partilhados (197 linhas)
│   └── get_next_line.c             # Leitura de ficheiros
│
├── validator/
│   ├── loader.c                    # Carregamento de Sudoku
│   └── parser.c                    # Parsing de ficheiros
│
├── obj/                            # Directório com ficheiros objeto (.o)
│   └── (17 ficheiros .o)
│
├── Sample instances/               # Ficheiros de teste Sudoku
│   └── (5 ficheiros .txt)
│
└── PASSO*.md                       # Documentação do projeto
```

---

## 2. ANÁLISE DE `sudoku_mpi.c`

### 2.1 — Estatísticas Globais


| Métrica | Valor |
|---------|------:|
| **Linhas totais** | **1063** |
| **Funções públicas** | 12 |
| **Funções estáticas** | 2 |
| **Funções total** | 14 |
| **Linhas de comentários** | ~200 |
| **Linhas de código** | ~860 |

### 2.2 — Inventário Completo de Funções

| # | Função | Tipo | Linhas Aprox | Localização |
|---|--------|------|------------:|-------------|
| 1 | `configure_openmp()` | Pública | ~70 | Linhas 95-165 |
| 2 | `flatten_board()` | Pública | ~20 | Linhas 189-210 |
| 3 | `unflatten_board()` | Pública | ~60 | Linhas 235-295 |
| 4 | `send_subproblem()` | Pública | ~40 | Linhas 314-354 |
| 5 | `recv_subproblem()` | Pública | ~120 | Linhas 380-500 |
| 6 | `send_solution()` | Pública | ~40 | Linhas 487-527 |
| 7 | `recv_solution()` | Pública | ~50 | Linhas 545-595 |
| 8 | `find_first_empty()` | **Estática** | ~20 | Linhas 594-615 |
| 9 | `generate_subproblems_mpi()` | Pública | ~80 | Linhas 646-726 |
| 10 | `dispatch_work()` | Pública | ~25 | Linhas 730-755 |
| 11 | `broadcast_termination()` | Pública | ~20 | Linhas 758-778 |
| 12 | `collect_solutions()` | Pública | ~25 | Linhas 788-813 |
| 13 | `worker_loop()` | Pública | ~50 | Linhas 837-887 |
| 14 | `print_usage()` | **Estática** | ~10 | Linhas 887-896 |
| 15 | `main()` | Pública | ~150 | Linhas 909-1059 |

**Total estimado**: ~780 linhas de código efetivo (excluindo comentários extensos)

---

## 3. ANÁLISE DE DEPENDÊNCIAS POR FUNÇÃO

### 3.1 — Análise Detalhada


#### **1. `configure_openmp(int rank, int nprocs)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~938)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM (apenas main)

**Dependências internas**:
- `omp_get_num_procs()` (OpenMP)
- `getenv()` (stdlib)
- `omp_set_num_threads()` (OpenMP)

**Conclusão**: Função exclusiva MPI para configuração híbrida.

---

#### **2. `flatten_board(int **board, int size, int *buffer)`**

**Chamada por**:
- `send_subproblem()` em `sudoku_mpi.c` (linha ~329)
- `send_solution()` em `sudoku_mpi.c` (linha ~502)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM (comunicação MPI)

**Dependências internas**: Nenhuma (loop simples)

**Conclusão**: Função exclusiva MPI para serialização.

---

#### **3. `unflatten_board(int *buffer, int size)`**

**Chamada por**:
- `recv_subproblem()` em `sudoku_mpi.c` (linha ~476)
- `recv_solution()` em `sudoku_mpi.c` (linha ~576)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM (comunicação MPI)

**Dependências internas**:
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para deserialização.

---


#### **4. `send_subproblem(int **board, int order, int dest_rank)`**

**Chamada por**:
- `dispatch_work()` em `sudoku_mpi.c` (linha ~746)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `flatten_board()` (própria)
- `MPI_Send()` (MPI)
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para comunicação.

---

#### **5. `recv_subproblem(int order)`**

**Chamada por**:
- `worker_loop()` em `sudoku_mpi.c` (linha ~852)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `unflatten_board()` (própria)
- `MPI_Iprobe()`, `MPI_Recv()` (MPI)
- `usleep()` (unistd)
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para comunicação com polling.

---

#### **6. `send_solution(int **board, int order)`**

**Chamada por**:
- `worker_loop()` em `sudoku_mpi.c` (linha ~863)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `flatten_board()` (própria)
- `MPI_Send()` (MPI)
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para comunicação.

---


#### **7. `recv_solution(int order)`**

**Chamada por**:
- `collect_solutions()` em `sudoku_mpi.c` (linha ~797)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `unflatten_board()` (própria)
- `MPI_Recv()` (MPI)
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para comunicação.

---

#### **8. `find_first_empty(int **board, int size, int *row, int *col)` [ESTÁTICA]**

**Chamada por**:
- `generate_subproblems_mpi()` em `sudoku_mpi.c` (linha ~657)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**: Nenhuma (loop simples)

**Conclusão**: Função estática exclusiva MPI (helper).

---

#### **9. `generate_subproblems_mpi(...)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~980)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `find_first_empty()` (própria)
- `alloc_board()` (general_utils/utils.c)
- `copy_board()` (general_utils/utils.c)
- `free_board()` (general_utils/utils.c)
- `malloc()` / `free()` (stdlib)

**Conclusão**: Função exclusiva MPI para geração de trabalho.

---


#### **10. `dispatch_work(...)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~994)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `send_subproblem()` (própria)

**Conclusão**: Função exclusiva MPI para distribuição Round-Robin.

---

#### **11. `broadcast_termination(int nprocs)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linhas ~1032, ~1048)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `MPI_Send()` (MPI)

**Conclusão**: Função exclusiva MPI para terminação global.

---

#### **12. `collect_solutions(...)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~1024)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `recv_solution()` (própria)
- `free_board()` (general_utils/utils.c)

**Conclusão**: Função exclusiva MPI para recolha de soluções.

---

#### **13. `worker_loop(int order)`**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~1057)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO
- ❌ `sudoku_omp.c` — NÃO
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**:
- `recv_subproblem()` (própria)
- `solve_omp()` (**backtracking_omp.c** — CRÍTICO)
- `is_solved()` (general_utils/utils.c — CRÍTICO)
- `send_solution()` (própria)
- `free_board()` (general_utils/utils.c)
- `MPI_Comm_rank()` (MPI)

**Conclusão**: Função exclusiva MPI, mas REUTILIZA solver OpenMP existente.

---


#### **14. `print_usage()` [ESTÁTICA]**

**Chamada por**:
- `main()` em `sudoku_mpi.c` (linha ~947)

**Utilizada por**:
- ❌ `sudoku_serial.c` — NÃO (tem sua própria `print_usage()`)
- ❌ `sudoku_omp.c` — NÃO (tem sua própria `print_usage()`)
- ✅ `sudoku_mpi.c` — SIM

**Dependências internas**: `fprintf()` (stdio)

**Conclusão**: Função estática exclusiva, específica de MPI. **NÃO MOVER** (fica em main).

---

#### **15. `main(int argc, char *argv[])`**

**Função principal**: Entry point do executável MPI+OpenMP

**Utilizada por**:
- ❌ Nenhum outro ficheiro (é o entry point)

**Dependências internas**:
- `MPI_Init_thread()`, `MPI_Comm_rank()`, `MPI_Comm_size()`, `MPI_Bcast()`, `MPI_Finalize()`, `MPI_Abort()` (MPI)
- `configure_openmp()` (própria)
- `load_sudoku()` (validator/loader.c)
- `generate_subproblems_mpi()` (própria)
- `dispatch_work()` (própria)
- `solve_omp()` (backtracking_omp.c — CRÍTICO)
- `is_solved()` (general_utils/utils.c — CRÍTICO)
- `alloc_board()`, `copy_board()`, `free_board()` (general_utils/utils.c)
- `collect_solutions()` (própria)
- `broadcast_termination()` (própria)
- `worker_loop()` (própria)
- `print()` (general_utils/utils.c)
- `free_sudoku()` (general_utils/utils.c)

**Conclusão**: Função principal MPI. **NÃO MOVER** (permanece em `sudoku_mpi.c`).

---

## 4. TABELA RESUMO DE DEPENDÊNCIAS

| # | Função | Utilizadores | Serial? | OMP? | Exclusiva MPI? | Pode Mover? |
|---|--------|--------------|:-------:|:----:|:--------------:|:-----------:|
| 1 | `configure_openmp()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 2 | `flatten_board()` | send_subproblem, send_solution | ❌ | ❌ | ✅ | ✅ SIM |
| 3 | `unflatten_board()` | recv_subproblem, recv_solution | ❌ | ❌ | ✅ | ✅ SIM |
| 4 | `send_subproblem()` | dispatch_work | ❌ | ❌ | ✅ | ✅ SIM |
| 5 | `recv_subproblem()` | worker_loop | ❌ | ❌ | ✅ | ✅ SIM |
| 6 | `send_solution()` | worker_loop | ❌ | ❌ | ✅ | ✅ SIM |
| 7 | `recv_solution()` | collect_solutions | ❌ | ❌ | ✅ | ✅ SIM |
| 8 | `find_first_empty()` | generate_subproblems_mpi | ❌ | ❌ | ✅ | ✅ SIM |
| 9 | `generate_subproblems_mpi()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 10 | `dispatch_work()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 11 | `broadcast_termination()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 12 | `collect_solutions()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 13 | `worker_loop()` | main | ❌ | ❌ | ✅ | ✅ SIM |
| 14 | `print_usage()` | main | ❌ | ❌ | ✅ | ❌ NÃO (static) |
| 15 | `main()` | — | ❌ | ❌ | ✅ | ❌ NÃO (entry) |

**CONCLUSÃO**: **13 funções podem ser movidas** (12 públicas + 1 estática helper)

---


## 5. PROPOSTA DE ESTRUTURA FINAL: `mpi/`

### 5.1 — Organização Modular Proposta

Baseado na análise de dependências, propõe-se a seguinte estrutura:

```
sudoku_mpi/
├── .git/
├── .gitignore
├── Makefile                        # ← ALTERADO: adicionar regras mpi/
│
├── sudoku.h                        # Header principal (INALTERADO)
│
├── sudoku_serial.c                 # Main serial (INALTERADO)
├── sudoku_serial.exe               # Executável serial
│
├── sudoku_omp.c                    # Main OpenMP (INALTERADO)
├── sudoku_omp.exe                  # Executável OpenMP
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (~200 linhas) ← REDUZIDO
│   ├── static void print_usage()   # ~10 linhas (mantido)
│   └── int main()                  # ~150 linhas (mantido)
│
├── sudoku_mpi.exe                  # Executável MPI+OpenMP
│
├── backtracking/                   # (INALTERADO)
│   ├── backtracking.c
│   └── backtracking_omp.c
│
├── general_utils/                  # (INALTERADO)
│   ├── utils.c
│   └── get_next_line.c
│
├── validator/                      # (INALTERADO)
│   ├── loader.c
│   └── parser.c
│
├── mpi/                            # ← NOVO DIRETÓRIO
│   │
│   ├── mpi_config.c                # ← NOVO (~70 linhas)
│   │   └── configure_openmp()
│   │
│   ├── mpi_board.c                 # ← NOVO (~100 linhas)
│   │   ├── flatten_board()
│   │   └── unflatten_board()
│   │
│   ├── mpi_comm.c                  # ← NOVO (~270 linhas)
│   │   ├── send_subproblem()
│   │   ├── recv_subproblem()
│   │   ├── send_solution()
│   │   └── recv_solution()
│   │
│   ├── mpi_scheduler.c             # ← NOVO (~200 linhas)
│   │   ├── find_first_empty() [static]
│   │   ├── generate_subproblems_mpi()
│   │   ├── dispatch_work()
│   │   ├── broadcast_termination()
│   │   ├── collect_solutions()
│   │   └── worker_loop()
│   │
│   └── mpi.h                       # ← NOVO HEADER
│       └── (declarações de todas as funções públicas)
│
├── obj/                            # Directório com ficheiros objeto
│   ├── (objectos existentes)
│   ├── mpi_config.o                # ← NOVO
│   ├── mpi_board.o                 # ← NOVO
│   ├── mpi_comm.o                  # ← NOVO
│   └── mpi_scheduler.o             # ← NOVO
│
├── Sample instances/               # (INALTERADO)
└── PASSO*.md                       # Documentação
```

---

### 5.2 — Distribuição de Funções por Módulo

#### **mpi/mpi_config.c** (~70 linhas)

**Responsabilidade**: Configuração híbrida MPI+OpenMP

**Funções**:
1. `configure_openmp(int rank, int nprocs)` — ~70 linhas

**Dependências**:
- OpenMP: `omp_get_num_procs()`, `omp_set_num_threads()`
- stdlib: `getenv()`, `atoi()`
- stdio: `fprintf()`

**Razão**: Isolamento de lógica de configuração de threads

---


#### **mpi/mpi_board.c** (~100 linhas)

**Responsabilidade**: Serialização e deserialização de boards

**Funções**:
1. `flatten_board(int **board, int size, int *buffer)` — ~20 linhas
2. `unflatten_board(int *buffer, int size)` — ~60 linhas

**Dependências**:
- stdlib: `malloc()`, `free()`

**Razão**: Funções de transformação de dados (row-major order)

**IMPORTANTE**: Funções puras, sem estado, testáveis independentemente

---

#### **mpi/mpi_comm.c** (~270 linhas)

**Responsabilidade**: Camada de comunicação MPI (send/recv)

**Funções**:
1. `send_subproblem(int **board, int order, int dest_rank)` — ~40 linhas
2. `recv_subproblem(int order)` — ~120 linhas
3. `send_solution(int **board, int order)` — ~40 linhas
4. `recv_solution(int order)` — ~50 linhas

**Dependências**:
- MPI: `MPI_Send()`, `MPI_Recv()`, `MPI_Iprobe()`, `MPI_Abort()`
- mpi/mpi_board.c: `flatten_board()`, `unflatten_board()`
- stdlib: `malloc()`, `free()`
- unistd: `usleep()`
- stdio: `fprintf()`

**Tags MPI utilizadas**:
- `MPI_TAG_SUBPROBLEM` (101)
- `MPI_TAG_SOLUTION` (102)
- `MPI_TAG_TERMINATION` (103)

**Razão**: Encapsulamento de toda a comunicação MPI com polling

**IMPORTANTE**: 
- `recv_subproblem()` usa polling para evitar deadlock
- Gestão de memória: aloca buffers temporários e liberta após uso

---

#### **mpi/mpi_scheduler.c** (~200 linhas)

**Responsabilidade**: Distribuição de trabalho, scheduling e controlo

**Funções**:
1. `find_first_empty(int **board, int size, int *row, int *col)` — ~20 linhas [**static**]
2. `generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count)` — ~80 linhas
3. `dispatch_work(int ***subproblems, int count, int order, int nprocs)` — ~25 linhas
4. `broadcast_termination(int nprocs)` — ~20 linhas
5. `collect_solutions(int order, int nprocs, int **solution_board)` — ~25 linhas
6. `worker_loop(int order)` — ~50 linhas

**Dependências**:
- mpi/mpi_comm.c: `send_subproblem()`, `recv_subproblem()`, `send_solution()`, `recv_solution()`
- general_utils/utils.c: `alloc_board()`, `copy_board()`, `free_board()`, `is_solved()`
- backtracking_omp.c: `solve_omp()` ← **REUTILIZAÇÃO CRÍTICA**
- MPI: `MPI_Send()`, `MPI_Comm_rank()`
- stdlib: `malloc()`, `free()`
- stdio: `fprintf()`

**Razão**: Lógica de orquestração MPI (geração, distribuição, terminação, recolha)

**IMPORTANTE**: 
- `worker_loop()` REUTILIZA `solve_omp()` existente (NÃO cria solver novo)
- Round-Robin scheduling em `dispatch_work()`
- Terminação global via `broadcast_termination()`

---


#### **mpi/mpi.h** (novo header)

**Conteúdo**:

```c
#ifndef MPI_H
# define MPI_H

# include "sudoku.h"
# include <mpi.h>

/* ========================================================================
 * MPI TAGS
 * ======================================================================== */

# define MPI_TAG_SUBPROBLEM    101
# define MPI_TAG_SOLUTION      102
# define MPI_TAG_TERMINATION   103

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

# define SUBPROBLEM_MAX 1024
# define POLL_INTERVAL 5000  // microseconds

/* ========================================================================
 * DEBUG MACROS
 * ======================================================================== */

# ifdef DEBUG_MPI
#  define DEBUG_LOG(rank, fmt, ...) \
    fprintf(stderr, "[DEBUG Rank %d] " fmt "\n", rank, ##__VA_ARGS__)
# else
#  define DEBUG_LOG(rank, fmt, ...) /* nop */
# endif

/* ========================================================================
 * FUNCTION DECLARATIONS
 * ======================================================================== */

// Configuration (mpi_config.c)
void configure_openmp(int rank, int nprocs);

// Serialization (mpi_board.c)
void flatten_board(int **board, int size, int *buffer);
int **unflatten_board(int *buffer, int size);

// Communication (mpi_comm.c)
void send_subproblem(int **board, int order, int dest_rank);
int **recv_subproblem(int order);
void send_solution(int **board, int order);
int **recv_solution(int order);

// Scheduling (mpi_scheduler.c)
int generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count);
void dispatch_work(int ***subproblems, int count, int order, int nprocs);
void broadcast_termination(int nprocs);
void collect_solutions(int order, int nprocs, int **solution_board);
void worker_loop(int order);

#endif /* MPI_H */
```

**Razão**: Header único para todas as funções MPI, facilita inclusão

---

### 5.3 — `sudoku_mpi.c` APÓS REFATORIZAÇÃO (~200 linhas)

**Conteúdo restante**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "sudoku.h"
#include "mpi/mpi.h"  // ← NOVO INCLUDE

static void print_usage(void)
{
    fprintf(stderr, "\nUSAGE:\n");
    fprintf(stderr, "  mpirun -np <N> sudoku_mpi.exe <ficheiro_sudoku>\n\n");
    fprintf(stderr, "EXAMPLES:\n");
    fprintf(stderr, "  mpirun -np 4 sudoku_mpi.exe \"Sample instances/9x9.txt\"\n");
    fprintf(stderr, "  OMP_NUM_THREADS=2 mpirun -np 4 sudoku_mpi.exe \"Sample instances/16x16.txt\"\n\n");
}

int main(int argc, char *argv[])
{
    // ... (código do main permanece idêntico)
    // Apenas chama funções de mpi/mpi_*.c
}
```

**Redução**: 1063 → ~200 linhas (**-81%**)

---


## 6. ANÁLISE DE IMPACTO E RISCO

### 6.1 — Impacto em Ficheiros Existentes

| Ficheiro | Impacto | Risco | Justificação |
|----------|---------|:-----:|--------------|
| `sudoku_serial.c` | **NENHUM** | ✅ **ZERO** | Não usa funções MPI |
| `sudoku_omp.c` | **NENHUM** | ✅ **ZERO** | Não usa funções MPI |
| `sudoku.h` | **NENHUM** | ✅ **ZERO** | Funções MPI não estão no header público |
| `backtracking.c` | **NENHUM** | ✅ **ZERO** | Independente |
| `backtracking_omp.c` | **NENHUM** | ✅ **ZERO** | Usado por MPI, mas não modificado |
| `general_utils/utils.c` | **NENHUM** | ✅ **ZERO** | Usado por MPI, mas não modificado |
| `validator/` | **NENHUM** | ✅ **ZERO** | Usado por MPI, mas não modificado |
| `Makefile` | **ALTERADO** | ⚠️ **MÉDIO** | Adicionar 4 novas regras de compilação |
| `sudoku_mpi.c` | **ALTERADO** | ⚠️ **MÉDIO** | Reduzir de 1063 → ~200 linhas |

**CONCLUSÃO**: ✅ **Refatorização segura** — nenhum ficheiro de solver ou utilitário é afetado

---

### 6.2 — Alterações Necessárias no Makefile

**Adições necessárias**:

```makefile
# ============================================
# MPI UTILS COMPILATION
# ============================================

$(OBJ_DIR)/mpi_config.o: mpi/mpi_config.c mpi/mpi.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/mpi_board.o: mpi/mpi_board.c mpi/mpi.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/mpi_comm.o: mpi/mpi_comm.c mpi/mpi.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/mpi_scheduler.o: mpi/mpi_scheduler.c mpi/mpi.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@
```

**Modificação na target MPI**:

```makefile
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/mpi_config.o \
             $(OBJ_DIR)/mpi_board.o \
             $(OBJ_DIR)/mpi_comm.o \
             $(OBJ_DIR)/mpi_scheduler.o \
             $(OBJ_DIR)/backtracking_omp.o \
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o
	$(MPICC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ MPI version: $(MPI_NAME)"
```

**Complexidade**: Média (4 novas regras + 1 modificação)

---

### 6.3 — Riscos Identificados

| Risco | Probabilidade | Impacto | Mitigação |
|-------|:-------------:|:-------:|-----------|
| Erro ao mover código | Baixa | Médio | Testes de regressão após cada módulo |
| Erro em protótipos | Baixa | Alto | Compilação incremental, verificar headers |
| Erro em includes | Média | Médio | Verificar `#include "mpi/mpi.h"` em todos os ficheiros |
| Dependência circular | Baixa | Alto | Análise de dependências já realizada |
| Quebra funcional MPI | Baixa | Alto | Testes com múltiplos processos (1, 2, 4, 8) |
| Performance degradada | Muito Baixa | Baixo | Nenhuma alteração funcional, apenas organizacional |

**RISCO GLOBAL**: ⚠️ **MÉDIO** (controlável com abordagem incremental)

---


## 7. ESTRATÉGIA DE IMPLEMENTAÇÃO

### 7.1 — Abordagem Incremental (Recomendada)

**FASE 1**: Criar estrutura `mpi/` e header

```
✓ Criar diretório mpi/
✓ Criar mpi/mpi.h com todas as declarações
✓ Copiar constantes (#define) para o header
✓ Compilar: verificar que nada quebrou (ainda não move código)
```

**FASE 2**: Mover `mpi_board.c` (mais simples, sem dependências internas)

```
✓ Criar mpi/mpi_board.c
✓ Mover flatten_board() e unflatten_board()
✓ Adicionar regra no Makefile
✓ Modificar sudoku_mpi.c: adicionar #include "mpi/mpi.h"
✓ Remover funções movidas de sudoku_mpi.c
✓ Compilar e testar
```

**FASE 3**: Mover `mpi_config.c` (independente)

```
✓ Criar mpi/mpi_config.c
✓ Mover configure_openmp()
✓ Adicionar regra no Makefile
✓ Remover função movida de sudoku_mpi.c
✓ Compilar e testar
```

**FASE 4**: Mover `mpi_comm.c` (depende de mpi_board)

```
✓ Criar mpi/mpi_comm.c
✓ Mover send_subproblem(), recv_subproblem(), send_solution(), recv_solution()
✓ Adicionar regra no Makefile
✓ Remover funções movidas de sudoku_mpi.c
✓ Compilar e testar
```

**FASE 5**: Mover `mpi_scheduler.c` (depende de mpi_comm)

```
✓ Criar mpi/mpi_scheduler.c
✓ Mover find_first_empty(), generate_subproblems_mpi(), dispatch_work(), 
        broadcast_termination(), collect_solutions(), worker_loop()
✓ Adicionar regra no Makefile
✓ Remover funções movidas de sudoku_mpi.c
✓ Compilar e testar
```

**FASE 6**: Testes finais de regressão

```
✓ Compilar serial, OMP, MPI
✓ Testar serial com maps existentes
✓ Testar OMP com maps existentes
✓ Testar MPI com 1, 2, 4, 8 processos
✓ Verificar outputs idênticos
✓ Verificar sem warnings (-Wall -Wextra)
```

---

### 7.2 — Critérios de Aprovação

A refatorização é considerada **APROVADA** quando:

✅ **Compilação limpa**: 0 errors, 0 warnings  
✅ **Funcionalidade preservada**: Todos os testes passam  
✅ **Serial inalterado**: `sudoku_serial.exe` funciona identicamente  
✅ **OMP inalterado**: `sudoku_omp.exe` funciona identicamente  
✅ **MPI funcional**: `sudoku_mpi.exe` produz resultados corretos  
✅ **Estrutura modular**: `sudoku_mpi.c` reduzido para ~200 linhas  
✅ **Headers corretos**: `mpi/mpi.h` com todas as declarações  

---


## 8. RESUMO EXECUTIVO

### 8.1 — Números da Refatorização

| Métrica | Antes | Depois | Variação |
|---------|------:|-------:|---------:|
| **sudoku_mpi.c** | 1063 linhas | ~200 linhas | **-81%** |
| **Ficheiros MPI** | 1 ficheiro | 5 ficheiros | +4 |
| **Funções movidas** | — | 13 funções | — |
| **Módulos criados** | — | 4 módulos | — |
| **Headers criados** | — | 1 header | — |
| **Linhas por módulo** | 1063 | <100 cada | **Modular** |

---

### 8.2 — Vantagens da Refatorização

✅ **Modularidade**: Cada módulo tem responsabilidade única  
✅ **Testabilidade**: Módulos podem ser testados independentemente  
✅ **Manutenibilidade**: Código mais fácil de ler e modificar  
✅ **Reutilização**: Funções de comunicação podem ser reutilizadas  
✅ **Clareza**: `sudoku_mpi.c` focado apenas em orquestração de alto nível  
✅ **Organização**: Estrutura similar a projetos profissionais  

---

### 8.3 — Desvantagens da Refatorização

⚠️ **Complexidade inicial**: 4 novos ficheiros para criar e testar  
⚠️ **Alterações no Makefile**: Requer adicionar 4 novas regras  
⚠️ **Risco de erro**: Movimentação de código pode introduzir bugs  
⚠️ **Tempo**: Processo incremental requer várias compilações e testes  

---

### 8.4 — Alternativa: Status Quo

Se o objetivo é **PASSO 6 (Benchmark)** imediatamente:

✅ **OPÇÃO ALTERNATIVA**: Manter estrutura atual (1063 linhas)

**Vantagens**:
- Zero risco de quebra funcional
- Código já validado e testado
- Pronto para benchmark imediatamente
- Nenhuma alteração no Makefile

**Desvantagens**:
- `sudoku_mpi.c` permanece grande
- Menos modular
- Mais difícil de testar individualmente

---

## 9. RECOMENDAÇÃO FINAL

### 9.1 — Para PASSO 6 (Benchmark Imediato)

**Recomendação**: ⏸️ **ADIAR REFATORIZAÇÃO**

**Razão**:
1. Código atual está funcional e validado (PASSO 5.5 cleanup completo)
2. Benchmark requer baseline estável
3. Refatorização pode ser feita **APÓS** ter dados de performance
4. Prioridade: obter métricas (speedup, eficiência) primeiro

**Próximo passo**: Ir diretamente para **PASSO 6 — Benchmark e Análise de Desempenho**

---

### 9.2 — Para Modularização Futura

**Recomendação**: ✅ **USAR ESTRUTURA `mpi/` PROPOSTA**

**Razão**:
1. Análise de dependências completa
2. Nenhum impacto em `sudoku_serial.c` ou `sudoku_omp.c`
3. Risco controlado com abordagem incremental
4. Benefícios de longo prazo (manutenibilidade, testabilidade)

**Quando**: Após PASSO 6 (quando tiver baseline de performance)

---

## 10. CONCLUSÃO

### ✅ ANÁLISE COMPLETA CONCLUÍDA

**Realizado**:
- ✓ Estrutura atual completa do projeto documentada
- ✓ Número de linhas de `sudoku_mpi.c`: **1063 linhas**
- ✓ Inventário de todas as 15 funções em `sudoku_mpi.c`
- ✓ Análise detalhada de dependências para cada função
- ✓ Tabela de utilizadores (Serial? OMP? MPI?)
- ✓ Identificação de **13 funções movíveis** (exclusivas MPI)
- ✓ Proposta de estrutura `mpi/` com 4 módulos
- ✓ Distribuição exata de funções por módulo
- ✓ Análise de impacto e risco
- ✓ Estratégia de implementação incremental

**Próxima decisão**:
1. **OPÇÃO A**: Ir para PASSO 6 (Benchmark) com estrutura atual → Risco ZERO
2. **OPÇÃO B**: Executar refatorização `mpi/` agora → Risco MÉDIO, benefícios futuros

---

**STATUS**: ✓ **RELATÓRIO DE ANÁLISE COMPLETO**  
**PRÓXIMO PASSO**: Aguardar decisão do utilizador

