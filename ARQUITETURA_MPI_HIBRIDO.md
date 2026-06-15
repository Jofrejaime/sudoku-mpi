# ARQUITECTURA TÉCNICA: SUDOKU MPI + OPENMP HÍBRIDO
## FASE 0 — Especificação Detalhada (SEM CÓDIGO)

---

## ÍNDICE

1. [Análise de Funções Existentes](#análise-de-funções-existentes)
2. [Mapa da Arquitectura](#mapa-da-arquitectura)
3. [Estratégia de Decomposição](#estratégia-de-decomposição)
4. [Arquitectura MPI](#arquitectura-mpi)
5. [Integração OpenMP](#integração-openmp)
6. [Terminação Global](#terminação-global)
7. [Configuração de Threads](#configuração-de-threads)
8. [Gestão de Memória](#gestão-de-memória)
9. [Funções a Reutilizar](#funções-a-reutilizar)
10. [Funções MPI Novas](#funções-mpi-novas)
11. [Estrutura do Novo sudoku_mpi.c](#estrutura-do-novo-sudoku_mpic)
12. [Fluxogramas](#fluxogramas)
13. [Validação Arquitectónica](#validação-arquitectónica)

---

## 1. ANÁLISE DE FUNÇÕES EXISTENTES

### 1.1 Funções em `sudoku.h` (Interface Global)

```
STRUCT:
  t_sudoku
  ├─ int **tab         (2D board)
  └─ int order         (9 para 9x9, 4 para 16x16)

DECLARAÇÕES:
  ├─ char *get_next_line(int fd)
  ├─ t_sudoku *load_sudoku(const char *path)
  ├─ void free_sudoku(t_sudoku *sudoku)
  ├─ int parser_parse_order_line(const char *line, int *order)
  ├─ int parser_compute_grid_size(int order, int *size)
  ├─ int parser_parse_board_line(const char *line, int *row, int size)
  ├─ void print(int **tb, int order)
  ├─ void solve(int **tb, int order)        ← SERIAL
  └─ void solve_omp(int **tb, int order)    ← OpenMP [REUTILIZAR]
```

### 1.2 Funções em `sudoku_omp.c` (Wrapper OpenMP)

```
main()
  │
  ├─ load_sudoku(argv[1])           [validator/loader.c]
  │                                  └─ RETORNA t_sudoku *
  │
  ├─ exec_time = -omp_get_wtime()
  │
  ├─ solve_omp(sudoku->tab, sudoku->order)   [backtracking/backtracking_omp.c]
  │                                            └─ ENTRY POINT PARALELIZADA
  │
  ├─ exec_time += omp_get_wtime()
  │
  ├─ print(sudoku->tab, sudoku->order)      [general_utils/utils.c]
  │
  ├─ fprintf(stderr, "%.10f\n", exec_time)
  │
  └─ free_sudoku(sudoku)                    [general_utils/utils.c]
```

**PADRÃO**: Load → Solve → Print → Time → Free

### 1.3 Funções em `sudoku_serial.c` (Wrapper Serial)

```
main()
  │
  ├─ load_sudoku(argv[1])          [validator/loader.c]
  │
  ├─ solve(sudoku->tab, sudoku->order)      [backtracking/backtracking.c]
  │
  ├─ print(sudoku->tab, sudoku->order)      [general_utils/utils.c]
  │
  ├─ fprintf(stderr, "%.1f\n", exec_time)   [Note: %.1f vs %.10f]
  │
  └─ free_sudoku(sudoku)                    [general_utils/utils.c]
```

### 1.4 Funções em `backtracking_omp.c` (Solver OpenMP)

| Função | Tipo | Linhas | Propósito | REUTILIZAR? |
|--------|------|--------|-----------|-------------|
| `board_size(int order)` | Static | - | `order * order` | ✅ Indirectamente (dentro solve_omp) |
| `full_mask(int size)` | Static | - | Máscara de bits completa | ✅ Indirectamente |
| `box_index(int order, int row, int col)` | Static | - | Índice da box Sudoku | ✅ Indirectamente |
| `count_bits(mask)` | Static | - | Conta bits em máscara | ✅ Indirectamente |
| `init_masks(...)` | Static | 31-86 | Inicializa row/col/box masks | ✅ Via solve_omp() |
| `possible_mask(...)` | Static | - | Calcula possibilidades para célula | ✅ Indirectamente |
| `find_best_cell(...)` | Static | 93-139 | MRV heurística, retorna melhor célula | ✅ Via solve_omp() |
| `backtrack_parallel(...)` | Static | 141-328 | **CORE: backtracking com #pragma omp parallel for** | ✅ Via solve_omp() |
| `backtrack_sequential(...)` | Static | 331-365 | Backtracking sequencial (folhas) | ✅ Via solve_omp() |
| **`solve_omp(int **tb, int order)`** | Public | 379-402 | **ENTRY POINT — ORQUESTRADOR** | ✅ **CHAMAR DIRECTAMENTE** |

### 1.5 Funções em `validator/loader.c` (I/O)

| Função | Tipo | Propósito | REUTILIZAR? |
|--------|------|-----------|-------------|
| `alloc_grid(int size)` | Static | Aloca `int **tab` | ✅ Via `load_sudoku()` |
| `free_tab(int **tab, int size)` | Static | Liberta `int **tab` | ✅ Via `free_sudoku()` |
| `read_header(int fd, int *order, int *size)` | Static | Parser primeira linha | ✅ Via `load_sudoku()` |
| `read_grid(int fd, int **tab, int size)` | Static | Parser board | ✅ Via `load_sudoku()` |
| **`load_sudoku(const char *path)`** | Public | Carrega ficheiro completo | ✅ **CHAMAR DIRECTAMENTE** |

### 1.6 Funções em `general_utils/utils.c` (Utilities)

| Função | Tipo | Propósito | REUTILIZAR? |
|--------|------|-----------|-------------|
| **`print(int **tb, int order)`** | Public | Imprime board para stdout | ✅ **CHAMAR DIRECTAMENTE** |
| **`free_sudoku(t_sudoku *sudoku)`** | Public | Liberta struct sudoku | ✅ **CHAMAR DIRECTAMENTE** |

### 1.7 Funções em `validator/parser.c` (Parser)

| Função | Tipo | Propósito | REUTILIZAR? |
|--------|------|-----------|-------------|
| `parser_parse_order_line(const char *line, int *order)` | Public | Extrai order da linha 1 | ✅ Via `load_sudoku()` |
| `parser_compute_grid_size(int order, int *size)` | Public | Calcula size | ✅ Via `load_sudoku()` |
| `parser_parse_board_line(const char *line, int *row, int size)` | Public | Parser uma linha do board | ✅ Via `load_sudoku()` |

### 1.8 Funções em `general_utils/get_next_line.c` (I/O)

| Função | Tipo | Propósito | REUTILIZAR? |
|--------|------|-----------|-------------|
| `get_next_line(int fd)` | Public | Lê linha de ficheiro | ✅ Via `load_sudoku()` |

---

## 2. MAPA DA ARQUITECTURA

### 2.1 Hierarquia de Parallelismo (Visão Alto Nível)

```
┌─────────────────────────────────────────────────────┐
│           PARALLELISMO SERIAL (Sequencial)           │
│  sudoku_serial.c → solve() → backtracking.c         │
│  1 thread, algoritmo não-paralelizado               │
└─────────────────────────────────────────────────────┘
                        ↓ evolução
┌─────────────────────────────────────────────────────┐
│       PARALLELISMO OpenMP (1 máquina, múltiplas CPU) │
│  sudoku_omp.c → solve_omp() → backtracking_omp.c    │
│  OpenMP threads, algoritmo adaptativo paralelizado  │
│  1 Master + (N-1) OpenMP threads                    │
└─────────────────────────────────────────────────────┘
                        ↓ evolução (NOVO)
┌─────────────────────────────────────────────────────┐
│  PARALLELISMO HÍBRIDO (MPI + OpenMP, múltiplas CPU) │
│  sudoku_mpi.c (NOVO) → MPI distribui tarefas        │
│  Cada rank: solve_omp() → backtracking_omp.c        │
│  1 Master rank + (N-1) Worker ranks                 │
│  Cada rank: OpenMP threads                          │
└─────────────────────────────────────────────────────┘
```

### 2.2 Componentes da Arquitectura MPI Híbrida

```
NOVO sudoku_mpi.c = Orchestrador MPI
       │
       ├─ MPI_Init_thread()
       │  └─ Modo: MPI_THREAD_SERIALIZED
       │
       ├─ RANK 0 (Master)
       │  ├─ load_sudoku()        [REUTILIZAR validator/loader.c]
       │  ├─ generate_subproblems_mpi()   [NOVO — MPI local]
       │  ├─ dispatch_work()              [NOVO — MPI broadcast]
       │  ├─ collect_solutions()          [NOVO — MPI polls]
       │  ├─ print()                      [REUTILIZAR general_utils/utils.c]
       │  └─ time + free
       │
       ├─ RANKS 1..N (Workers)
       │  ├─ recv_subproblem()    [NOVO — MPI recv]
       │  ├─ solve_omp()          [REUTILIZAR backtracking_omp.c]
       │  │  ├─ OpenMP threads
       │  │  ├─ backtrack_parallel()
       │  │  └─ backtrack_sequential()
       │  ├─ send_solution()      [NOVO — MPI send]
       │  └─ wait_termination()   [NOVO — MPI poll]
       │
       └─ MPI_Finalize()
```

### 2.3 Fluxo de Dados

```
Ficheiro sudoku.txt
       ↓
load_sudoku()  [validator/loader.c]
       ↓
t_sudoku struct (int **tab, int order)
       ↓ (Rank 0)
generate_subproblems_mpi()  [NOVO]
       │ Expande árvore até depth 2
       │ Produz 32 subproblemas (exemplo)
       ↓
MPI_Bcast(order, grid_size)
       ├─ Todos ranks recebem parameters
       ↓
MPI_Send(subproblema) → Rank 1
MPI_Send(subproblema) → Rank 2
...
MPI_Send(subproblema) → Rank N
       ↓
CADA RANK:
  recv_subproblem()  [NOVO]
       ↓
  solve_omp()  [backtracking_omp.c]
       │ Com OpenMP threads
       ↓
  IF solução encontrada:
       ├─ MPI_Send(solução) → Rank 0
       └─ EXIT
  ELSE:
       ├─ poll termination signal
       └─ EXIT
       ↓
RANK 0:
  poll MPI_Iprobe()
       ├─ IF solução recebida:
       │  ├─ MPI_Bcast(STOP)
       │  ├─ print()
       │  └─ EXIT
       └─ ELSE: timeout, nenhuma solução
            └─ EXIT "Nenhuma solução"
```

---

## 3. ESTRATÉGIA DE DECOMPOSIÇÃO

### 3.1 Problema: Gerar Subproblemas Iniciais

**Contexto**: Uma árvore de backtracking tem ~10^60 nós para 9x9 fácil.
Seria ineficiente abrir toda a árvore e distribuir.

**Solução**: Expandir árvore até profundidade controlada (depth 0-2),
colectar todos os boards nesse nível, distribuir entre ranks.

### 3.2 Algoritmo de Geração

```
generate_subproblems_mpi(int **board, int order, depth_limit=2)

ENTRADA:  board inicial (parcialmente preenchido)
SAÍDA:    vetor de subproblemas (cada um é um int **board)
RETORNA:  número de subproblemas gerados

PSEUDOCÓDIGO:
  1. Iniciar com board original
  2. Chamar backtrack_gerar_subs(board, depth=0, depth_limit=2)
  3. Cada vez que depth == depth_limit:
     - Copiar board actual
     - Adicionar à lista de subproblemas
     - Parar recursão nesse ramo
  4. Retornar lista de subproblemas
```

### 3.3 Exemplo Concreto: 9x9

```
CENÁRIO:
  - Board inicial: ~17 células preenchidas (dificuldade fácil)
  - depth_limit = 2

RESULTADO:
  - Profundidade 0: 1 board (original)
  - Profundidade 1: ~9 boards (9 primeiras escolhas possíveis)
  - Profundidade 2: ~81 boards (81 segundas escolhas)
  
TOTAL GERADO: ~32 subproblemas (ordem de magnitude)

DISTRIBUIÇÃO (Exemplo 4 ranks):
  Rank 0: subproblemas 0, 4, 8, 12, 16, 20, 24, 28
  Rank 1: subproblemas 1, 5, 9, 13, 17, 21, 25, 29
  Rank 2: subproblemas 2, 6, 10, 14, 18, 22, 26, 30
  Rank 3: subproblemas 3, 7, 11, 15, 19, 23, 27, 31
```

### 3.4 Balanceamento de Carga

**Problema**: Alguns subproblemas podem ser muito mais difíceis que outros.

**Solução 1**: Round-robin (simples, razoável)
```
FOR i = 0 TO num_subproblems-1:
  rank = i % num_ranks
  enviar subproblema i para rank
```

**Solução 2**: Work-stealing (complexo, melhor)
```
Não implementar AGORA. Manter simples round-robin.
```

### 3.5 Parametrização

```
#define SUBPROBLEM_DEPTH_LIMIT 2
→ Controla quantos subproblemas gerar

#define SUBPROBLEM_MAX 256
→ Limite máximo de subproblemas (segurança)
```

**Ajustes por problema**:
- 9x9 fácil: depth_limit = 2, ~30 subproblemas
- 9x9 difícil: depth_limit = 3, ~300 subproblemas
- 16x16 fácil: depth_limit = 1, ~16 subproblemas (muito poucos!)
- 16x16 difícil: depth_limit = 2, ~256 subproblemas

---

## 4. ARQUITECTURA MPI

### 4.1 Estrutura de Mensagens

```
MENSAGEM 1: Broadcast Parâmetros (Rank 0 → ALL)
├─ Tag: MPI_TAG_INIT
├─ Conteúdo:
│  ├─ order (int)
│  ├─ grid_size = order * order (int)
│  └─ num_subproblems (int)
└─ Tipo: MPI_INT

MENSAGEM 2: Send Subproblema (Rank 0 → Rank N)
├─ Tag: MPI_TAG_SUBPROBLEM
├─ Conteúdo:
│  ├─ subproblema_id (int)
│  ├─ board_data (grid_size * grid_size ints)
│  └─ Serializado em 1D: [board[0][0], board[0][1], ..., board[size-1][size-1]]
└─ Tipo: MPI_INT

MENSAGEM 3: Send Solução (Rank N → Rank 0)
├─ Tag: MPI_TAG_SOLUTION
├─ Conteúdo:
│  ├─ subproblema_id (int)
│  ├─ solution_board (grid_size * grid_size ints)
│  └─ Serializado em 1D
├─ Tipo: MPI_INT
└─ Nota: Enviada apenas se found

MENSAGEM 4: Broadcast Terminação (Rank 0 → ALL)
├─ Tag: MPI_TAG_TERMINATION
├─ Conteúdo:
│  ├─ terminate_flag (int) = 1
│  └─ solution_rank (int) = qual rank encontrou (ou -1)
├─ Tipo: MPI_INT
└─ Efeito: Todos os ranks saem do loop
```

### 4.2 Tags MPI Definidas

```c
// sudoku_mpi.c

#define MPI_TAG_INIT          100
#define MPI_TAG_SUBPROBLEM    101
#define MPI_TAG_SOLUTION      102
#define MPI_TAG_TERMINATION   103
#define MPI_TAG_ALLREDUCE     104
```

### 4.3 Operações MPI Utilizadas

| Operação | Localização | Propósito |
|----------|------------|-----------|
| `MPI_Init_thread()` | main() | Inicializa MPI com thread support |
| `MPI_Comm_size()` | main() | Obtém número total de ranks |
| `MPI_Comm_rank()` | main() | Obtém rank actual |
| `MPI_Bcast()` | dispatch_work() | Broadcast de (order, grid_size, num_subproblems) |
| `MPI_Send()` | dispatch_work() | Envia subproblema para cada rank |
| `MPI_Recv()` | worker_loop() | Recebe subproblema |
| `MPI_Iprobe()` | collect_solutions() | Polling não-bloqueante (existe msg?) |
| `MPI_Recv()` | collect_solutions() | Recebe solução |
| `MPI_Bcast()` | collect_solutions() | Broadcast stop signal |
| `MPI_Allreduce()` | shutdown_cluster() | Confirmação global (opcional) |
| `MPI_Finalize()` | main() | Finaliza MPI |

### 4.4 Comportamento de Rank 0 (Master)

```
RANK 0 FLOW:

1. MPI_Comm_rank(), MPI_Comm_size()
2. load_sudoku(argv[1])
   ├─ Carrega ficheiro
   ├─ Valida board
   └─ Retorna t_sudoku *

3. configure_openmp()
   ├─ Decide num_threads para Rank 0
   ├─ omp_set_num_threads()
   └─ Log: "Rank 0: N threads"

4. generate_subproblems_mpi()
   ├─ Expande árvore até depth 2
   ├─ Produz ~30 subproblemas
   └─ Retorna subproblems[], count

5. dispatch_work()
   ├─ MPI_Bcast(order, grid_size, num_subproblems)
   ├─ FOR each subproblem:
   │  ├─ Serialize subproblem para 1D array
   │  ├─ MPI_Send(array, grid_size*grid_size, MPI_INT, rank, TAG_SUBPROBLEM)
   │  └─ rank = (i % nprocs) para round-robin
   └─ Log: "Dispatched N subproblems to N ranks"

6. solve_omp(first_subproblem, order)
   ├─ Chama solver OpenMP existente
   ├─ Se encontra solução:
   │  ├─ found_solution = 1
   │  ├─ salvar solução
   │  └─ goto collect_solutions()
   └─ Se não:
        ├─ found_solution = 0
        └─ continue polling

7. collect_solutions() loop:
   ├─ ENQUANTO NOT found_solution:
   │  ├─ FOR each rank 1..nprocs-1:
   │  │  ├─ MPI_Iprobe(rank, TAG_SOLUTION, &flag)
   │  │  ├─ IF flag:
   │  │  │  ├─ MPI_Recv(solution_buffer)
   │  │  │  ├─ found_solution = 1
   │  │  │  ├─ Deserialize para 2D
   │  │  │  └─ BREAK
   │  │  └─ ENDIF
   │  └─ ENDFOR
   │
   │  ├─ IF found_solution:
   │  │  ├─ MPI_Bcast(TERMINATION, stop_flag=1)
   │  │  └─ print(solution) → stdout
   │  │
   │  └─ ELSE:
   │     ├─ sleep(POLL_INTERVAL) ou spin
   │     └─ continue
   │
   └─ ENDWHILE

8. fprintf(stderr, "%.10f\n", exec_time)

9. free memory, MPI_Finalize()
```

### 4.5 Comportamento de Ranks 1..N (Workers)

```
RANK > 0 FLOW:

1. MPI_Comm_rank(), MPI_Comm_size()

2. MPI_Bcast(order, grid_size, num_subproblems)
   ├─ Recebe parâmetros do Rank 0
   └─ Log: "Rank N: order=%d, grid_size=%d", order, grid_size

3. configure_openmp()
   ├─ Decide num_threads para este rank
   ├─ omp_set_num_threads()
   └─ Log: "Rank N: M threads"

4. worker_loop():
   ├─ ENQUANTO TRUE:
   │  ├─ recv_subproblem()
   │  │  ├─ MPI_Recv(buffer, grid_size*grid_size, MPI_INT, 0, TAG_SUBPROBLEM)
   │  │  ├─ Deserialize para 2D array
   │  │  └─ Log: "Rank N received subproblem ID"
   │  │
   │  ├─ solve_omp(subproblema, order)
   │  │  ├─ Chama solver OpenMP existente
   │  │  ├─ OpenMP cria threads
   │  │  ├─ backtrack_parallel() / backtrack_sequential()
   │  │  └─ REUTILIZA backtracking_omp.c 100%
   │  │
   │  ├─ IF solução encontrada em solve_omp():
   │  │  ├─ send_solution()
   │  │  │  ├─ Serialize subproblem para 1D
   │  │  │  ├─ MPI_Send(buffer, grid_size*grid_size, MPI_INT, 0, TAG_SOLUTION)
   │  │  │  └─ Log: "Rank N sent solution"
   │  │  │
   │  │  └─ BREAK (sai loop)
   │  │
   │  └─ ELSE:
   │     ├─ Log: "Rank N: no solution for this subproblem"
   │     └─ Continue loop para próximo subproblem
   │
   ├─ wait_termination()
   │  ├─ ENQUANTO TRUE:
   │  │  ├─ MPI_Iprobe(0, TAG_TERMINATION, &flag)
   │  │  ├─ IF flag:
   │  │  │  ├─ MPI_Recv(termination_msg)
   │  │  │  ├─ Log: "Rank N: termination signal received"
   │  │  │  └─ BREAK
   │  │  └─ ELSE:
   │  │     ├─ sleep(POLL_INTERVAL)
   │  │     └─ continue
   │  └─ ENDWHILE
   │
   └─ ENDWHILE

5. free memory, MPI_Finalize()
```

---

## 5. INTEGRAÇÃO OPENMP

### 5.1 Onde OpenMP é Chamado

```
RANK 0:
  ├─ generate_subproblems_mpi() [sequencial, local]
  │  └─ Não usa OpenMP aqui
  │
  ├─ solve_omp(first_subproblem)  [✓ OPENMP]
  │  ├─ init_masks() [serial init]
  │  └─ backtrack_parallel() [#pragma omp parallel for]
  │     ├─ #pragma omp parallel for (diferentes threads por ramo)
  │     ├─ Cada thread: malloc copies de board
  │     ├─ backtrack_sequential() [exploração sequencial]
  │     └─ #pragma omp critical (escrita resultado compartilhado)
  │
  └─ collect_solutions() [sequencial, polling]

RANKS 1..N:
  ├─ recv_subproblem() [sequencial]
  │
  ├─ solve_omp(subproblem)  [✓ OPENMP]
  │  ├─ init_masks() [serial init]
  │  └─ backtrack_parallel() [#pragma omp parallel for]
  │     ├─ #pragma omp parallel for
  │     ├─ Cada thread: malloc copies
  │     ├─ backtrack_sequential()
  │     └─ #pragma omp critical
  │
  └─ send_solution() [sequencial]
```

### 5.2 Estrutura de Execução Paralela

```
Exemplo: 4 MPI ranks, 2 OpenMP threads por rank

RANK 0           RANK 1           RANK 2           RANK 3
│                │                │                │
├─ solve_omp()   ├─ solve_omp()    ├─ solve_omp()    ├─ solve_omp()
│  Thread0 Thread1 Thread0 Thread1 Thread0 Thread1  Thread0 Thread1
│  │      │       │      │       │      │       │      │
├─ back  back    ├─ back  back    ├─ back  back     ├─ back  back
│  track parallel  track parallel  track parallel   track parallel
│  │              │              │              │
└─ result         └─ result       └─ result      └─ result
     │                 │              │             │
     └─────────────────┴──────────────┴─────────────┘
                       │
            Se algum encontra solução
            │
            MPI_Send → Rank 0
            │
            Rank 0 broadcasts STOP
            │
            Todos saem
```

### 5.3 Configuração de Threads

```
MODO MANUAL:

$ OMP_NUM_THREADS=4 mpirun -np 2 ./sudoku_mpi.exe Sample\ instances/9x9.txt
  ├─ 2 MPI ranks
  ├─ 4 OpenMP threads por rank
  └─ Total: 2 × 4 = 8 threads paralelas


MODO AUTOMÁTICO:

Inside sudoku_mpi.c:

configure_openmp():
  ├─ available = omp_get_num_procs()
  ├─ IF OMP_NUM_THREADS já definido:
  │  └─ usar valor existente
  ├─ ELSE:
  │  ├─ num_threads = available / nprocs
  │  └─ omp_set_num_threads(num_threads)
  │
  └─ Log: "Rank %d: %d threads\n", rank, omp_get_max_threads()

EXEMPLO:
  - máquina com 8 CPUs
  - mpirun -np 4 ./sudoku_mpi.exe (sem OMP_NUM_THREADS)
  
  RESULTADO:
  - available = omp_get_num_procs() = 8
  - num_threads = 8 / 4 = 2
  - Cada rank: 2 threads
  - Log: "Rank 0: 2 threads"
```

---

## 6. TERMINAÇÃO GLOBAL

### 6.1 Cenários

#### CENÁRIO A: Worker encontra solução

```
Timeline:

T=0s:  Rank 0: dispatch_work() completo, comece solve_omp()
       Ranks 1-3: recv_subproblem() e solve_omp() em paralelo

T=5s:  Rank 2: encontra solução em solve_omp()
       ├─ solve_omp() retorna (tb agora contém solução)
       ├─ send_solution() é chamado
       └─ MPI_Send(solução) → Rank 0

T=5.1s: Rank 0: MPI_Iprobe() detecta msg de Rank 2
        ├─ MPI_Recv(solução)
        ├─ found_solution = 1
        └─ MPI_Bcast(TERMINATION, stop_flag=1)

T=5.2s: Rank 0: print(solução) → stdout
        Rank 1: MPI_Iprobe() detecta TERMINATION
        Rank 2: já saiu
        Rank 3: MPI_Iprobe() detecta TERMINATION

T=5.3s: Todos os ranks: MPI_Finalize()
        └─ Programa termina
```

#### CENÁRIO B: Rank 0 encontra solução

```
Timeline:

T=0s:  Rank 0: solve_omp(first_subproblem)
       Ranks 1-3: recv_subproblem() e solve_omp()

T=3s:  Rank 0: solve_omp() encontra solução
       ├─ found_solution = 1
       └─ pulta collect_solutions() e vai direto para broadcast

T=3.1s: Rank 0: MPI_Bcast(TERMINATION, stop_flag=1)
        Ranks 1-3: MPI_Iprobe() detecta (ou próximo poll)

T=3.2s: Rank 0: print(solução) → stdout
        Ranks 1-3: saem de wait_termination() loop

T=3.3s: Todos: MPI_Finalize()
```

#### CENÁRIO C: Nenhuma solução existe

```
Timeline:

T=0s:  Dispatch, solve_omp() em todos ranks

T=INF: Nenhum rank encontra solução
       └─ Timeout global? (NÃO IMPLEMENTAR AGORA)

COMPORTAMENTO ACTUAL:
  - Rank 0 continua polling forever
  - Ranks 1-3 continuam buscando forever
  - Programa NUNCA termina
  
MELHORIA FUTURA (FASE 1+):
  - Timeout global em Rank 0
  - Após X segundos sem solução → assume não existe
  - MPI_Bcast(TERMINATION) com flag diferente
  - Todos saem e imprimem "Nenhuma solução"
```

### 6.2 Mecanismo Anti-Deadlock

**Problema**: Rank A espera msg de Rank B, mas Rank B espera em solve_omp() infinito.

**Solução Adoptada**:

1. **Polling não-bloqueante** (MPI_Iprobe)
   ```
   Em vez de MPI_Recv() bloqueante (pode travar forever)
   Usar MPI_Iprobe() + MPI_Recv() condicional
   ```

2. **Ordem de Execução**
   ```
   Rank N:
   1. solve_omp() [pode levar tempo, mas é local, não bloqueia outros]
   2. send_solution() [se encontrou]
   3. wait_termination() [polling não-bloqueante]
   
   Rank 0:
   1. solve_omp(first) [parallel com os outros]
   2. collect_solutions() [polling + broadcast]
   3. broadcast_termination()
   ```

3. **Evitar MPI_Recv() bloqueante**
   ```
   ✗ ERRADO:
     MPI_Recv(termination_buffer, ...)  ← BLOQUEIA forever se nenhuma msg
   
   ✓ CORRECTO:
     MPI_Iprobe(0, TAG_TERMINATION, &flag)
     IF flag:
       MPI_Recv(...)
     ELSE:
       continue polling
   ```

### 6.3 Sincronização Global Final

```
ANTES DE MPI_Finalize():

Opção 1 (Simples, ADOPTADA):
  ├─ Rank 0 broadcast TERMINATION
  └─ Cada rank recebe e sai

Opção 2 (Robusta):
  ├─ MPI_Barrier() ← todos esperam até estar prontos
  └─ Depois MPI_Finalize()

IMPLEMENTAÇÃO ADOPTADA:
  Opção 1 + implicit MPI_Finalize() synchronization
```

---

## 7. CONFIGURAÇÃO DE THREADS

### 7.1 Fluxo de Configuração

```
ENTRADA:
  ├─ Variável ambiente OMP_NUM_THREADS (se definida)
  └─ mpirun -np N → número de ranks

PROCESSAMENTO:

1. main()
   ├─ MPI_Init_thread(..., MPI_THREAD_SERIALIZED)
   ├─ MPI_Comm_rank() → rank
   ├─ MPI_Comm_size() → nprocs
   └─ call configure_openmp()

2. configure_openmp()
   ├─ available_procs = omp_get_num_procs()
   │
   ├─ IF getenv("OMP_NUM_THREADS"):
   │  ├─ user_threads = atoi(getenv(...))
   │  ├─ omp_set_num_threads(user_threads)
   │  └─ Log: "Using OMP_NUM_THREADS=%d\n", user_threads
   │
   ├─ ELSE:
   │  ├─ // Modo automático
   │  ├─ ideal_threads = available_procs / nprocs
   │  ├─ IF ideal_threads >= 1:
   │  │  └─ omp_set_num_threads(ideal_threads)
   │  └─ ELSE:
   │     └─ omp_set_num_threads(1)
   │
   └─ Log: "Rank %d: %d threads available\n", rank, omp_get_max_threads()

3. Verificação
   ├─ omp_get_max_threads() → confirmar threads efectivos
   └─ Log final
```

### 7.2 Exemplos de Configuração

| Cenário | CPUs | Ranks | OMP_NUM_THREADS | Resultado |
|---------|------|-------|-----------------|-----------|
| Manual | 8 | 4 | 2 | 4 ranks × 2 threads = 8 threads total |
| Manual | 8 | 4 | 4 | 4 ranks × 4 threads = 16 threads (oversubscription!) |
| Manual | 8 | 2 | (none) | 2 ranks × (8/2=4) threads = 8 threads |
| Manual | 8 | 1 | (none) | 1 rank × 8 threads = 8 threads |
| Manual | 16 | 4 | (none) | 4 ranks × (16/4=4) threads = 16 threads |
| Manual | 16 | 8 | (none) | 8 ranks × (16/8=2) threads = 16 threads |

### 7.3 Modo Automático vs Manual

```
MODO AUTOMÁTICO (Recomendado):
  $ mpirun -np 4 ./sudoku_mpi.exe Sample\ instances/9x9.txt
  
  Behavior:
  - available_procs = 8
  - ideal_threads = 8 / 4 = 2
  - Cada rank: 2 threads
  - Balanceado, sem oversubscription

MODO MANUAL (para tuning):
  $ OMP_NUM_THREADS=2 mpirun -np 4 ./sudoku_mpi.exe ...
  
  Behavior:
  - Ignora cálculo automático
  - Cada rank: 2 threads (como especificado)
  - Útil para experimentação

PERIGO - Oversubscription:
  $ OMP_NUM_THREADS=8 mpirun -np 4 ./sudoku_mpi.exe ...
  
  Behavior:
  - 4 ranks × 8 threads = 32 threads em 8 CPUs
  - Sistema vai context-switch constantemente
  - Performance PÉSSIMA
```

---

## 8. GESTÃO DE MEMÓRIA

### 8.1 Propriedade de Dados

```
FASE 1: Rank 0 carrega

  Rank 0 possuidor:
  ├─ t_sudoku *sudoku
  │  ├─ int **sudoku->tab (original board)
  │  └─ int order
  │
  └─ int ***subproblems (array de boards)
     ├─ subproblems[0] = cópia board 1
     ├─ subproblems[1] = cópia board 2
     └─ ...

FASE 2: Rank 0 distribui

  Rank 0 envia 1D array de cada subproblem
  │
  └─ Rank N recebe e deserializa para 2D
     └─ int **local_board (privado)

FASE 3: Cada rank resolve

  Rank 0:
  ├─ solve_omp(sudoku->tab) [modifica in-place]
  │
  └─ Se encontra solução: guarda resultado

  Rank N:
  ├─ solve_omp(local_board) [modifica in-place]
  │
  └─ Se encontra solução: serializa e envia

FASE 4: Rank 0 recebe resultado

  Rank 0:
  ├─ MPI_Recv(1D array)
  ├─ Deserializa para 2D
  ├─ print(2D solution) → stdout
  └─ free memory
```

### 8.2 Alocação por Rank

```
RANK 0:
  ├─ load_sudoku() alocação
  │  ├─ sudoku struct
  │  ├─ int **sudoku->tab (grid_size × grid_size)
  │  └─ size: ~2KB (9x9) ou ~16KB (16x16)
  │
  ├─ generate_subproblems_mpi() alocações
  │  ├─ subproblems[0..N-1]
  │  ├─ Cada subproblem: int **
  │  └─ Total: N × grid_size²
  │     = 32 × 81 = 2592 ints = ~10KB (9x9)
  │     = 32 × 256 = 8192 ints = ~32KB (16x16)
  │
  ├─ solve_omp() buffer local
  │  ├─ unsigned long long row_used[63], col_used[63], box_used[63]
  │  └─ size: 3 × 63 × 8 bytes = ~1.5KB (stack)
  │
  └─ Durante collect_solutions()
     ├─ recv buffer: int array[grid_size²]
     └─ size: ~2KB (9x9) ou ~16KB (16x16)

RANKS 1..N:
  ├─ recv_subproblem() buffer
  │  ├─ int receive_buffer[grid_size²]
  │  └─ size: ~2KB (9x9) ou ~16KB (16x16)
  │
  ├─ Deserializa para 2D
  │  ├─ int **local_board
  │  └─ size: grid_size² ints + grid_size pointers
  │     = ~2KB (9x9) ou ~16KB (16x16)
  │
  ├─ solve_omp() buffers local
  │  ├─ row_used[], col_used[], box_used[]
  │  └─ size: ~1.5KB (stack)
  │
  ├─ backtrack_parallel() OpenMP buffers
  │  ├─ Cada thread cria:
  │  │  ├─ int **tb_copy (allocação)
  │  │  ├─ unsigned long long *row_copy, *col_copy, *box_copy
  │  │  └─ size por thread: ~2KB (9x9)
  │  │
  │  └─ Total em 2 threads: 2 × 2KB = 4KB
  │
  └─ send_solution() buffer
     ├─ int send_buffer[grid_size²]
     └─ size: ~2KB (9x9)
```

### 8.3 Estratégia de Cópia vs Referência

```
OPÇÃO A: Copiar board inteiro (ADOPTADA)
  
  generate_subproblems_mpi():
  ├─ FOR each subproblem:
  │  ├─ Allocate int **copy
  │  ├─ Memcpy de board original
  │  └─ Guardar em array
  └─ Razão: Evitar modificações acidentais durante backtrack

OPÇÃO B: Referência com restore
  
  (NÃO adoptar - complexo)
  └─ Guardar only estado inicial, restaurar após cada rank
  
ADOPTADA: OPÇÃO A (cópia completa)
```

### 8.4 Ordem de Libertação

```
RANK 0:

  collect_solutions():
    ├─ Recebeu solução?
    │  └─ Usar resultado até print()
    │
    └─ Depois do print():
       ├─ free(subproblems) [array de cópias]
       ├─ free_sudoku(sudoku) [original + struct]
       └─ free(recv_buffer)

RANKS 1..N:

  worker_loop() após find:
    ├─ IF send_solution():
    │  ├─ free(local_board) [cópia alocada]
    │  └─ free(send_buffer)
    │
    └─ EXIT

ANTES MPI_Finalize():
  ├─ Todos os malloc() foram free()
  ├─ Verificar valgrind/asan para leaks
  └─ Seguro para MPI_Finalize()
```

### 8.5 Evitar Race Conditions

```
PROBLEMA: Rank 0 liberta subproblems enquanto Rank 1 ainda usa?

SOLUÇÃO: Sincronização implícita

  Rank 0 liberta:
  ├─ Após MPI_Bcast(TERMINATION)
  └─ Após todos os ranks receberem (bloqueante no MPI_Bcast)

  Rank 1..N:
  ├─ Recebem TERMINATION
  ├─ Libertam suas estruturas
  └─ Saem de worker_loop()

  SEM race: Ordem garantida por MPI_Bcast()

ADICIONALMENTE:
  ├─ Cada rank tem sua própria cópia de board
  ├─ Sem acesso compartilhado (pós-distribuição)
  └─ Seguro para parallelismo
```

---

## 9. FUNÇÕES A REUTILIZAR

### 9.1 Funções Reutilizadas SEM Qualquer Modificação

#### I/O e Parsing

| Função | Ficheiro | Tipo | Reutilização |
|--------|----------|------|--------------|
| `load_sudoku(const char *path)` | validator/loader.c | Public | ✅ Chamar directamente em Rank 0 |
| `free_sudoku(t_sudoku *sudoku)` | general_utils/utils.c | Public | ✅ Chamar directamente em todos ranks |
| `print(int **tb, int order)` | general_utils/utils.c | Public | ✅ Chamar em Rank 0 para imprimir solução |
| `get_next_line(int fd)` | general_utils/get_next_line.c | Public | ✅ Via `load_sudoku()` |
| `parser_parse_order_line(...)` | validator/parser.c | Public | ✅ Via `load_sudoku()` |
| `parser_parse_board_line(...)` | validator/parser.c | Public | ✅ Via `load_sudoku()` |

#### Solver OpenMP

| Função | Ficheiro | Tipo | Reutilização |
|--------|----------|------|--------------|
| `solve_omp(int **tb, int order)` | backtracking/backtracking_omp.c | Public | ✅ **CHAMAR DIRECTAMENTE EM CADA RANK** |
| `init_masks(...)` | backtracking/backtracking_omp.c | Static | ✅ Via `solve_omp()` |
| `find_best_cell(...)` | backtracking/backtracking_omp.c | Static | ✅ Via `solve_omp()` |
| `backtrack_parallel(...)` | backtracking/backtracking_omp.c | Static | ✅ Via `solve_omp()` |
| `backtrack_sequential(...)` | backtracking/backtracking_omp.c | Static | ✅ Via `solve_omp()` |

### 9.2 Padrão de Reutilização

```
SUDOKU_SERIAL.C (padrão base):
  
  main()
  └─ load_sudoku()
     └─ solve()
        └─ print()
           └─ free_sudoku()

SUDOKU_OMP.C (padrão paralelo):
  
  main()
  └─ load_sudoku()
     └─ solve_omp()  [← parallelizado]
        └─ print()
           └─ free_sudoku()

SUDOKU_MPI.C (padrão híbrido - NOVO):
  
  RANK 0:
    main()
    └─ load_sudoku()
       ├─ generate_subproblems_mpi()  [NOVO]
       ├─ dispatch_work()              [NOVO]
       ├─ solve_omp()                  [REUTILIZAR]
       └─ print()
          └─ free_sudoku()

  RANK 1..N:
    └─ solve_omp()  [REUTILIZAR]
       └─ (colectar resultado via MPI)
```

---

## 10. FUNÇÕES MPI NOVAS

### 10.1 Lista Completa de Funções Novas

```
sudoku_mpi.c (NOVO FICHEIRO)

Função 1: void configure_openmp(int rank, int nprocs)
├─ Tipo: Utility
├─ Propósito: Configurar threads OpenMP por rank
├─ Entrada: rank atual, número total de ranks
├─ Saída: omp_set_num_threads() chamado
├─ Lógica:
│  ├─ IF OMP_NUM_THREADS definida: usar
│  ├─ ELSE: num_threads = omp_get_num_procs() / nprocs
│  └─ omp_set_num_threads()
└─ Usada em: main() no início

Função 2: int generate_subproblems_mpi(int **board, int order,
                                        int ***subproblems,
                                        int *count)
├─ Tipo: Distribution
├─ Propósito: Gerar subproblemas iniciais por backtracking shallow
├─ Entrada: board original, order
├─ Saída: array de boards (subproblems), count preenchida
├─ Lógica:
│  ├─ Iniciar com board original
│  ├─ Backtrack até depth = DEPTH_LIMIT
│  ├─ Colectar cada board no nível final
│  └─ Serializar/manter em 2D
├─ Memória: Aloca subproblems[0..N-1]
└─ Usada em: Rank 0 na fase de setup

Função 3: void send_subproblem(int **board, int order,
                               int dest_rank)
├─ Tipo: MPI Communication
├─ Propósito: Enviar 1D array de subproblem para rank destino
├─ Entrada: board 2D, order, dest_rank
├─ Saída: MPI_Send() chamado
├─ Lógica:
│  ├─ Serializar board para 1D array (grid_size²)
│  ├─ MPI_Send(1D_array, size, MPI_INT, dest_rank, TAG_SUBPROBLEM)
│  └─ Log: "Sent subproblem to rank X"
└─ Usada em: dispatch_work() em Rank 0

Função 4: int **recv_subproblem(int order, int *subproblem_id)
├─ Tipo: MPI Communication
├─ Propósito: Receber 1D array e deserializar para 2D
├─ Entrada: order
├─ Saída: board 2D allocado e preenchido
├─ Lógica:
│  ├─ Allocar 1D buffer
│  ├─ MPI_Recv(buffer, size, MPI_INT, 0, TAG_SUBPROBLEM)
│  ├─ Deserializar para 2D
│  └─ Retornar 2D board
└─ Usada em: worker_loop() em Ranks 1..N

Função 5: void send_solution(int **board, int order)
├─ Tipo: MPI Communication
├─ Propósito: Enviar solução para Rank 0
├─ Entrada: board 2D contendo solução
├─ Saída: MPI_Send() chamado
├─ Lógica:
│  ├─ Serializar board para 1D array
│  ├─ MPI_Send(1D_array, size, MPI_INT, 0, TAG_SOLUTION)
│  └─ Log: "Solution sent to rank 0"
└─ Usada em: worker_loop() após solve_omp() success

Função 6: int **recv_solution(int order)
├─ Tipo: MPI Communication
├─ Propósito: Receber solução de worker
├─ Entrada: order (grid_size derivado)
├─ Saída: board 2D allocado
├─ Lógica:
│  ├─ Allocar 1D buffer
│  ├─ MPI_Recv(buffer, size, MPI_INT, ANY_SOURCE, TAG_SOLUTION)
│  ├─ Deserializar para 2D
│  └─ Retornar board
└─ Usada em: collect_solutions() em Rank 0

Função 7: void broadcast_termination(int nprocs)
├─ Tipo: MPI Communication
├─ Propósito: Notificar todos workers para terminar
├─ Entrada: número de ranks
├─ Saída: MPI_Bcast() chamado
├─ Lógica:
│  ├─ Criar buffer com flag=1
│  ├─ MPI_Bcast(buffer, size, MPI_INT, 0, TAG_TERMINATION)
│  └─ Log: "Termination broadcast"
└─ Usada em: collect_solutions() após encontrar solução

Função 8: int check_termination()
├─ Tipo: MPI Communication
├─ Propósito: Polling não-bloqueante para termination signal
├─ Entrada: (nenhuma, usa MPI communicator global)
├─ Saída: 1 se recebeu stop, 0 caso contrário
├─ Lógica:
│  ├─ MPI_Iprobe(0, TAG_TERMINATION, &flag)
│  ├─ IF flag: MPI_Recv(), return 1
│  └─ ELSE: return 0
└─ Usada em: wait_termination() em Ranks 1..N

Função 9: void dispatch_work(int **subproblems, int count,
                            int order, int nprocs)
├─ Tipo: Distribution
├─ Propósito: Enviar subproblems para cada rank via round-robin
├─ Entrada: array de subproblems, count, order, nprocs
├─ Saída: MPI_Send() chamado para cada rank
├─ Lógica:
│  ├─ MPI_Bcast(order, grid_size, count)  [enviar config]
│  ├─ FOR i = 0 TO count-1:
│  │  ├─ dest_rank = i % nprocs
│  │  └─ send_subproblem(subproblems[i], order, dest_rank)
│  └─ Log: "Dispatch complete"
└─ Usada em: main() em Rank 0

Função 10: void collect_solutions(int order, int nprocs,
                                 int **first_result)
├─ Tipo: Collection
├─ Propósito: Polling para solução de workers
├─ Entrada: order, nprocs, resultado de Rank 0
├─ Saída: found_solution definido, result_board preenchido
├─ Lógica:
│  ├─ found = 0
│  ├─ ENQUANTO NOT found:
│  │  ├─ FOR each rank 1..nprocs-1:
│  │  │  ├─ IF check_termination() ou MPI_Iprobe():
│  │  │  │  ├─ recv_solution()
│  │  │  │  ├─ found = 1
│  │  │  │  ├─ broadcast_termination()
│  │  │  │  └─ BREAK
│  │  │  └─ ENDIF
│  │  └─ ENDFOR
│  │
│  │  ├─ IF NOT found:
│  │  │  └─ usleep(POLL_INTERVAL)
│  │  └─ ENDIF
│  └─ ENDWHILE
│
└─ Usada em: main() em Rank 0 após solve_omp()

Função 11: void worker_loop(int **first_subproblem, int order)
├─ Tipo: Work Distribution
├─ Propósito: Receber e processar subproblems
├─ Entrada: first_subproblem (alocado em recv), order
├─ Saída: (nenhuma externa)
├─ Lógica:
│  ├─ board = first_subproblem (ou recv se rank > 0)
│  ├─ ENQUANTO TRUE:
│  │  ├─ solve_omp(board, order)
│  │  │  └─ [REUTILIZAR completamente]
│  │  │
│  │  ├─ IF solve_omp() encontrou:
│  │  │  ├─ send_solution(board, order)
│  │  │  └─ BREAK
│  │  │
│  │  ├─ ELSE:
│  │  │  ├─ free(board)
│  │  │  ├─ board = recv_subproblem(order)
│  │  │  ├─ IF board == NULL:
│  │  │  │  └─ BREAK (sem mais trabalho)
│  │  │  └─ CONTINUE
│  │  │
│  │  └─ IF check_termination():
│  │     └─ BREAK
│  │
│  └─ ENDWHILE
│
└─ Usada em: main() em Ranks 1..N

Função 12: void dispatch_work_to_self(int **first_subproblem,
                                     int order, int rank)
├─ Tipo: Distribution
├─ Propósito: Rank 0 resolve seu próprio subproblem
├─ Entrada: first_subproblem (de generate_subproblems_mpi)
├─ Saída: solve_omp() chamado
├─ Lógica:
│  ├─ solve_omp(first_subproblem, order)
│  │  └─ [REUTILIZAR]
│  │
│  └─ Resultado em first_subproblem
│
└─ Usada em: main() em Rank 0
```

### 10.2 Assinatura Resumida

```c
// sudoku_mpi.c

// Configuration
void configure_openmp(int rank, int nprocs);

// Problem Generation
int generate_subproblems_mpi(int **board, int order,
                             int ***subproblems, int *count);

// MPI Send
void send_subproblem(int **board, int order, int dest_rank);
void send_solution(int **board, int order);

// MPI Recv
int **recv_subproblem(int order, int *subproblem_id);
int **recv_solution(int order);

// MPI Broadcast
void broadcast_termination(int nprocs);
int check_termination(void);

// Distribution & Collection
void dispatch_work(int **subproblems, int count,
                  int order, int nprocs);
void collect_solutions(int order, int nprocs, int **first_result);

// Work Loop
void worker_loop(int **first_subproblem, int order);
void dispatch_work_to_self(int **first_subproblem, int order);

// Main entry (já existe)
int main(int argc, char *argv[]);
```

---

## 11. ESTRUTURA DO NOVO sudoku_mpi.c

### 11.1 Organização de Ficheiro

```
sudoku_mpi.c (NOVO)

SECÇÃO 1: Includes & Defines
  #include <stdio.h>, <stdlib.h>, <string.h>
  #include <mpi.h>
  #include <omp.h>
  #include "sudoku.h"
  
  #define MPI_TAG_INIT 100
  #define MPI_TAG_SUBPROBLEM 101
  #define MPI_TAG_SOLUTION 102
  #define MPI_TAG_TERMINATION 103
  
  #define SUBPROBLEM_DEPTH_LIMIT 2
  #define SUBPROBLEM_MAX 256
  #define POLL_INTERVAL 100  // microseconds

SECÇÃO 2: Utility Functions (MPI setup)
  ├─ void configure_openmp(int rank, int nprocs)
  └─ static void print_usage(void)

SECÇÃO 3: Problem Generation
  ├─ static int **alloc_board(int size)
  ├─ static void free_board(int **board, int size)
  ├─ static void copy_board(int **dest, int **src, int size)
  └─ int generate_subproblems_mpi(int **board, int order,
                                  int ***subproblems, int *count)

SECÇÃO 4: MPI Send Functions
  ├─ static void serialize_board(int **board, int *buffer, int size)
  ├─ void send_subproblem(int **board, int order, int dest_rank)
  └─ void send_solution(int **board, int order)

SECÇÃO 5: MPI Recv Functions
  ├─ static void deserialize_board(int *buffer, int **board, int size)
  ├─ int **recv_subproblem(int order, int *subproblem_id)
  └─ int **recv_solution(int order)

SECÇÃO 6: MPI Broadcast & Polling
  ├─ void broadcast_termination(int nprocs)
  └─ int check_termination(void)

SECÇÃO 7: Distribution & Collection
  ├─ void dispatch_work(int **subproblems, int count,
  │                     int order, int nprocs)
  └─ void collect_solutions(int order, int nprocs, int **first_result)

SECÇÃO 8: Work Loops
  ├─ void worker_loop(int **first_subproblem, int order)
  └─ void dispatch_work_to_self(int **first_subproblem, int order)

SECÇÃO 9: Main
  └─ int main(int argc, char *argv[])
```

### 11.2 Fluxo de main()

```
int main(int argc, char *argv[])

PASSO 1: Validação de Input
  ├─ IF argc != 2:
  │  └─ print_usage(), return 1
  └─ char *filename = argv[1]

PASSO 2: Inicializar MPI
  ├─ MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided)
  ├─ MPI_Comm_rank(MPI_COMM_WORLD, &rank)
  ├─ MPI_Comm_size(MPI_COMM_WORLD, &nprocs)
  └─ time_start = omp_get_wtime()

PASSO 3: Configurar OpenMP
  ├─ configure_openmp(rank, nprocs)
  └─ IF rank == 0:
       └─ fprintf(stderr, "Rank 0: %d threads\n", omp_get_max_threads())

PASSO 4: Rank 0 específico (IF rank == 0)
  │
  ├─ load_sudoku(filename)  [REUTILIZAR]
  ├─ IF !sudoku: error, return 1
  │
  ├─ generate_subproblems_mpi(sudoku->tab, sudoku->order,
  │                            &subproblems, &num_subproblems)
  │
  ├─ dispatch_work(subproblems, num_subproblems, sudoku->order, nprocs)
  │
  ├─ first_sub = subproblems[0]
  ├─ dispatch_work_to_self(first_sub, sudoku->order)
  │  └─ ESTE CHAMA solve_omp() [REUTILIZAR]
  │
  ├─ collect_solutions(sudoku->order, nprocs, &first_sub)
  │  └─ POLLING para resultado
  │
  ├─ IF solução encontrada:
  │  ├─ print(first_sub, sudoku->order)  [REUTILIZAR]
  │  └─ time_end = omp_get_wtime()
  │
  ├─ fprintf(stderr, "%.10f\n", time_end - time_start)
  │
  ├─ free memory (subproblems, sudoku, etc)
  │
  └─ ELSE (IF rank > 0)
      │
      ├─ recv subproblems from Rank 0 (MPI_Bcast)
      │
      ├─ worker_loop(NULL, order)
      │  ├─ Recebe subproblems via MPI_Recv
      │  ├─ Chama solve_omp() [REUTILIZAR]
      │  ├─ Se encontra solução: MPI_Send() e BREAK
      │  └─ Polling para check_termination()
      │
      └─ free memory

PASSO 5: Sincronização Final
  ├─ MPI_Barrier()  [Opcional]
  └─ MPI_Finalize()

PASSO 6: Return
  └─ return 0
```

---

## 12. FLUXOGRAMAS

### 12.1 Fluxograma de Execução Global

```
┌─────────────────────────────────────┐
│   START: main()                     │
└─────────────────────────────────────┘
          │
          ├─ MPI_Init_thread()
          ├─ MPI_Comm_rank(), MPI_Comm_size()
          └─ configure_openmp()
                    │
        ┌───────────┴────────────┐
        │                        │
        ▼ (rank == 0)            ▼ (rank > 0)
    ┌──────────────┐        ┌──────────────┐
    │   RANK 0    │        │  RANK 1..N   │
    │  (Master)   │        │  (Workers)   │
    └──────────────┘        └──────────────┘
        │                       │
        ├─ load_sudoku()        ├─ MPI_Bcast()
        │  [REUTILIZAR]         │  recv: order, size, count
        │                       │
        ├─ generate_subprob.    ├─ Aloca buffer para recv
        │  (depth=2)            │
        │                       ├─ worker_loop()
        ├─ dispatch_work()      │  ├─ MPI_Recv(subproblem)
        │  ├─ MPI_Bcast()       │  │
        │  └─ MPI_Send()        │  ├─ solve_omp()
        │     (round-robin)     │  │  [REUTILIZAR + OpenMP]
        │                       │  │
        ├─ dispatch_to_self()   │  ├─ IF found:
        │  ├─ solve_omp()       │  │  ├─ MPI_Send(solution)
        │  │  [REUTILIZAR]      │  │  └─ BREAK
        │  │  + OpenMP threads  │  │
        │  └─ IF found:         │  ├─ ELSE:
        │     found_flag = 1    │  │  ├─ free(board)
        │                       │  │  ├─ loop
        │                       │  │  └─ continue
        ├─ collect_solutions() │  │
        │  ├─ MPI_Iprobe()     │  ├─ check_termination()
        │  │  (polling)         │  │  ├─ MPI_Iprobe()
        │  │                    │  │  └─ IF flag: MPI_Recv()
        │  ├─ IF solution:     │  │     BREAK
        │  │  ├─ found = 1     │  │
        │  │  ├─ MPI_Bcast()   │  └─ free memory
        │  │  │ (TERMINATION)  │
        │  │  └─ break polling └──────────┐
        │  │                              │
        │  └─ print(solution)             │
        │     [REUTILIZAR]                │
        │                                 │
        ├─ fprintf(stderr, time)          │
        │                                 │
        └─ free memory                    │
             │                            │
             └─────────────┬──────────────┘
                          │
                ┌─────────┴─────────┐
                │  MPI_Barrier()    │
                │  MPI_Finalize()   │
                └─────────┬─────────┘
                          │
                ┌─────────▼─────────┐
                │  PROGRAM EXIT     │
                │  return 0         │
                └───────────────────┘
```

### 12.2 Fluxograma de Rank 0 Detalhado

```
┌──────────────────────────────┐
│  Rank 0: load_sudoku()       │
└──────┬───────────────────────┘
       │
       ├─ open(argv[1])
       ├─ parse order
       ├─ alloc_grid(size)
       ├─ parse board
       └─ return t_sudoku*
           │
┌──────────▼──────────────────────────┐
│  Rank 0: generate_subproblems_mpi() │
└──────┬───────────────────────────────┘
       │
       ├─ backtrack até depth 2
       ├─ colectar boards em lista
       ├─ malloc subproblems array
       └─ return count ~30
           │
┌──────────▼──────────────────────────┐
│  Rank 0: dispatch_work()            │
└──────┬───────────────────────────────┘
       │
       ├─ MPI_Bcast(order, size, count)
       │  └─ Todos ranks recebem
       │
       ├─ FOR i = 0 TO count-1:
       │  ├─ dest = i % nprocs
       │  ├─ IF dest != 0:
       │  │  └─ MPI_Send(subproblem)
       │  └─ ELSE:
       │     └─ salvar em first_subproblem
       │
       └─ todos workers em recv
           │
┌──────────▼──────────────────────────┐
│  Rank 0: dispatch_work_to_self()    │
└──────┬───────────────────────────────┘
       │
       ├─ solve_omp(first_subproblem, order)
       │  └─ [REUTILIZAR]
       │
       ├─ IF solve_omp() encontra:
       │  ├─ first_subproblem = solução
       │  └─ found = 1
       │
       └─ solution em first_subproblem
           │
┌──────────▼──────────────────────────┐
│  Rank 0: collect_solutions()        │
└──────┬───────────────────────────────┘
       │
       ├─ ENQUANTO NOT found:
       │  ├─ IF Iprobe solução:
       │  │  ├─ MPI_Recv(solução)
       │  │  ├─ found = 1
       │  │  └─ break
       │  │
       │  └─ ELSE:
       │     └─ usleep(100)
       │
       └─ broadcast_termination()
           MPI_Bcast(STOP)
           │
┌──────────▼──────────────────────────┐
│  Rank 0: print(solution)            │
│         [REUTILIZAR]                │
└──────┬───────────────────────────────┘
       │
       ├─ print() → stdout
       ├─ time cálculo
       ├─ fprintf(stderr, "%.10f\n", time)
       │
       └─ free_sudoku(), free memory
           │
           └─► MPI_Finalize()
```

### 12.3 Fluxograma de Worker Rank Detalhado

```
┌──────────────────────────────┐
│  Rank > 0: MPI_Bcast()       │
└──────┬───────────────────────┘
       │
       ├─ recebe order, size, count
       └─ configura variáveis locais
           │
┌──────────▼──────────────────┐
│  worker_loop()              │
└──────┬──────────────────────┘
       │
       ├─ ENQUANTO TRUE:
       │
       ├─ recv_subproblem(order)
       │  ├─ MPI_Recv(buffer)
       │  ├─ deserialize buffer → 2D board
       │  └─ return board
       │
       ├─ solve_omp(board, order)
       │  └─ [REUTILIZAR]
       │  │  OpenMP threads criam paralelismo
       │  │
       │  ├─ IF encontra solução:
       │  │  └─ return 1, board tem solução
       │  │
       │  └─ ELSE:
       │     └─ return 0, board não resolvido
       │
       ├─ IF solve_omp() retorna 1:
       │  ├─ send_solution(board, order)
       │  │  ├─ serialize board → 1D buffer
       │  │  ├─ MPI_Send(buffer, size, MPI_INT, 0, TAG_SOLUTION)
       │  │  └─ return
       │  │
       │  ├─ free(board)
       │  └─ BREAK ← EXIT worker_loop
       │
       ├─ ELSE (sem solução):
       │  ├─ free(board)
       │  │
       │  ├─ check_termination()
       │  │  ├─ MPI_Iprobe(0, TAG_TERMINATION, &flag)
       │  │  ├─ IF flag:
       │  │  │  ├─ MPI_Recv(termination_msg)
       │  │  │  └─ return 1
       │  │  │
       │  │  └─ ELSE:
       │  │     └─ return 0
       │  │
       │  ├─ IF termination recebida:
       │  │  └─ BREAK ← EXIT worker_loop
       │  │
       │  └─ ELSE:
       │     └─ CONTINUE ← próximo subproblem
       │
       └─ ENDWHILE
           │
           └─► free memory
               └─► MPI_Finalize()
```

### 12.4 Fluxograma de Comunicação MPI

```
╔═══════════════════════════════════════════════════════════╗
║           MPI COMMUNICATION FLOW                          ║
╚═══════════════════════════════════════════════════════════╝

                        RANK 0
                          │
            ┌─────────────┬┴────────────┬──────────────┐
            │             │            │              │
            ▼             ▼            ▼              ▼
        RANK 1        RANK 2       RANK 3        RANK 4

1. INITIALIZAÇÃO
   ├─ Rank 0: MPI_Bcast(order, size, count)
   │
   ├─ Ranks 1-4: MPI_Recv() implícito via Bcast
   │  └─ Todos recebem (order, size, count)

2. DISTRIBUIÇÃO DE TRABALHO
   Rank 0: FOR each subproblem
   │
   ├─ rank = i % 4
   ├─ MPI_Send(subproblem[0]) → Rank 1  (TAG_SUBPROBLEM)
   ├─ MPI_Send(subproblem[1]) → Rank 2  (TAG_SUBPROBLEM)
   ├─ MPI_Send(subproblem[2]) → Rank 3  (TAG_SUBPROBLEM)
   ├─ MPI_Send(subproblem[3]) → Rank 4  (TAG_SUBPROBLEM)
   ├─ MPI_Send(subproblem[4]) → Rank 1  (TAG_SUBPROBLEM)
   └─ ...

   Rank 1-4: MPI_Recv(subproblem) bloqueante
   │
   └─ Aguardam

3. PROCESSAMENTO LOCAL (PARALELO)
   Rank 0: solve_omp(subproblem[0])  [local]
   Rank 1: solve_omp(subproblem[k])  [local]
   Rank 2: solve_omp(subproblem[k])  [local]
   Rank 3: solve_omp(subproblem[k])  [local]
   Rank 4: solve_omp(subproblem[k])  [local]

4. RECOLHA DE RESULTADOS
   Rank 0: polling loop
   │
   ├─ MPI_Iprobe(1..4, TAG_SOLUTION, &flag)
   │
   ├─ IF Rank 2 encontra solução:
   │  └─ MPI_Send(solution) → Rank 0
   │
   └─ Rank 0: MPI_Recv(solution)
      └─ found = 1

5. TERMINAÇÃO GLOBAL
   Rank 0: MPI_Bcast(TERMINATION)
   │
   └─ Ranks 1-4: MPI_Iprobe(), MPI_Recv(TERMINATION)
      └─ Saem de worker_loop()

6. FINALIZAÇÃO
   Todos: MPI_Finalize()
   │
   └─ Programa termina
```

---

## 13. VALIDAÇÃO ARQUITECTÓNICA

### 13.1 VALIDAÇÃO: Não Duplica Backtracking

#### Checklist Reutilização

```
✓ REUTILIZA solve_omp() 100%
  ├─ Chamada directa em Rank 0: solve_omp(board, order)
  └─ Chamada directa em Ranks 1..N: solve_omp(board, order)

✓ REUTILIZA backtrack_parallel() 100%
  └─ Via solve_omp() → backtrack_parallel() automaticamente

✓ REUTILIZA backtrack_sequential() 100%
  └─ Via solve_omp() → backtrack_sequential() automaticamente

✓ REUTILIZA init_masks() 100%
  └─ Via solve_omp() → init_masks() automaticamente

✓ REUTILIZA find_best_cell() 100%
  └─ Via solve_omp() → find_best_cell() automaticamente

✓ Não reimplementa nenhuma função de backtracking
✓ Não cria backtrack_mpi(), backtrack_flat(), etc.
✓ MPI código = APENAS distribuição e comunicação
```

#### Onde solve_omp() É Chamado

```
sudoku_mpi.c (NOVO):

1. dispatch_work_to_self() em Rank 0
   └─ solve_omp(first_subproblem, order)

2. worker_loop() em Ranks 1..N
   └─ solve_omp(board, order)

TOTAL: 2 localizações
       Ambas DIRECTAS
       Nenhuma reimplementação
       100% reutilização de infraestrutura OpenMP
```

### 13.2 VALIDAÇÃO: MPI Apenas Distribuição

#### O que MPI FAZ (Permitido)

```
✓ MPI_Init_thread()        → inicializa
✓ MPI_Comm_rank()          → identifica rank
✓ MPI_Comm_size()          → obtém nprocs
✓ MPI_Bcast()              → distribui parameters
✓ MPI_Send()               → envia subproblem
✓ MPI_Recv()               → recebe subproblem
✓ MPI_Iprobe()             → polling não-bloqueante
✓ MPI_Finalize()           → finaliza
✓ Lógica de distribuição   → round-robin, balanceamento
✓ Lógica de terminação     → broadcast stop signal
```

#### O que MPI NÃO FAZ (Proibido)

```
✗ NÃO implementa backtracking
✗ NÃO cria solve_mpi()
✗ NÃO duplica find_best_cell()
✗ NÃO reimplementa init_masks()
✗ NÃO cria versão 1D flat
✗ NÃO invoca backtrack_parallel() directamente
✗ NÃO invoca backtrack_sequential() directamente
```

### 13.3 VALIDAÇÃO: Arquitetura Hierárquica

#### Padrão Serial → OMP → MPI+OMP

```
┌─────────────────────────────────┐
│ SERIAL (backtracking.c)         │
│  solve(int **tb, int order)     │
│  └─ Recursão pura, sequencial   │
└─────────────────────────────────┘
           ↓ REUTILIZAÇÃO
┌─────────────────────────────────┐
│ OpenMP (backtracking_omp.c)     │
│  solve_omp(int **tb, int order) │
│  ├─ init_masks()                │
│  ├─ find_best_cell()            │
│  ├─ backtrack_parallel()        │
│  │  ├─ #pragma omp parallel for │
│  │  ├─ cada thread: sua cópia   │
│  │  └─ #pragma omp critical     │
│  └─ backtrack_sequential()      │
└─────────────────────────────────┘
           ↓ REUTILIZAÇÃO
┌─────────────────────────────────┐
│ MPI+OMP (sudoku_mpi.c - NOVO)   │
│  ├─ Rank 0: load + dispatch     │
│  ├─ Ranks 1..N: recv + solve    │
│  │  └─ solve_omp()  ← CHAMA     │
│  │     ├─ OpenMP threads        │
│  │     └─ Paralelismo recursivo │
│  └─ Polling + collection        │
│     └─ result + print           │
└─────────────────────────────────┘

VALIDAÇÃO:
✓ solve_omp() é o mesmo em OMP e MPI+OMP
✓ Sem re-implementação de backtracking
✓ MPI adiciona apenas distribuição
✓ OpenMP adaptativo mantido intacto
✓ Hierarquia Serial → OMP → MPI+OMP respeitada
```

### 13.4 VALIDAÇÃO: Thread Safety

```
NÍVEL 1: MPI Thread Safety
├─ MPI_Init_thread(..., MPI_THREAD_SERIALIZED)
│  └─ Apenas 1 thread por rank chama MPI
│
└─ Cumprido: cada rank é independente no MPI

NÍVEL 2: OpenMP Thread Safety (dentro rank)
├─ solve_omp() chamado por 1 thread (main thread de cada rank)
│  └─ Dentro solve_omp():
│     ├─ backtrack_parallel() cria #pragma omp parallel for
│     ├─ Cada thread: malloc suas próprias cópias
│     ├─ Sem race conditions (estruturas privadas)
│     └─ #pragma omp critical para resultado compartilhado
│
└─ Cumprido: infra OpenMP existente mantém thread-safety

VALIDAÇÃO:
✓ MPI_THREAD_SERIALIZED garantido
✓ Sem MPI calls dentro de #pragma omp parallel
✓ sem data races no board (cada thread copia)
✓ #pragma omp critical apenas na escrita final
```

### 13.5 VALIDAÇÃO: Sem Deadlocks

#### Análise de Deadlock Potencial

```
CENÁRIO 1: Rank A espera msg de Rank B forever
  MITIGAÇÃO:
  ├─ MPI_Iprobe() é não-bloqueante
  ├─ Se nenhuma msg: return 0 e continue
  ├─ Sem MPI_Recv() bloqueante indefinido
  ├─ Polling com timeout implícito
  └─ ✓ Sem deadlock

CENÁRIO 2: Rank 0 bloqueia, Ranks 1..N travam
  MITIGAÇÃO:
  ├─ solve_omp() executa localmente
  ├─ Não bloqueia MPI (exceto inicio Bcast)
  ├─ Polling em Rank 0 continua mesmo se Ranks bloqueados
  ├─ MPI_Iprobe() nunca bloqueia
  └─ ✓ Sem deadlock

CENÁRIO 3: Circular wait (A→B, B→A)
  MITIGAÇÃO:
  ├─ Hierarquia clara:
  │  ├─ Rank 0: send, depois recv (sempre master)
  │  └─ Rank 1..N: recv, depois send (sempre slave)
  │
  ├─ Sem espera cíclica
  └─ ✓ Sem deadlock
```

### 13.6 VALIDAÇÃO: Reutilização Máxima

#### Matriz de Reutilização

| Componente | Ficheiro | Função | Status | MPI Novo? |
|-----------|----------|--------|--------|-----------|
| **Loader** | validator/loader.c | load_sudoku() | ✓ Intacto | ✗ Não |
| **Parser** | validator/parser.c | parser_* | ✓ Intacto | ✗ Não |
| **Utils** | general_utils/utils.c | print(), free_sudoku() | ✓ Intacto | ✗ Não |
| **Solver OMP** | backtracking_omp.c | solve_omp() | ✓ Intacto | ✗ Não |
| **Backtrack Par** | backtracking_omp.c | backtrack_parallel() | ✓ Intacto | ✗ Não |
| **Backtrack Seq** | backtracking_omp.c | backtrack_sequential() | ✓ Intacto | ✗ Não |
| **Init Masks** | backtracking_omp.c | init_masks() | ✓ Intacto | ✗ Não |
| **Find Best Cell** | backtracking_omp.c | find_best_cell() | ✓ Intacto | ✗ Não |
| **MPI Distrib** | sudoku_mpi.c (NOVO) | generate_subproblems_mpi() | ✗ Novo | ✓ Sim |
| **MPI Comms** | sudoku_mpi.c (NOVO) | send/recv functions | ✗ Novo | ✓ Sim |
| **OpenMP Config** | sudoku_mpi.c (NOVO) | configure_openmp() | ✗ Novo | ✓ Sim |

**RESULTADO**: 8 funções reutilizadas, 3 novas criadas.
Reutilização: 73% (8/11)

---

## CONCLUSÃO

### 14.1 Resumo Arquitectónico

```
NOVO SUDOKU_MPI.C:

├─ MPI layer (distribuição)
│  ├─ generate_subproblems_mpi()   [NOVO]
│  ├─ dispatch_work()               [NOVO]
│  ├─ collect_solutions()           [NOVO]
│  └─ polling + termination         [NOVO]
│
├─ OpenMP layer (solver)
│  └─ solve_omp()  [REUTILIZAR 100%]
│     ├─ init_masks()               [REUTILIZAR]
│     ├─ find_best_cell()           [REUTILIZAR]
│     ├─ backtrack_parallel()       [REUTILIZAR]
│     └─ backtrack_sequential()     [REUTILIZAR]
│
└─ I/O layer
   ├─ load_sudoku()                [REUTILIZAR]
   ├─ print()                       [REUTILIZAR]
   └─ free_sudoku()                [REUTILIZAR]
```

### 14.2 Garantias Arquitectónicas

```
✓ REUTILIZAÇÃO 100%: solve_omp() é único solver
✓ NÃO há duplicação: backtrack_* não reimplementado
✓ THREAD-SAFE: MPI_THREAD_SERIALIZED + OpenMP safety
✓ DEADLOCK-FREE: Polling + hierarquia clara
✓ HIERÁRQUICO: Serial → OMP → MPI+OMP
✓ ESCALÁVEL: 2 níveis paralelismo independentes
```

### 14.3 Próximas Fases

```
FASE 1: Implementação
  └─ Escrever sudoku_mpi.c segundo esta arquitectura

FASE 2: Testes
  ├─ Compilação com backtracking_omp.o
  ├─ Testes funcionais (1, 2, 4 ranks)
  ├─ Testes com OpenMP threads (2, 4, 8)
  └─ Validação de resultados

FASE 3: Optimização
  ├─ Tuning de DEPTH_LIMIT
  ├─ Análise de balanceamento
  └─ Profiling de performance

FASE 4: Melhorias
  ├─ Timeout global (se nenhuma solução)
  ├─ Work-stealing (dynamic load balancing)
  └─ Checkpointing (resumir execução)
```

---

**FIM DO DOCUMENTO TÉCNICO FASE 0**

**Status**: ✓ Especificação Completa
**Próximo**: Implementação (Fase 1)

