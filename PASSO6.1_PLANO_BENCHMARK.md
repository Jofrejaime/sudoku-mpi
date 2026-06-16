# PASSO 6.1 — PLANO DE IMPLEMENTAÇÃO DO BENCHMARK MODULAR

**Data**: 2026-06-16  
**Objectivo**: Planear implementação modular de benchmark sem alterar lógica do solver  
**Status**: ⚠️ **APENAS PLANEAMENTO** — NENHUM CÓDIGO SERÁ GERADO AINDA

---

## 1. ESTRUTURA DOS NOVOS FICHEIROS

### 1.1 — Árvore de Ficheiros (Após Implementação)

```
sudoku_mpi/
├── .git/
├── .gitignore
├── Makefile                        # ← ALTERADO (adicionar regras benchmark/)
│
├── sudoku.h                        # (INALTERADO)
├── sudoku_serial.c                 # (INALTERADO)
├── sudoku_serial.exe
├── sudoku_omp.c                    # (INALTERADO)
├── sudoku_omp.exe
├── sudoku_mpi.c                    # ← ALTERADO (integrar benchmark)
├── sudoku_mpi.exe
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
├── benchmark/                      # ← NOVO DIRETÓRIO
│   ├── benchmark.h                 # ← NOVO (~30 linhas)
│   └── benchmark.c                 # ← NOVO (~80 linhas)
│
├── obj/
│   ├── (objectos existentes)
│   └── benchmark.o                 # ← NOVO
│
├── Sample instances/
└── PASSO*.md
```

---

## 2. ESPECIFICAÇÃO: `benchmark/benchmark.h`

### 2.1 — Conteúdo Completo

```c
#ifndef BENCHMARK_H
# define BENCHMARK_H

# include <mpi.h>

/* ========================================================================
 * BENCHMARK MODULE - MPI+OpenMP Hybrid
 * 
 * Módulo independente para medição de performance em código híbrido.
 * NÃO deve ser misturado com lógica de solver.
 * 
 * Uso:
 *   benchmark_start();
 *   // ... código a medir ...
 *   benchmark_stop();
 *   double elapsed = benchmark_get_elapsed();
 *   benchmark_print_report(elapsed, nprocs, nthreads);
 * ======================================================================== */

/* ========================================================================
 * FUNÇÃO: benchmark_start
 * 
 * Inicia a medição de tempo usando MPI_Wtime().
 * Deve ser chamada ANTES do código a medir.
 * 
 * IMPORTANTE:
 *   - Usar após MPI_Init()
 *   - Considerar adicionar MPI_Barrier() antes para sincronização
 *   - Thread-safe (pode ser chamada em ambiente OpenMP)
 * 
 * @return: void
 * ======================================================================== */
void benchmark_start(void);

/* ========================================================================
 * FUNÇÃO: benchmark_stop
 * 
 * Para a medição de tempo usando MPI_Wtime().
 * Deve ser chamada APÓS o código a medir.
 * 
 * @return: void
 * ======================================================================== */
void benchmark_stop(void);

/* ========================================================================
 * FUNÇÃO: benchmark_get_elapsed
 * 
 * Retorna o tempo decorrido entre start e stop.
 * 
 * @return: Tempo em segundos (double) com alta precisão (microsegundos)
 * 
 * PRÉ-CONDIÇÃO: benchmark_start() e benchmark_stop() já foram chamadas
 * ======================================================================== */
double benchmark_get_elapsed(void);

/* ========================================================================
 * FUNÇÃO: benchmark_print_report
 * 
 * Imprime relatório formatado de benchmark.
 * 
 * @elapsed_time: Tempo de execução em segundos
 * @mpi_processes: Número de processos MPI
 * @omp_threads: Número de threads OpenMP por processo
 * 
 * Formato de saída:
 *   ====================================
 *   MPI+OMP BENCHMARK
 *   MPI Processes : 4
 *   OMP Threads   : 2
 *   Total Workers : 8
 *   Execution Time: 0.532841 s
 *   ====================================
 * 
 * IMPORTANTE:
 *   - Imprime para stderr (não stdout, para não misturar com solução)
 *   - Deve ser chamada APENAS por Rank 0
 *   - NÃO calcula speedup ou eficiência (apenas imprime tempo)
 * 
 * @return: void
 * ======================================================================== */
void benchmark_print_report(double elapsed_time, int mpi_processes, int omp_threads);

#endif /* BENCHMARK_H */
```

