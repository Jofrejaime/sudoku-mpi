# PASSO 5.6 — ANÁLISE DE DEPENDÊNCIAS E IMPACTO
# ANTES DA REFACTORIZAÇÃO

**Data**: 2026-06-16  
**Objectivo**: Analisar dependências completas antes de qualquer reorganização  
**Status**: ⚠️ **APENAS ANÁLISE** — NENHUMA ALTERAÇÃO FEITA

---

## FASE 1 — ESTRUTURA ACTUAL DO PROJECTO

```
sudoku_mpi/
├── .git/                           # Controlo de versões
├── .gitignore                      # Ficheiros ignorados
├── Makefile                        # Build system
│
├── sudoku.h                        # Header principal (funções públicas)
│
├── sudoku_serial.c                 # Main serial (42 linhas)
├── sudoku_omp.c                    # Main OpenMP (42 linhas)
├── sudoku_mpi.c                    # Main MPI+OpenMP (1063 linhas)
│
├── sudoku_serial.exe               # Executável serial
├── sudoku_omp.exe                  # Executável OpenMP
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
├── obj/                            # Ficheiros objeto (.o)
│   ├── sudoku_serial.o
│   ├── sudoku_omp.o
│   ├── sudoku_mpi.o
│   ├── backtracking.o
│   ├── backtracking_omp.o
│   ├── utils.o / utils_omp.o / utils_mpi.o
│   ├── loader.o / loader_omp.o / loader_mpi.o
│   ├── parser.o / parser_omp.o / parser_mpi.o
│   └── get_next_line.o / get_next_line_omp.o / get_next_line_mpi.o
│
├── Sample instances/              # Ficheiros de teste
│   ├── 9x9.txt
│   ├── 9x9-nosol.txt
│   ├── 16x16.txt
│   ├── 16x16-nosol.txt
│   └── 16x16-zeros.txt
│
└── PASSO*.md                       # Documentação do projeto
```

---

## FASE 2 — INVENTÁRIO COMPLETO DE FUNÇÕES

### 2.1 — `sudoku.h` (Header Principal)

**Estruturas**:
```c
typedef struct s_sudoku {
    int **tab;
    int order;
} t_sudoku;
```

**Funções Públicas Declaradas**:


| Função | Origem | Descrição |
|--------|--------|-----------|
| `get_next_line()` | general_utils/ | Leitura linha por linha |
| `load_sudoku()` | validator/ | Carrega Sudoku de ficheiro |
| `free_sudoku()` | general_utils/ | Liberta t_sudoku |
| `alloc_board()` | general_utils/ | Aloca board 2D |
| `free_board()` | general_utils/ | Liberta board 2D |
| `copy_board()` | general_utils/ | Copia board elemento a elemento |
| `is_solved()` | general_utils/ | Valida solução completa |
| `parser_parse_order_line()` | validator/ | Parse da primeira linha |
| `parser_compute_grid_size()` | validator/ | Calcula tamanho do grid |
| `parser_parse_board_line()` | validator/ | Parse de linhas do board |
| `print()` | general_utils/ | Imprime board |
| `solve()` | backtracking/ | Solver serial |
| `solve_omp()` | backtracking/ | Solver OpenMP |

**Total**: 13 funções públicas

---

### 2.2 — `sudoku_serial.c` (42 linhas)

**Funções**:
- `static void print_usage()` — Instruções de uso
- `int main()` — Entry point

**Dependências** (funções de sudoku.h):
- `load_sudoku()`
- `solve()`
- `print()`
- `free_sudoku()`

---

### 2.3 — `sudoku_omp.c` (42 linhas)

**Funções**:
- `static void print_usage()` — Instruções de uso
- `int main()` — Entry point

**Dependências** (funções de sudoku.h):
- `load_sudoku()`
- `solve_omp()`
- `print()`
- `free_sudoku()`

---

### 2.4 — `sudoku_mpi.c` (1063 linhas)

**Funções Públicas**:
1. `void configure_openmp(int rank, int nprocs)` — Configuração híbrida
2. `void flatten_board(int **board, int size, int *buffer)` — Serialização
3. `int **unflatten_board(int *buffer, int size)` — Deserialização
4. `void send_subproblem(int **board, int order, int dest_rank)` — Envio MPI
5. `void send_solution(int **board, int order)` — Envio solução
6. `int **recv_subproblem(int order)` — Recepção com polling
7. `int **recv_solution(int order)` — Recepção solução
8. `int generate_subproblems_mpi(...)` — Geração de subproblemas
9. `void dispatch_work(...)` — Distribuição Round-Robin
10. `void worker_loop(int order)` — Loop dos workers
11. `void collect_solutions(...)` — Recolha de soluções
12. `void broadcast_termination(int nprocs)` — Notificação terminação

**Funções Estáticas**:
13. `static int find_first_empty(...)` — Helper geração
14. `static void print_usage()` — Instruções de uso

**Função Principal**:
15. `int main(int argc, char *argv[])` — Entry point

**Dependências** (funções de sudoku.h):
- `load_sudoku()`
- `solve_omp()` ← **REUTILIZAÇÃO CRÍTICA**
- `is_solved()` ← **REUTILIZAÇÃO CRÍTICA**
- `print()`
- `free_sudoku()`
- `alloc_board()` ← **REUTILIZAÇÃO CRÍTICA**
- `free_board()` ← **REUTILIZAÇÃO CRÍTICA**
- `copy_board()` ← **REUTILIZAÇÃO CRÍTICA**

