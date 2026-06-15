# PASSO 5.5 — INVENTÁRIO DE CLEANUP
# SUDOKU MPI + OPENMP HÍBRIDO

**Data**: 2026-06-15  
**Fase**: PRÉ-CLEANUP  
**Objectivo**: Identificar código temporário, testes e elementos removíveis

---

## ESTATÍSTICAS BASELINE

**Ficheiro principal**: `sudoku_mpi.c`  
**Linhas totais**: **1835 linhas**  
**Estado**: Funcional mas com código de desenvolvimento

---

## 1. FUNÇÕES IDENTIFICADAS

### 1.1 Funções de Produção (MANTER)

#### Configuração
- ✓ `configure_openmp()` — Configuração híbrida MPI+OpenMP

#### Serialização
- ✓ `flatten_board()` — Serialização board 2D → 1D
- ✓ `unflatten_board()` — Deserialização 1D → 2D

#### Comunicação MPI
- ✓ `send_subproblem()` — Envio de subproblema via MPI
- ✓ `recv_subproblem()` — Recepção com polling anti-deadlock
- ✓ `send_solution()` — Envio de solução
- ✓ `recv_solution()` — Recepção de solução

#### Distribuição de Trabalho
- ✓ `find_first_empty()` — Helper para geração de subproblemas
- ✓ `generate_subproblems_mpi()` — Geração de subproblemas
- ✓ `dispatch_work()` — Distribuição Round-Robin
- ❌ `dispatch_work_to_self()` — **FUNÇÃO MORTA** (apenas logging, não utilizada)

#### Worker & Controlo
- ✓ `worker_loop()` — Loop principal dos workers
- ✓ `collect_solutions()` — Aguarda solução de workers
- ✓ `broadcast_termination()` — Notifica terminação
- ❓ `check_termination()` — **FUNÇÃO NÃO UTILIZADA DIRETAMENTE** (lógica dentro de recv_subproblem)

#### Utilitários
- ✓ `print_usage()` — Instruções de uso
- ✓ `main()` — Entry point

---

### 1.2 Funções de Teste (REMOVER)

❌ `test_serialization()` — **~140 linhas** de teste flatten/unflatten  
❌ `test_board_utils()` — **~130 linhas** de teste alloc/free/copy  
❌ `test_mpi_communication()` — **~150 linhas** de teste Rank 0 ↔ Rank 1

**Total estimado**: ~420 linhas de código de teste

---

## 2. CÓDIGO MORTO E NÃO UTILIZADO

### 2.1 Funções Mortas

❌ **`dispatch_work_to_self()`** (linhas ~1145-1165)
```c
void dispatch_work_to_self(int **board, int order, int rank)
{
    // Apenas logging, nenhuma lógica funcional
    // NUNCA chamada no main()
}
```
**Razão**: Subproblema local de Rank 0 já processado diretamente no main()

❌ **`check_termination()`** (linhas ~1500-1520)
```c
int check_termination(void)
{
    // Lógica de polling já integrada em recv_subproblem()
    // NUNCA chamada
}
```

**Razão**: Polling implementado dentro de `recv_subproblem()`, não requer função separada

---

### 2.2 Constantes Não Utilizadas

❌ **`SUBPROBLEM_DEPTH_LIMIT`** (linha 43)
```c
#define SUBPROBLEM_DEPTH_LIMIT 2
```
**Razão**: Geração de subproblemas não usa profundidade, apenas primeira célula vazia

---

### 2.3 Variáveis Não Utilizadas

Em `collect_solutions()`:
```c
(void)nprocs;  // Unused
```

---

## 3. LOGGING TEMPORÁRIO

### 3.1 Logs de Desenvolvimento (AVALIAR REMOÇÃO)

#### Em `configure_openmp()`:
```c
fprintf(stderr, "[OpenMP] Manual configuration: %d threads per process\n", ...);
fprintf(stderr, "[OpenMP] Auto-configured: %d threads per process ...\n", ...);
```
**Avaliação**: ✓ **MANTER** — informação de configuração útil

#### Em `generate_subproblems_mpi()`:
```c
fprintf(stderr, "[GENERATION] First empty cell: [%d][%d]\n", row, col);
fprintf(stderr, "[GENERATION] Generated %d subproblems\n", *count);
```
**Avaliação**: ❌ **REMOVER** — debugging temporário

#### Em `dispatch_work()`:
```c
fprintf(stderr, "\n========================================\n");
fprintf(stderr, " WORK DISTRIBUTION (ROUND-ROBIN)\n");
fprintf(stderr, "========================================\n");
// + estatísticas detalhadas
```
**Avaliação**: ❌ **REDUZIR** — manter apenas resumo conciso