### 2.2 — Análise do Header

**Linhas de código**: ~30 linhas (incluindo comentários extensos)

**Dependências**:
- `<mpi.h>` — Para `MPI_Wtime()`

**Funções exportadas**: 4 funções públicas

**Estado interno**: Privado (variáveis estáticas em `.c`)

---


## 3. ESPECIFICAÇÃO: `benchmark/benchmark.c`

### 3.1 — Estrutura Interna

**Variáveis estáticas** (estado privado):
```c
static double start_time = 0.0;
static double end_time = 0.0;
```

**Funções**:
1. `benchmark_start()` — ~5 linhas
2. `benchmark_stop()` — ~5 linhas
3. `benchmark_get_elapsed()` — ~5 linhas
4. `benchmark_print_report()` — ~25 linhas

**Total**: ~80 linhas (incluindo comentários)

---

### 3.2 — Pseudo-código das Funções

#### **benchmark_start()**
```c
void benchmark_start(void)
{
    start_time = MPI_Wtime();
}
```

**Comportamento**:
- Captura timestamp atual usando `MPI_Wtime()`
- Armazena em variável estática `start_time`
- Não valida se MPI foi inicializado (responsabilidade do caller)

**Complexidade**: O(1)

---

#### **benchmark_stop()**
```c
void benchmark_stop(void)
{
    end_time = MPI_Wtime();
}
```

**Comportamento**:
- Captura timestamp atual usando `MPI_Wtime()`
- Armazena em variável estática `end_time`

**Complexidade**: O(1)

---

#### **benchmark_get_elapsed()**
```c
double benchmark_get_elapsed(void)
{
    return (end_time - start_time);
}
```

**Comportamento**:
- Calcula diferença entre `end_time` e `start_time`
- Retorna resultado em segundos

**Pré-condições**:
- `benchmark_start()` foi chamada
- `benchmark_stop()` foi chamada

**Pós-condições**:
- Se pré-condições satisfeitas: retorna tempo positivo
- Se pré-condições NÃO satisfeitas: comportamento indefinido

**Complexidade**: O(1)

---

#### **benchmark_print_report()**
```c
void benchmark_print_report(double elapsed_time, int mpi_processes, int omp_threads)
{
    int total_workers = mpi_processes * omp_threads;
    
    fprintf(stderr, "\n");
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "MPI+OMP BENCHMARK\n");
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "MPI Processes : %d\n", mpi_processes);
    fprintf(stderr, "OMP Threads   : %d\n", omp_threads);
    fprintf(stderr, "Total Workers : %d\n", total_workers);
    fprintf(stderr, "Execution Time: %.6f s\n", elapsed_time);
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "\n");
}
```

**Comportamento**:
- Calcula `total_workers = mpi_processes * omp_threads`
- Imprime relatório formatado para `stderr`
- Usa formato `%.6f` para tempo (6 casas decimais)

**Importante**:
- **NÃO calcula speedup** (não tem acesso a T_serial)
- **NÃO calcula eficiência** (não tem acesso a T_serial)
- Apenas imprime métricas básicas
- Deve ser chamada **APENAS por Rank 0**

**Complexidade**: O(1)

---

### 3.3 — Dependências do Módulo

**Headers necessários**:
```c
#include <stdio.h>    // fprintf, stderr
#include <mpi.h>      // MPI_Wtime
#include "benchmark.h"
```