---

### 2.5 — `backtracking/backtracking.c` (227 linhas)

**Funções Públicas**:
- `void solve(int **tb, int order)` — Solver serial

**Funções Estáticas** (helpers internos):
- `static int board_size(int order)`
- `static unsigned long long full_mask(int size)`
- `static int box_index(int order, int row, int col)`
- `static int count_bits(unsigned long long mask)`
- `static int init_masks(...)`
- `static unsigned long long possible_mask(...)`
- `static int find_best_cell(...)`
- `static int backtrack(...)`

**Dependências**: Nenhuma (self-contained)

---

### 2.6 — `backtracking/backtracking_omp.c` (456 linhas)

**Funções Públicas**:
- `void solve_omp(int **tb, int order)` — Solver OpenMP paralelo

**Funções Estáticas** (helpers internos):
- `static int board_size(int order)`
- `static unsigned long long full_mask(int size)`
- `static int box_index(int order, int row, int col)`
- `static int count_bits(unsigned long long mask)`
- `static int init_masks(...)`
- `static unsigned long long possible_mask(...)`
- `static int find_best_cell(...)` — versão com `*best_count`
- `static int backtrack_parallel(...)`
- `static int backtrack_sequential(...)`

**Dependências**: Nenhuma (self-contained)

---

### 2.7 — `general_utils/utils.c` (197 linhas)

**Funções Públicas** (todas em sudoku.h):
1. `void print(int **tb, int order)` — Impressão
2. `void free_sudoku(t_sudoku *sudoku)` — Libertação de estrutura
3. `int **alloc_board(int size)` — Alocação 2D
4. `void free_board(int **board, int size)` — Libertação 2D
5. `void copy_board(int **dest, int **src, int size)` — Cópia
6. `int is_solved(int **tb, int order)` — Validação completa

**Dependências**: Nenhuma (funções utilitárias puras)

---

### 2.8 — `validator/loader.c` e `validator/parser.c`

**Funções** (não analisadas em detalhe, mas presentes):
- `load_sudoku()` — parsing completo de ficheiro
- `parser_parse_order_line()`
- `parser_compute_grid_size()`
- `parser_parse_board_line()`

**Dependências**: `get_next_line()`, funções de alocação

---

## FASE 3 — ANÁLISE DE DEPENDÊNCIAS

### 3.1 — Tabela de Dependências por Função

| Função | Serial | OMP | MPI | Exclusiva MPI? |
|--------|:------:|:---:|:---:|:--------------:|
| **Funções Partilhadas (general_utils/utils.c)** |
| `print()` | ✓ | ✓ | ✓ | ❌ |
| `free_sudoku()` | ✓ | ✓ | ✓ | ❌ |
| `alloc_board()` | — | — | ✓ | ❌ |
| `free_board()` | — | — | ✓ | ❌ |
| `copy_board()` | — | — | ✓ | ❌ |
| `is_solved()` | — | — | ✓ | ❌ |
| **Funções de Loading (validator/)** |
| `load_sudoku()` | ✓ | ✓ | ✓ | ❌ |
| `parser_*()` | ✓ | ✓ | ✓ | ❌ |
| `get_next_line()` | ✓ | ✓ | ✓ | ❌ |
| **Solvers (backtracking/)** |
| `solve()` | ✓ | — | — | ❌ |
| `solve_omp()` | — | ✓ | ✓ | ❌ |
| **Funções MPI (sudoku_mpi.c)** |
| `configure_openmp()` | — | — | ✓ | ✓ |
| `flatten_board()` | — | — | ✓ | ✓ |
| `unflatten_board()` | — | — | ✓ | ✓ |
| `send_subproblem()` | — | — | ✓ | ✓ |
| `recv_subproblem()` | — | — | ✓ | ✓ |
| `send_solution()` | — | — | ✓ | ✓ |
| `recv_solution()` | — | — | ✓ | ✓ |
| `generate_subproblems_mpi()` | — | — | ✓ | ✓ |
| `dispatch_work()` | — | — | ✓ | ✓ |
| `worker_loop()` | — | — | ✓ | ✓ |
| `collect_solutions()` | — | — | ✓ | ✓ |
| `broadcast_termination()` | — | — | ✓ | ✓ |
| `find_first_empty()` | — | — | ✓ | ✓ |

---

### 3.2 — Mapa de Dependências Críticas

```
sudoku_serial.c
    └── depende de: load_sudoku(), solve(), print(), free_sudoku()

sudoku_omp.c
    └── depende de: load_sudoku(), solve_omp(), print(), free_sudoku()

sudoku_mpi.c
    └── depende de: load_sudoku(), solve_omp(), is_solved(), print(), 
                    free_sudoku(), alloc_board(), free_board(), copy_board()

backtracking.c
    └── depende de: NADA (self-contained)

backtracking_omp.c
    └── depende de: NADA (self-contained)

general_utils/utils.c
    └── depende de: NADA (funções utilitárias puras)

validator/
    └── depende de: get_next_line(), alloc_board()
```

**⚠️ DEPENDÊNCIA CRÍTICA IDENTIFICADA**:

`sudoku_mpi.c` **REUTILIZA**:
- `solve_omp()` de `backtracking_omp.c`
- `is_solved()` de `general_utils/utils.c`
- `alloc_board()`, `free_board()`, `copy_board()` de `general_utils/utils.c`