**Funções externas usadas**:
- `MPI_Wtime()` — Medição de tempo
- `fprintf()` — Output formatado

**Bibliotecas de linking**:
- MPI (implícito ao usar `mpicc`)

---


## 4. INTEGRAÇÃO EM `sudoku_mpi.c`

### 4.1 — Modificações Necessárias

**Linha ~20** (após includes existentes):
```c
#include "benchmark/benchmark.h"  // ← ADICIONAR
```

**Função `main()` — Rank 0**:

**Localização da medição**:
```c
// ========== PONTO 1: APÓS CARREGAR SUDOKU ==========
sudoku = load_sudoku(argv[1]);
if (!sudoku) { ... }

size = sudoku->order * sudoku->order;

// ========== ADICIONAR BARREIRA + START ==========
MPI_Barrier(MPI_COMM_WORLD);  // Sincronizar todos os processos
benchmark_start();             // Iniciar medição
// ================================================

// Gerar subproblemas
ret = generate_subproblems_mpi(...);
if (ret != 0 || subproblems_count == 0) { ... }

// Broadcast parâmetros
MPI_Bcast(&sudoku->order, ...);
MPI_Bcast(&subproblems_count, ...);

// Distribuir trabalho
dispatch_work(...);

// Resolver localmente
rank0_board = subproblems[0];
solve_omp(rank0_board, sudoku->order);

// Verificar solução local
if (is_solved(...)) {
    // ... copiar solução ...
} else {
    // Recolher de workers
    collect_solutions(...);
}

// Enviar terminação
broadcast_termination(nprocs);

// ========== PONTO 2: APÓS TERMINAÇÃO ==========
benchmark_stop();              // Parar medição
// ================================================

// ========== PONTO 3: OBTER E IMPRIMIR ==========
double elapsed = benchmark_get_elapsed();
int omp_threads = omp_get_max_threads();

benchmark_print_report(elapsed, nprocs, omp_threads);
// ================================================

// Imprimir solução
fprintf(stderr, "\n");
if (solution_found && solution_board) {
    fprintf(stderr, "SOLUTION FOUND\n");
    // ...
}
```

---

### 4.2 — Resumo das Alterações em `sudoku_mpi.c`

| Localização | Tipo | Linhas |
|-------------|------|-------:|
| Includes (topo) | Adicionar `#include "benchmark/benchmark.h"` | 1 |
| Rank 0: Antes de gerar subproblemas | `MPI_Barrier()` + `benchmark_start()` | 2 |
| Rank 0: Após `broadcast_termination()` | `benchmark_stop()` | 1 |
| Rank 0: Após stop | Obter elapsed e threads | 2 |
| Rank 0: Imprimir relatório | `benchmark_print_report()` | 1 |

**Total de linhas adicionadas**: ~7 linhas

**Linhas modificadas**: 0 (apenas adições)

---

### 4.3 — O Que NÃO Será Alterado

✅ **Funções que permanecem intocadas**:
- `configure_openmp()`
- `flatten_board()` / `unflatten_board()`
- `send_subproblem()` / `recv_subproblem()`
- `send_solution()` / `recv_solution()`
- `generate_subproblems_mpi()`
- `dispatch_work()`
- `worker_loop()`
- `collect_solutions()`
- `broadcast_termination()`
- `find_first_empty()`

✅ **Ficheiros que permanecem intocados**:
- `sudoku_serial.c`
- `sudoku_omp.c`
- `backtracking.c`
- `backtracking_omp.c`
- `validator/*`
- `general_utils/*`

---

### 4.4 — Workers (Ranks > 0)

**NENHUMA alteração necessária** nos workers.

**Razão**:
- Workers não precisam medir tempo (apenas Rank 0)
- `MPI_Barrier()` sincroniza automaticamente todos os processos
- Workers continuam funcionando exatamente como antes

---


## 5. MODIFICAÇÕES NO `Makefile`

### 5.1 — Alterações Necessárias

#### **Adicionar regra de compilação** (após regras MPI existentes):

```makefile
# ============================================
# BENCHMARK COMPILATION
# ============================================

$(OBJ_DIR)/benchmark.o: benchmark/benchmark.c benchmark/benchmark.h
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS_OPT) -c $< -o $@
```

#### **Modificar target MPI** (adicionar benchmark.o):

```makefile
# ANTES:
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/backtracking_omp.o \
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o
	$(MPICC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ MPI version: $(MPI_NAME)"

# DEPOIS:
$(MPI_NAME): $(OBJ_DIR)/sudoku_mpi.o \
             $(OBJ_DIR)/benchmark.o \
             $(OBJ_DIR)/backtracking_omp.o \
             $(OBJ_DIR)/get_next_line_mpi.o \
             $(OBJ_DIR)/utils_mpi.o \
             $(OBJ_DIR)/parser_mpi.o \
             $(OBJ_DIR)/loader_mpi.o
	$(MPICC) $(CFLAGS_OPT) $^ -o $@
	@echo "✓ MPI version: $(MPI_NAME)"
```

---

### 5.2 — Resumo das Alterações no Makefile

| Alteração | Tipo | Linhas |
|-----------|------|-------:|
| Nova regra `benchmark.o` | Adicionar | 4 |
| Modificar target `$(MPI_NAME)` | Adicionar dependência | 1 |

**Total**: ~5 linhas modificadas/adicionadas

**Impacto**: 
- ✅ NENHUM impacto em Serial ou OMP
- ✅ Apenas adiciona novo objeto ao linking MPI

---

## 6. ANÁLISE DE DEPENDÊNCIAS

### 6.1 — Dependências do Módulo `benchmark/`

```
benchmark/benchmark.c
├── <stdio.h>          (stdlib C)
├── <mpi.h>            (MPI)
└── "benchmark.h"      (próprio header)

benchmark/benchmark.h
└── <mpi.h>            (MPI)
```

**Dependências externas**: Apenas MPI (já usada em `sudoku_mpi.c`)

**Dependências internas**: NENHUMA
- NÃO depende de `sudoku.h`
- NÃO depende de `general_utils/`
- NÃO depende de `backtracking/`
- NÃO depende de `validator/`

**Conclusão**: ✅ Módulo **completamente independente** do resto do código

---

### 6.2 — Dependências de `sudoku_mpi.c` (Após Integração)

```
sudoku_mpi.c
├── <stdio.h>
├── <stdlib.h>
├── <mpi.h>
├── <omp.h>
├── "sudoku.h"
└── "benchmark/benchmark.h"  ← NOVA DEPENDÊNCIA
```

**Impacto**: 
- ✅ Adiciona 1 dependência (header simples)
- ✅ NENHUM impacto funcional no solver
- ✅ Apenas adiciona capacidade de medição

---

### 6.3 — Grafo de Dependências (Completo)

```
sudoku_mpi.exe
├── sudoku_mpi.o
│   ├── sudoku.h
│   └── benchmark/benchmark.h  ← NOVO
│
├── benchmark.o  ← NOVO
│   └── mpi.h
│
├── backtracking_omp.o
├── utils_mpi.o
├── loader_mpi.o
├── parser_mpi.o
└── get_next_line_mpi.o
```

**Análise**: 
- ✅ Módulo `benchmark` está isolado
- ✅ NENHUMA dependência circular
- ✅ Pode ser removido facilmente se necessário

---


## 7. IMPACTO NO CÓDIGO EXISTENTE

### 7.1 — Análise de Impacto por Ficheiro