**Conclusão**: `sudoku_mpi.c` **NÃO é self-contained**. Depende fortemente de funções partilhadas.

---

## FASE 4 — IDENTIFICAÇÃO DE CANDIDATOS A REFACTORIZAÇÃO

### GRUPO A — Funções Partilhadas (NÃO MOVER)

**Localização**: `general_utils/utils.c`

| Função | Usada por Serial? | Usada por OMP? | Usada por MPI? |
|--------|:-----------------:|:--------------:|:--------------:|
| `print()` | ✓ | ✓ | ✓ |
| `free_sudoku()` | ✓ | ✓ | ✓ |
| `alloc_board()` | — | — | ✓ |
| `free_board()` | — | — | ✓ |
| `copy_board()` | — | — | ✓ |
| `is_solved()` | — | — | ✓ |

**Decisão**: ❌ **NÃO MOVER**

**Razão**:
1. Usadas por múltiplas versões
2. Localização atual (`general_utils/`) é correta
3. Movê-las quebraria `sudoku_mpi.c`

---

### GRUPO B — Funções Exclusivas MPI (PODEM SER MOVIDAS)

**Localização atual**: `sudoku_mpi.c`

**Candidatas a módulo separado**:

1. **Serialização**:
   - `flatten_board()`
   - `unflatten_board()`

2. **Comunicação MPI**:
   - `send_subproblem()`
   - `recv_subproblem()`
   - `send_solution()`
   - `recv_solution()`

3. **Distribuição de Trabalho**:
   - `generate_subproblems_mpi()`
   - `dispatch_work()`
   - `find_first_empty()` (helper)

4. **Controlo**:
   - `worker_loop()`
   - `collect_solutions()`
   - `broadcast_termination()`

5. **Configuração**:
   - `configure_openmp()`

**Total**: 13 funções exclusivas MPI

---

### GRUPO C — Funções de Configuração (FRONTEIRA)

**Localização atual**: `sudoku_mpi.c`

- `configure_openmp()`

**Análise**: Poderia ser útil para `sudoku_omp.c` também, mas atualmente apenas `sudoku_mpi.c` usa.

**Decisão**: Manter em `sudoku_mpi.c` (específica para ambiente híbrido)

---

## FASE 5 — PROPOSTA DE NOVA ESTRUTURA

### 5.1 — Estrutura Actual (COMPLETA)

```
sudoku_mpi/
├── .git/                           # Controlo de versões Git
│   └── (ficheiros internos do git)
│
├── .gitignore                      # Ficheiros ignorados pelo Git
│
├── Makefile                        # Build system (serial + OMP + MPI)
│
├── sudoku.h                        # Header principal (13 funções públicas)
│
├── sudoku_serial.c                 # Main serial (42 linhas)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_serial.exe               # Executável serial compilado
│
├── sudoku_omp.c                    # Main OpenMP (42 linhas)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_omp.exe                  # Executável OpenMP compilado
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (1063 linhas) ← FICHEIRO GRANDE
│   ├── configure_openmp()          # ~50 linhas
│   ├── flatten_board()             # ~30 linhas
│   ├── unflatten_board()           # ~50 linhas
│   ├── send_subproblem()           # ~30 linhas
│   ├── recv_subproblem()           # ~80 linhas (com polling)
│   ├── send_solution()             # ~30 linhas
│   ├── recv_solution()             # ~50 linhas
│   ├── find_first_empty()          # ~30 linhas
│   ├── generate_subproblems_mpi()  # ~80 linhas
│   ├── dispatch_work()             # ~30 linhas
│   ├── worker_loop()               # ~50 linhas
│   ├── collect_solutions()         # ~40 linhas
│   ├── broadcast_termination()     # ~20 linhas
│   ├── static void print_usage()   # ~10 linhas
│   └── int main()                  # ~150 linhas
│
├── sudoku_mpi.exe                  # Executável MPI+OpenMP compilado
│
├── backtracking/
│   ├── backtracking.c              # Solver serial (227 linhas)
│   │   ├── static int board_size()
│   │   ├── static unsigned long long full_mask()
│   │   ├── static int box_index()
│   │   ├── static int count_bits()
│   │   ├── static int init_masks()
│   │   ├── static unsigned long long possible_mask()
│   │   ├── static int find_best_cell()
│   │   ├── static int backtrack()
│   │   └── void solve()            # ← Função pública
│   │
│   └── backtracking_omp.c          # Solver OpenMP paralelo (456 linhas)
│       ├── static int board_size()
│       ├── static unsigned long long full_mask()
│       ├── static int box_index()
│       ├── static int count_bits()
│       ├── static int init_masks()
│       ├── static unsigned long long possible_mask()
│       ├── static int find_best_cell()
│       ├── static int backtrack_parallel()
│       ├── static int backtrack_sequential()
│       └── void solve_omp()        # ← Função pública
│
├── general_utils/
│   ├── utils.c                     # Utilitários partilhados (197 linhas)
│   │   ├── void print()            # ← Imprime Sudoku
│   │   ├── void free_sudoku()      # ← Liberta t_sudoku
│   │   ├── int **alloc_board()     # ← Aloca board 2D
│   │   ├── void free_board()       # ← Liberta board 2D
│   │   ├── void copy_board()       # ← Copia board
│   │   └── int is_solved()         # ← Valida solução
│   │
│   └── get_next_line.c             # Leitura linha-por-linha de ficheiros
│       └── char *get_next_line()   # ← Função pública
│
├── validator/
│   ├── loader.c                    # Carregamento completo de Sudoku
│   │   └── t_sudoku *load_sudoku() # ← Função pública
│   │
│   └── parser.c                    # Parsing de linhas individuais
│       ├── int parser_parse_order_line()
│       ├── int parser_compute_grid_size()
│       └── int parser_parse_board_line()
│
├── obj/                            # Directório com ficheiros objeto (.o)
│   ├── sudoku_serial.o             # ← Compilado de sudoku_serial.c
│   ├── sudoku_omp.o                # ← Compilado de sudoku_omp.c
│   ├── sudoku_mpi.o                # ← Compilado de sudoku_mpi.c
│   ├── backtracking.o              # ← Compilado de backtracking.c
│   ├── backtracking_omp.o          # ← Compilado de backtracking_omp.c (partilhado OMP+MPI)
│   ├── utils.o                     # ← Compilado de utils.c (versão serial)
│   ├── utils_omp.o                 # ← Compilado de utils.c (versão OMP)
│   ├── utils_mpi.o                 # ← Compilado de utils.c (versão MPI)
│   ├── loader.o                    # ← Compilado de loader.c (versão serial)
│   ├── loader_omp.o                # ← Compilado de loader.c (versão OMP)
│   ├── loader_mpi.o                # ← Compilado de loader.c (versão MPI)
│   ├── parser.o                    # ← Compilado de parser.c (versão serial)
│   ├── parser_omp.o                # ← Compilado de parser.c (versão OMP)
│   ├── parser_mpi.o                # ← Compilado de parser.c (versão MPI)
│   ├── get_next_line.o             # ← Compilado de get_next_line.c (versão serial)
│   ├── get_next_line_omp.o         # ← Compilado de get_next_line.c (versão OMP)
│   └── get_next_line_mpi.o         # ← Compilado de get_next_line.c (versão MPI)
│
├── Sample instances/               # Ficheiros de teste Sudoku
│   ├── 9x9.txt                     # ← Sudoku 9×9 com solução
│   ├── 9x9-nosol.txt               # ← Sudoku 9×9 sem solução
│   ├── 16x16.txt                   # ← Sudoku 16×16 com solução
│   ├── 16x16-nosol.txt             # ← Sudoku 16×16 sem solução
│   └── 16x16-zeros.txt             # ← Sudoku 16×16 vazio
│
└── PASSO*.md                       # Documentação do projeto (análises, relatórios)
    ├── PASSO5.5_INVENTARIO.md
    ├── PASSO5.5_RELATORIO_FINAL.md
    ├── PASSO5.5_SUMARIO.md
    ├── PASSO5.5_CHECKLIST.md
    └── PASSO5.6_ANALISE_DEPENDENCIAS.md
```