| Ficheiro | Impacto | Tipo | Risco |
|----------|---------|------|:-----:|
| `sudoku_serial.c` | ✅ NENHUM | — | **ZERO** |
| `sudoku_omp.c` | ✅ NENHUM | — | **ZERO** |
| `sudoku_mpi.c` | ⚠️ ALTERADO | 7 linhas adicionadas | **BAIXO** |
| `backtracking.c` | ✅ NENHUM | — | **ZERO** |
| `backtracking_omp.c` | ✅ NENHUM | — | **ZERO** |
| `general_utils/` | ✅ NENHUM | — | **ZERO** |
| `validator/` | ✅ NENHUM | — | **ZERO** |
| `sudoku.h` | ✅ NENHUM | — | **ZERO** |
| `Makefile` | ⚠️ ALTERADO | 5 linhas adicionadas | **BAIXO** |
| `benchmark/` | ✅ NOVO | Módulo criado | — |

**Resumo**:
- ✅ **2 ficheiros alterados** (sudoku_mpi.c, Makefile)
- ✅ **2 ficheiros novos** (benchmark.h, benchmark.c)
- ✅ **0 ficheiros de solver alterados**

---

### 7.2 — Impacto Funcional

**Alterações funcionais**: ✅ **NENHUMA**

**Razão**:
- Benchmark apenas **mede** tempo, não altera lógica
- `MPI_Barrier()` sincroniza mas não altera comportamento
- `benchmark_*()` funções são observadoras (não intrusivas)

**Teste de regressão**:
- ✅ Solução deve ser idêntica (antes e depois)
- ✅ Apenas adiciona output de benchmark
- ✅ Performance pode ter overhead de ~1-5 μs (negligível)

---

### 7.3 — Impacto em Performance

**Overhead do benchmark**:

| Operação | Overhead | Impacto |
|----------|----------|---------|
| `MPI_Wtime()` | ~100-500 ns | Negligível |
| `MPI_Barrier()` | ~1-10 μs | Negligível |
| `benchmark_start()` | ~100-500 ns | Negligível |
| `benchmark_stop()` | ~100-500 ns | Negligível |
| `benchmark_get_elapsed()` | ~10 ns | Negligível |
| `benchmark_print_report()` | ~1 ms | Após resolução |

**Total overhead na medição**: ~2-20 μs

**Tempo típico de Sudoku 9×9**: ~10-100 ms  
**Tempo típico de Sudoku 16×16**: ~1-10 s

**Overhead relativo**: <0.01% (completamente negligível)

**Conclusão**: ✅ Benchmark não afecta performance medida

---

### 7.4 — Impacto em Compilação

**Build time**:
- Adiciona compilação de `benchmark.c` (~0.1-0.5s)
- Adiciona linking de `benchmark.o` (~0.01s)

**Binário**:
- Adiciona ~2-4 KB ao executável `sudoku_mpi.exe`

**Dependências**:
- NENHUMA nova biblioteca externa
- Usa apenas MPI (já presente)

**Conclusão**: ✅ Impacto mínimo no build

---


## 8. EXEMPLO DE USO E OUTPUT

### 8.1 — Código de Integração em `sudoku_mpi.c`

```c
// ... (código existente) ...

if (rank == 0)
{
    int **rank0_board = NULL;
    int **solution_board = NULL;
    int solution_found = 0;
    
    // Carregar Sudoku
    sudoku = load_sudoku(argv[1]);
    if (!sudoku) { /* ... erro ... */ }
    
    size = sudoku->order * sudoku->order;
    
    // ========== INÍCIO BENCHMARK ==========
    MPI_Barrier(MPI_COMM_WORLD);
    benchmark_start();
    // ======================================
    
    // Gerar subproblemas
    int ret = generate_subproblems_mpi(...);
    if (ret != 0 || subproblems_count == 0) { /* ... erro ... */ }
    
    // Broadcast parâmetros
    MPI_Bcast(&sudoku->order, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&subproblems_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Distribuir trabalho
    dispatch_work(subproblems, subproblems_count, sudoku->order, nprocs);
    
    // Resolver localmente
    rank0_board = subproblems[0];
    solve_omp(rank0_board, sudoku->order);
    
    // Verificar solução
    if (is_solved(rank0_board, sudoku->order))
    {
        solution_board = alloc_board(size);
        if (solution_board)
        {
            copy_board(solution_board, rank0_board, size);
            solution_found = 1;
        }
    }
    else
    {
        solution_board = alloc_board(size);
        if (!solution_board) { /* ... erro ... */ }
        
        collect_solutions(sudoku->order, nprocs, solution_board);
        solution_found = 1;
    }
    
    // Enviar terminação
    broadcast_termination(nprocs);
    
    // ========== FIM BENCHMARK ==========
    benchmark_stop();
    // ===================================
    
    // ========== RELATÓRIO ==========
    double elapsed = benchmark_get_elapsed();
    int omp_threads = omp_get_max_threads();
    
    benchmark_print_report(elapsed, nprocs, omp_threads);
    // ===============================
    
    // Imprimir solução
    fprintf(stderr, "\n");
    if (solution_found && solution_board)
    {
        fprintf(stderr, "SOLUTION FOUND\n");
        fprintf(stderr, "========================================\n");
        print(solution_board, sudoku->order);
    }
    else
    {
        fprintf(stderr, "NO SOLUTION FOUND\n");
    }
    fprintf(stderr, "\n");
    
    // Libertar memória
    free_sudoku(sudoku);
    for (int i = 0; i < subproblems_count; i++)
        free_board(subproblems[i], size);
    free(subproblems);
    if (solution_board)
        free_board(solution_board, size);
}
else
{
    // Workers
    int order_recv, count_recv;
    
    MPI_Bcast(&order_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&count_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    worker_loop(order_recv);
}
```

---

### 8.2 — Output Esperado (Exemplo)

**Comando**:
```bash
$ mpirun -np 4 ./sudoku_mpi.exe "Sample instances/9x9.txt"
```

**Output**:
```

====================================
MPI+OMP BENCHMARK
====================================
MPI Processes : 4
OMP Threads   : 2
Total Workers : 8
Execution Time: 0.532841 s
====================================


SOLUTION FOUND
========================================
 5  3  4 | 6  7  8 | 9  1  2
 6  7  2 | 1  9  5 | 3  4  8
 1  9  8 | 3  4  2 | 5  6  7
---------+---------+---------
 8  5  9 | 7  6  1 | 4  2  3
 4  2  6 | 8  5  3 | 7  9  1
 7  1  3 | 9  2  4 | 8  5  6
---------+---------+---------
 9  6  1 | 5  3  7 | 2  8  4
 2  8  7 | 4  1  9 | 6  3  5
 3  4  5 | 2  8  6 | 1  7  9

```

---

### 8.3 — Variações de Configuração

**1 processo, 1 thread**:
```bash
$ mpirun -np 1 ./sudoku_mpi.exe "Sample instances/9x9.txt"

====================================
MPI+OMP BENCHMARK
====================================
MPI Processes : 1
OMP Threads   : 1
Total Workers : 1
Execution Time: 2.345678 s
====================================
```

**4 processos, 4 threads cada**:
```bash
$ OMP_NUM_THREADS=4 mpirun -np 4 ./sudoku_mpi.exe "Sample instances/16x16.txt"

====================================
MPI+OMP BENCHMARK
====================================
MPI Processes : 4
OMP Threads   : 4
Total Workers : 16
Execution Time: 0.123456 s
====================================
```

---


## 9. TESTES E VALIDAÇÃO

### 9.1 — Testes Funcionais

**Teste 1: Compilação**
```bash
$ make clean
$ make sudoku_mpi.exe
```
✅ **Critério**: Compila sem erros ou warnings

---

**Teste 2: Execução Básica (1 processo)**
```bash
$ mpirun -np 1 ./sudoku_mpi.exe "Sample instances/9x9.txt"
```
✅ **Critérios**:
- Imprime benchmark report
- Resolve Sudoku corretamente
- Tempo > 0.0 segundos

---