**Problema identificado**: `sudoku_mpi.c` com **1063 linhas** concentra todas as funções MPI.

---

**NOTA IMPORTANTE**:
- O Makefile compila os mesmos ficheiros `.c` múltiplas vezes com diferentes compiladores (gcc vs mpicc) para gerar `.o` separados
- `backtracking_omp.o` é **partilhado** entre `sudoku_omp.exe` e `sudoku_mpi.exe`
- `utils.c`, `loader.c`, `parser.c`, `get_next_line.c` são compilados 3 vezes cada (serial, OMP, MPI)

---

### 5.2 — Estrutura Proposta (OPÇÃO 1 — Modular Completa)

```
sudoku_mpi/
├── .git/                           # Controlo de versões Git (INALTERADO)
│   └── (ficheiros internos do git)
│
├── .gitignore                      # Ficheiros ignorados pelo Git (INALTERADO)
│
├── Makefile                        # ← ALTERADO: adicionar 4 novas regras de compilação
│
├── sudoku.h                        # Header principal (INALTERADO)
│
├── sudoku_serial.c                 # Main serial (42 linhas) — (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_serial.exe               # Executável serial (INALTERADO)
│
├── sudoku_omp.c                    # Main OpenMP (42 linhas) — (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_omp.exe                  # Executável OpenMP (INALTERADO)
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (~300 linhas) ← REDUZIDO de 1063
│   ├── configure_openmp()          # ~50 linhas (mantido)
│   ├── static void print_usage()   # ~10 linhas (mantido)
│   └── int main()                  # ~150 linhas (chama funções de mpi_utils/)
│   │
│   │   REMOVIDAS DAQUI:
│   │   ❌ flatten_board() / unflatten_board()
│   │   ❌ send_subproblem() / recv_subproblem()
│   │   ❌ send_solution() / recv_solution()
│   │   ❌ find_first_empty()
│   │   ❌ generate_subproblems_mpi()
│   │   ❌ dispatch_work()
│   │   ❌ worker_loop()
│   │   ❌ collect_solutions()
│   │   ❌ broadcast_termination()
│
├── sudoku_mpi.exe                  # Executável MPI+OpenMP (recompilado com novos links)
│
├── backtracking/                   # (INALTERADO)
│   ├── backtracking.c              # Solver serial (227 linhas)
│   │   ├── static helpers (7 funções)
│   │   └── void solve()
│   │
│   └── backtracking_omp.c          # Solver OpenMP paralelo (456 linhas)
│       ├── static helpers (9 funções)
│       └── void solve_omp()
│
├── general_utils/                  # (INALTERADO)
│   ├── utils.c                     # Utilitários partilhados (197 linhas)
│   │   ├── void print()
│   │   ├── void free_sudoku()
│   │   ├── int **alloc_board()
│   │   ├── void free_board()
│   │   ├── void copy_board()
│   │   └── int is_solved()
│   │
│   └── get_next_line.c             # Leitura linha-por-linha de ficheiros
│       └── char *get_next_line()
│
├── validator/                      # (INALTERADO)
│   ├── loader.c                    # Carregamento completo de Sudoku
│   │   └── t_sudoku *load_sudoku()
│   │
│   └── parser.c                    # Parsing de linhas individuais
│       ├── int parser_parse_order_line()
│       ├── int parser_compute_grid_size()
│       └── int parser_parse_board_line()
│
├── mpi_utils/                      # ← NOVO DIRETÓRIO
│   │
│   ├── mpi_serialization.c         # ← NOVO (~100 linhas)
│   │   ├── void flatten_board()    # ← Movido de sudoku_mpi.c
│   │   └── int **unflatten_board() # ← Movido de sudoku_mpi.c
│   │
│   ├── mpi_communication.c         # ← NOVO (~300 linhas)
│   │   ├── void send_subproblem()  # ← Movido de sudoku_mpi.c
│   │   ├── int **recv_subproblem() # ← Movido de sudoku_mpi.c
│   │   ├── void send_solution()    # ← Movido de sudoku_mpi.c
│   │   └── int **recv_solution()   # ← Movido de sudoku_mpi.c
│   │
│   ├── mpi_distribution.c          # ← NOVO (~200 linhas)
│   │   ├── int find_first_empty()  # ← Movido de sudoku_mpi.c
│   │   ├── int generate_subproblems_mpi() # ← Movido de sudoku_mpi.c
│   │   └── void dispatch_work()    # ← Movido de sudoku_mpi.c
│   │
│   ├── mpi_worker.c                # ← NOVO (~200 linhas)
│   │   ├── void worker_loop()      # ← Movido de sudoku_mpi.c
│   │   ├── void collect_solutions() # ← Movido de sudoku_mpi.c
│   │   └── void broadcast_termination() # ← Movido de sudoku_mpi.c
│   │
│   └── mpi_utils.h                 # ← NOVO HEADER
│       └── (declarações de todas as 13 funções movidas acima)
│
├── obj/                            # Directório com ficheiros objeto
│   ├── sudoku_serial.o             # (INALTERADO)
│   ├── sudoku_omp.o                # (INALTERADO)
│   ├── sudoku_mpi.o                # ← RECOMPILADO (agora mais pequeno)
│   ├── backtracking.o              # (INALTERADO)
│   ├── backtracking_omp.o          # (INALTERADO)
│   ├── utils.o                     # (INALTERADO)
│   ├── utils_omp.o                 # (INALTERADO)
│   ├── utils_mpi.o                 # (INALTERADO)
│   ├── loader.o                    # (INALTERADO)
│   ├── loader_omp.o                # (INALTERADO)
│   ├── loader_mpi.o                # (INALTERADO)
│   ├── parser.o                    # (INALTERADO)
│   ├── parser_omp.o                # (INALTERADO)
│   ├── parser_mpi.o                # (INALTERADO)
│   ├── get_next_line.o             # (INALTERADO)
│   ├── get_next_line_omp.o         # (INALTERADO)
│   ├── get_next_line_mpi.o         # (INALTERADO)
│   ├── mpi_serialization.o         # ← NOVO
│   ├── mpi_communication.o         # ← NOVO
│   ├── mpi_distribution.o          # ← NOVO
│   └── mpi_worker.o                # ← NOVO
│
├── Sample instances/               # Ficheiros de teste (INALTERADOS)
│   ├── 9x9.txt
│   ├── 9x9-nosol.txt
│   ├── 16x16.txt
│   ├── 16x16-nosol.txt
│   └── 16x16-zeros.txt
│
└── PASSO*.md                       # Documentação do projeto (INALTERADOS)
    ├── PASSO5.5_INVENTARIO.md
    ├── PASSO5.5_RELATORIO_FINAL.md
    ├── PASSO5.5_SUMARIO.md
    ├── PASSO5.5_CHECKLIST.md
    └── PASSO5.6_ANALISE_DEPENDENCIAS.md
```