**Teste 3: Execução Multi-Processo (4 processos)**
```bash
$ mpirun -np 4 ./sudoku_mpi.exe "Sample instances/9x9.txt"
```
✅ **Critérios**:
- Imprime benchmark report com 4 processos
- Resolve Sudoku corretamente
- Solução idêntica ao teste anterior
- Tempo < tempo do teste com 1 processo

---

**Teste 4: Execução Híbrida (4 processos × 2 threads)**
```bash
$ OMP_NUM_THREADS=2 mpirun -np 4 ./sudoku_mpi.exe "Sample instances/9x9.txt"
```
✅ **Critérios**:
- Imprime "Total Workers : 8"
- Resolve Sudoku corretamente
- Solução idêntica aos testes anteriores

---

**Teste 5: Sudoku 16×16**
```bash
$ mpirun -np 4 ./sudoku_mpi.exe "Sample instances/16x16.txt"
```
✅ **Critérios**:
- Resolve sem crashes
- Tempo > tempo do 9×9
- Benchmark report correto

---

### 9.2 — Testes de Regressão

**Objectivo**: Garantir que benchmark NÃO altera comportamento funcional

**Método**: Comparar soluções antes e depois

```bash
# Versão ANTES do benchmark (se disponível backup)
$ mpirun -np 4 ./sudoku_mpi_old.exe "Sample instances/9x9.txt" > old_solution.txt

# Versão DEPOIS do benchmark
$ mpirun -np 4 ./sudoku_mpi.exe "Sample instances/9x9.txt" > new_solution.txt

# Comparar
$ diff old_solution.txt new_solution.txt
```

✅ **Critério**: Diferença APENAS no benchmark report (solução idêntica)

---

### 9.3 — Testes de Precisão de Medição

**Teste de Repetibilidade**:
```bash
$ for i in {1..5}; do
    mpirun -np 4 ./sudoku_mpi.exe "Sample instances/9x9.txt" 2>&1 | grep "Execution Time"
  done
```

**Output esperado**:
```
Execution Time: 0.532841 s
Execution Time: 0.531234 s
Execution Time: 0.533567 s
Execution Time: 0.532012 s
Execution Time: 0.531890 s
```

✅ **Critérios**:
- Tempos similares (variação <10%)
- Nenhum valor absurdo (0.0 ou muito alto)
- Média e desvio padrão razoáveis

---

### 9.4 — Checklist de Validação

Antes de considerar implementação completa:

- [ ] `benchmark/benchmark.h` criado
- [ ] `benchmark/benchmark.c` criado
- [ ] Makefile actualizado (regra benchmark.o)
- [ ] Makefile actualizado (target MPI com benchmark.o)
- [ ] `sudoku_mpi.c` inclui `benchmark/benchmark.h`
- [ ] `sudoku_mpi.c` chama `MPI_Barrier()`
- [ ] `sudoku_mpi.c` chama `benchmark_start()`
- [ ] `sudoku_mpi.c` chama `benchmark_stop()`
- [ ] `sudoku_mpi.c` chama `benchmark_get_elapsed()`
- [ ] `sudoku_mpi.c` chama `benchmark_print_report()`
- [ ] Compila sem erros
- [ ] Compila sem warnings
- [ ] Executa com 1 processo
- [ ] Executa com 4 processos
- [ ] Solução está correta
- [ ] Benchmark report é impresso
- [ ] Formato está correto
- [ ] Tempo medido faz sentido

---


## 10. RESUMO EXECUTIVO

### 10.1 — O Que Será Criado

| Item | Tipo | Linhas | Descrição |
|------|------|-------:|-----------|
| `benchmark/benchmark.h` | Header | ~30 | Declarações públicas |
| `benchmark/benchmark.c` | Implementação | ~80 | Lógica de medição |
| **Total novos ficheiros** | — | **~110** | Módulo completo |

### 10.2 — O Que Será Modificado

| Ficheiro | Linhas adicionadas | Linhas modificadas | Tipo |
|----------|-------------------:|-------------------:|------|
| `sudoku_mpi.c` | 7 | 0 | Integração |
| `Makefile` | 5 | 0 | Build rules |
| **Total** | **12** | **0** | — |