**Alterações**:
- ✓ Criado diretório `mpi_utils/`
- ✓ Criados 4 novos ficheiros `.c`
- ✓ Criado 1 novo header `mpi_utils.h`
- ✓ `sudoku_mpi.c`: 1063 → ~300 linhas
- ✓ Makefile: adicionar 4 novas regras

**Ficheiros INALTERADOS**:
- ✓ `sudoku_serial.c`
- ✓ `sudoku_omp.c`
- ✓ `sudoku.h`
- ✓ `backtracking/`
- ✓ `general_utils/`
- ✓ `validator/`

**Benefícios**:
- Separação clara de responsabilidades
- Cada módulo <350 linhas
- Reutilização futura facilitada
- Testes modulares possíveis

**Desvantagens**:
- 4 novos ficheiros para manter
- Alterações no Makefile mais complexas

---

### 5.3 — Estrutura Proposta (OPÇÃO 2 — Minimalista)

```
sudoku_mpi/
├── .git/                           # Controlo de versões Git (INALTERADO)
│   └── (ficheiros internos do git)
│
├── .gitignore                      # Ficheiros ignorados pelo Git (INALTERADO)
│
├── Makefile                        # ← ALTERADO: adicionar 1 nova regra de compilação
│
├── sudoku.h                        # Header principal (INALTERADO)
│
├── sudoku_serial.c                 # Main serial (42 linhas) — (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_serial.exe               # Executável serial (INALTERADO)
│
├── sudoku_omp.c                    # Main OpenMP (42 linhas) — (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_omp.exe                  # Executável OpenMP (INALTERADO)
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (~400 linhas) ← REDUZIDO de 1063
│   ├── configure_openmp()          # ~50 linhas (mantido)
│   ├── worker_loop()               # ~50 linhas (mantido - controlo alto nível)
│   ├── collect_solutions()         # ~40 linhas (mantido - controlo alto nível)
│   ├── broadcast_termination()     # ~20 linhas (mantido - controlo alto nível)
│   ├── static void print_usage()   # ~10 linhas (mantido)
│   └── int main()                  # ~150 linhas (mantido)
│   │
│   │   REMOVIDAS DAQUI (movidas para mpi_core.c):
│   │   ❌ flatten_board() / unflatten_board()
│   │   ❌ send_subproblem() / recv_subproblem()
│   │   ❌ send_solution() / recv_solution()
│   │   ❌ find_first_empty()
│   │   ❌ generate_subproblems_mpi()
│   │   ❌ dispatch_work()
│
├── sudoku_mpi.exe                  # Executável MPI+OpenMP (recompilado com novo link)
│
├── backtracking/                   # (INALTERADO)
│   ├── backtracking.c              # Solver serial (227 linhas)
│   │   ├── static helpers (7 funções)
│   │   └── void solve()
│   │
│   └── backtracking_omp.c          # Solver OpenMP paralelo (456 linhas)
│       ├── static helpers (9 funções)
│       └── void solve_omp()
│
├── general_utils/                  # (INALTERADO)
│   ├── utils.c                     # Utilitários partilhados (197 linhas)
│   │   ├── void print()
│   │   ├── void free_sudoku()
│   │   ├── int **alloc_board()
│   │   ├── void free_board()
│   │   ├── void copy_board()
│   │   └── int is_solved()
│   │
│   └── get_next_line.c             # Leitura linha-por-linha de ficheiros
│       └── char *get_next_line()
│
├── validator/                      # (INALTERADO)
│   ├── loader.c                    # Carregamento completo de Sudoku
│   │   └── t_sudoku *load_sudoku()
│   │
│   └── parser.c                    # Parsing de linhas individuais
│       ├── int parser_parse_order_line()
│       ├── int parser_compute_grid_size()
│       └── int parser_parse_board_line()
│
├── mpi_utils/                      # ← NOVO DIRETÓRIO
│   │
│   ├── mpi_core.c                  # ← NOVO (~500 linhas)
│   │   │                           # Contém TODAS as funções de baixo nível MPI:
│   │   │
│   │   ├── void flatten_board()    # ← Movido de sudoku_mpi.c
│   │   ├── int **unflatten_board() # ← Movido de sudoku_mpi.c
│   │   ├── void send_subproblem()  # ← Movido de sudoku_mpi.c
│   │   ├── int **recv_subproblem() # ← Movido de sudoku_mpi.c
│   │   ├── void send_solution()    # ← Movido de sudoku_mpi.c
│   │   ├── int **recv_solution()   # ← Movido de sudoku_mpi.c
│   │   ├── int find_first_empty()  # ← Movido de sudoku_mpi.c
│   │   ├── int generate_subproblems_mpi() # ← Movido de sudoku_mpi.c
│   │   └── void dispatch_work()    # ← Movido de sudoku_mpi.c
│   │
│   └── mpi_core.h                  # ← NOVO HEADER
│       └── (declarações das 9 funções movidas acima)
│
├── obj/                            # Directório com ficheiros objeto
│   ├── sudoku_serial.o             # (INALTERADO)
│   ├── sudoku_omp.o                # (INALTERADO)
│   ├── sudoku_mpi.o                # ← RECOMPILADO (agora mais pequeno)
│   ├── backtracking.o              # (INALTERADO)
│   ├── backtracking_omp.o          # (INALTERADO)
│   ├── utils.o                     # (INALTERADO)
│   ├── utils_omp.o                 # (INALTERADO)
│   ├── utils_mpi.o                 # (INALTERADO)
│   ├── loader.o                    # (INALTERADO)
│   ├── loader_omp.o                # (INALTERADO)
│   ├── loader_mpi.o                # (INALTERADO)
│   ├── parser.o                    # (INALTERADO)
│   ├── parser_omp.o                # (INALTERADO)
│   ├── parser_mpi.o                # (INALTERADO)
│   ├── get_next_line.o             # (INALTERADO)
│   ├── get_next_line_omp.o         # (INALTERADO)
│   ├── get_next_line_mpi.o         # (INALTERADO)
│   └── mpi_core.o                  # ← NOVO
│
├── Sample instances/               # Ficheiros de teste (INALTERADOS)
│   ├── 9x9.txt
│   ├── 9x9-nosol.txt
│   ├── 16x16.txt
│   ├── 16x16-nosol.txt
│   └── 16x16-zeros.txt
│
└── PASSO*.md                       # Documentação do projeto (INALTERADOS)
    ├── PASSO5.5_INVENTARIO.md
    ├── PASSO5.5_RELATORIO_FINAL.md
    ├── PASSO5.5_SUMARIO.md
    ├── PASSO5.5_CHECKLIST.md
    └── PASSO5.6_ANALISE_DEPENDENCIAS.md
```

**Alterações**:
- ✓ Criado diretório `mpi_utils/`
- ✓ Criado 1 novo ficheiro `mpi_core.c`
- ✓ Criado 1 novo header `mpi_core.h`
- ✓ `sudoku_mpi.c`: 1063 → ~400 linhas
- ✓ Makefile: adicionar 1 nova regra

**Ficheiros INALTERADOS**:
- ✓ `sudoku_serial.c`
- ✓ `sudoku_omp.c`
- ✓ `sudoku.h`
- ✓ `backtracking/`
- ✓ `general_utils/`
- ✓ `validator/`

**Benefícios**:
- Menos ficheiros que OPÇÃO 1
- Alterações concentradas
- Menor risco
- `sudoku_mpi.c` ainda gerível (~400 linhas)

**Desvantagens**:
- `mpi_core.c` ainda grande (~500 linhas)
- Responsabilidades misturadas no core

---

### 5.4 — Estrutura Proposta (OPÇÃO 3 — Status Quo)

```
sudoku_mpi/
├── .git/
├── .gitignore
├── Makefile                        # INALTERADO
│
├── sudoku.h                        # Header principal (INALTERADO)
│
├── sudoku_serial.c                 # Main serial (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_omp.c                    # Main OpenMP (INALTERADO)
│   ├── static void print_usage()
│   └── int main()
│
├── sudoku_mpi.c                    # Main MPI+OpenMP (1063 linhas) ← INALTERADO
│   ├── configure_openmp()
│   ├── flatten_board() / unflatten_board()
│   ├── send_*() / recv_*()
│   ├── generate_subproblems_mpi()
│   ├── dispatch_work()
│   ├── worker_loop()
│   ├── collect_solutions()
│   ├── broadcast_termination()
│   ├── find_first_empty()
│   ├── static void print_usage()
│   └── int main()
│
├── backtracking/                   # INALTERADO
│   ├── backtracking.c
│   └── backtracking_omp.c
│
├── general_utils/                  # INALTERADO
│   ├── utils.c
│   └── get_next_line.c
│
├── validator/                      # INALTERADO
│   ├── loader.c
│   └── parser.c
│
├── obj/                            # INALTERADO
│   └── *.o
│
├── Sample instances/               # INALTERADO
└── PASSO*.md                       # INALTERADO
```