### 10.3 — O Que NÃO Será Tocado

✅ **Ficheiros protegidos** (conforme regras):
- `sudoku_serial.c`
- `sudoku_omp.c`
- `backtracking.c`
- `backtracking_omp.c`
- `validator/*`
- `general_utils/*`

✅ **Funções protegidas em `sudoku_mpi.c`**:
- `solve_omp()`
- `worker_loop()`
- `collect_solutions()`
- `dispatch_work()`
- `generate_subproblems_mpi()`
- Todas as outras funções existentes

---

### 10.4 — Benefícios da Abordagem Modular

✅ **Separação de responsabilidades**:
- Benchmark isolado do solver
- Fácil de remover/desactivar
- Não polui código de negócio

✅ **Reutilizável**:
- Pode ser usado em outros projetos MPI
- API simples e clara
- Sem dependências internas

✅ **Testável**:
- Módulo independente pode ser testado isoladamente
- Não afecta testes do solver

✅ **Manutenível**:
- Mudanças no benchmark não afectam solver
- Mudanças no solver não afectam benchmark
- Responsabilidades bem definidas

---

### 10.5 — Comparação com Alternativas

#### **Alternativa 1: Benchmark inline em `sudoku_mpi.c`**

❌ **Desvantagens**:
- Polui código do solver
- Dificulta manutenção
- Mistura responsabilidades
- Mais difícil de testar

✅ **Vantagens**:
- Nenhuma (só simplicidade inicial)

---

#### **Alternativa 2: Módulo benchmark (escolhida)**

✅ **Vantagens**:
- Código limpo e organizado
- Módulo reutilizável
- Fácil de manter
- Responsabilidades separadas
- Testável independentemente

❌ **Desvantagens**:
- Requer 2 ficheiros adicionais
- Requer modificação no Makefile

**Decisão**: ✅ Módulo benchmark é claramente superior

---

### 10.6 — Riscos e Mitigações

| Risco | Probabilidade | Impacto | Mitigação |
|-------|:-------------:|:-------:|-----------|
| Erro de compilação | Baixa | Médio | Testar compilação incremental |
| Erro de linking | Baixa | Médio | Verificar Makefile cuidadosamente |
| Medição incorreta | Baixa | Alto | Testar com múltiplas execuções |
| Overhead significativo | Muito Baixa | Baixo | Overhead é <0.01% |
| Quebra funcional | Muito Baixa | Alto | Benchmark é read-only, não altera estado |

**Risco global**: ✅ **BAIXO**

---

## 11. APROVAÇÃO PARA IMPLEMENTAÇÃO

### 11.1 — Critérios de Aprovação

✅ **Planeamento completo**: Estrutura, dependências, impacto documentados  
✅ **Abordagem modular**: Benchmark isolado do solver  
✅ **Zero modificações em solvers**: Backtracking intocado  
✅ **Alterações mínimas**: Apenas 7 linhas em sudoku_mpi.c  
✅ **API simples**: 4 funções públicas  
✅ **Risco baixo**: Nenhuma alteração funcional  

---

### 11.2 — Próximos Passos

**FASE 1**: Criar módulo benchmark
1. Criar `benchmark/benchmark.h`
2. Criar `benchmark/benchmark.c`
3. Verificar sintaxe

**FASE 2**: Integrar em build
1. Modificar Makefile
2. Compilar incrementalmente
3. Resolver erros de compilação

**FASE 3**: Integrar em código
1. Adicionar include em `sudoku_mpi.c`
2. Adicionar chamadas de benchmark
3. Compilar e linkar

**FASE 4**: Testar
1. Executar testes funcionais
2. Validar output
3. Confirmar solução correta

---

**STATUS**: ✓ **PLANEAMENTO COMPLETO**  
**PRÓXIMO PASSO**: Aguardar aprovação para gerar código