**Alterações**: **NENHUMA**

**Benefícios**:
- Zero risco
- Código já validado e testado
- Nenhuma alteração no Makefile
- Nenhum novo ficheiro
- Pronto para PASSO 6 (benchmark) imediatamente

**Desvantagens**:
- `sudoku_mpi.c` com 1063 linhas (ficheiro grande)
- Todas as responsabilidades MPI num só ficheiro
- Mais difícil de testar individualmente

---

## FASE 6 — ANÁLISE DE RISCO

### 6.1 — Risco para `sudoku_serial.c`

| Opção | Risco | Justificação |
|-------|:-----:|--------------|
| OPÇÃO 1 (Modular) | **BAIXO** | Não usa funções MPI |
| OPÇÃO 2 (Minimalista) | **BAIXO** | Não usa funções MPI |
| OPÇÃO 3 (Status Quo) | **ZERO** | Nenhuma alteração |

**Dependências de `sudoku_serial.c`**:
- `load_sudoku()` ✓ (permanece em validator/)
- `solve()` ✓ (permanece em backtracking/)
- `print()` ✓ (permanece em utils.c)
- `free_sudoku()` ✓ (permanece em utils.c)

**Conclusão**: ✓ **Nenhuma movimentação afecta sudoku_serial.c**

---

### 6.2 — Risco para `sudoku_omp.c`

| Opção | Risco | Justificação |
|-------|:-----:|--------------|
| OPÇÃO 1 (Modular) | **BAIXO** | Não usa funções MPI |
| OPÇÃO 2 (Minimalista) | **BAIXO** | Não usa funções MPI |
| OPÇÃO 3 (Status Quo) | **ZERO** | Nenhuma alteração |

**Dependências de `sudoku_omp.c`**:
- `load_sudoku()` ✓ (permanece em validator/)
- `solve_omp()` ✓ (permanece em backtracking_omp.c)
- `print()` ✓ (permanece em utils.c)
- `free_sudoku()` ✓ (permanece em utils.c)

**Conclusão**: ✓ **Nenhuma movimentação afecta sudoku_omp.c**

---

### 6.3 — Risco para `Makefile`

| Opção | Risco | Justificação |
|-------|:-----:|--------------|
| OPÇÃO 1 (Modular) | **MÉDIO** | Requer adicionar 4 novos ficheiros .c |
| OPÇÃO 2 (Minimalista) | **BAIXO** | Requer adicionar 1 novo ficheiro .c |
| OPÇÃO 3 (Status Quo) | **ZERO** | Nenhuma alteração |

**Alterações necessárias**:

#### OPÇÃO 1:
```makefile
# Adicionar novos objectos
MPI_OBJS = $(OBJ_DIR)/sudoku_mpi.o \
           $(OBJ_DIR)/mpi_serialization.o \
           $(OBJ_DIR)/mpi_communication.o \
           $(OBJ_DIR)/mpi_distribution.o \
           $(OBJ_DIR)/mpi_worker.o \
           ...

# Adicionar regras de compilação
$(OBJ_DIR)/mpi_serialization.o: mpi_utils/mpi_serialization.c
    $(MPICC) $(CFLAGS) -c $< -o $@
...
```

**Complexidade**: Moderada (4 novas regras)

#### OPÇÃO 2:
```makefile
# Adicionar novo objeto
MPI_OBJS = $(OBJ_DIR)/sudoku_mpi.o \
           $(OBJ_DIR)/mpi_core.o \
           ...

# Adicionar regra
$(OBJ_DIR)/mpi_core.o: mpi_utils/mpi_core.c
    $(MPICC) $(CFLAGS) -c $< -o $@
```

**Complexidade**: Baixa (1 nova regra)

---

### 6.4 — Risco para Headers

| Opção | Risco | Justificação |
|-------|:-----:|--------------|
| OPÇÃO 1 (Modular) | **MÉDIO** | Requer novo header `mpi_utils.h` |
| OPÇÃO 2 (Minimalista) | **BAIXO** | Requer novo header `mpi_core.h` |
| OPÇÃO 3 (Status Quo) | **ZERO** | Nenhuma alteração |

**Novo header necessário**:

#### OPÇÃO 1 — `mpi_utils/mpi_utils.h`:
```c
#ifndef MPI_UTILS_H
# define MPI_UTILS_H

#include "sudoku.h"
#include <mpi.h>

// Serialization
void flatten_board(int **board, int size, int *buffer);
int **unflatten_board(int *buffer, int size);

// Communication
void send_subproblem(int **board, int order, int dest_rank);
int **recv_subproblem(int order);
void send_solution(int **board, int order);
int **recv_solution(int order);

// Distribution
int generate_subproblems_mpi(...);
void dispatch_work(...);

// Worker
void worker_loop(int order);
void collect_solutions(...);
void broadcast_termination(int nprocs);

#endif
```

**Risco**: Baixo (header simples)

---

### 6.5 — Risco de Quebra Funcional

| Opção | Risco | Justificação |
|-------|:-----:|--------------|
| OPÇÃO 1 (Modular) | **MÉDIO** | 13 funções movidas, múltiplos ficheiros |
| OPÇÃO 2 (Minimalista) | **BAIXO** | 9 funções movidas, 1 ficheiro |
| OPÇÃO 3 (Status Quo) | **ZERO** | Nenhuma alteração |

**Factores de risco**:
1. Erro ao mover código
2. Erro em protótipos de funções
3. Erro em includes
4. Erro no Makefile
5. Dependências circulares não detectadas

**Mitigação**:
- Testes de regressão após cada movimentação
- Compilação incremental
- Validação com testes existentes

---

## ANÁLISE FINAL E RECOMENDAÇÃO

### Comparação de Opções

| Critério | OPÇÃO 1 | OPÇÃO 2 | OPÇÃO 3 |
|----------|:-------:|:-------:|:-------:|
| Modularidade | ★★★★★ | ★★★☆☆ | ★☆☆☆☆ |
| Risco | MÉDIO | BAIXO | ZERO |
| Esforço | ALTO | MÉDIO | ZERO |
| Manutenibilidade | ★★★★★ | ★★★★☆ | ★★☆☆☆ |
| Testabilidade | ★★★★★ | ★★★☆☆ | ★★☆☆☆ |

### Recomendação

**OPÇÃO 2 — MINIMALISTA** (BAIXO RISCO, MÉDIO BENEFÍCIO)

**Justificação**:
1. ✓ Reduz `sudoku_mpi.c` de 1063 → ~400 linhas
2. ✓ Separa lógica MPI core (serialização + comunicação + distribuição)
3. ✓ Mantém controlo de alto nível em `sudoku_mpi.c`
4. ✓ Risco BAIXO para sudoku_serial.c e sudoku_omp.c
5. ✓ Alterações no Makefile mínimas
6. ✓ Facilita testes unitários futuros

**Alternativa**:
Se o objectivo é **preparar para PASSO 6 (benchmark)** sem riscos:
→ **OPÇÃO 3 — STATUS QUO**

Código já está limpo (PASSO 5.5), funcional e validado.
Refactorização pode ser feita **APÓS** benchmark, quando tiver dados de performance como baseline.

---

## CONCLUSÃO

### Dependências Críticas Identificadas

✓ **`sudoku_serial.c`**: Independente de funções MPI  
✓ **`sudoku_omp.c`**: Independente de funções MPI  
✓ **`sudoku_mpi.c`**: Depende fortemente de `general_utils/` e `backtracking_omp.c`

### Funções Seguras para Mover

✓ **13 funções exclusivas MPI** podem ser movidas sem afetar serial/omp

### Recomendação Final

**Para PASSO 6 (Benchmark)**: Manter **OPÇÃO 3 (Status Quo)**  
**Para modularização futura**: Usar **OPÇÃO 2 (Minimalista)**

---

**STATUS**: ✓ **ANÁLISE CONCLUÍDA**  
**PRÓXIMO PASSO**: Decisão sobre refactorização (Status Quo vs Minimalista)
