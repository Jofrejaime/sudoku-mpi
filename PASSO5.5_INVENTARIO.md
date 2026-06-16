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


#### Em `worker_loop()`:
```c
fprintf(stderr, "[RANK %d] Entering worker loop\n", rank);
fprintf(stderr, "[RANK %d] Waiting for subproblem...\n", rank);
fprintf(stderr, "[RANK %d] Subproblem received, solving...\n", rank);
fprintf(stderr, "[RANK %d] Calling solve_omp() ← REUSE\n", rank);
```
**Avaliação**: ❌ **REMOVER MAIORIA** — manter apenas mensagens críticas

#### Em `main()`:
Centenas de linhas de logging para validação de infraestrutura (FASE 2, FASE 3.1, etc.)
**Avaliação**: ❌ **REMOVER COMPLETAMENTE** — toda a lógica de teste

---

## 4. COMENTÁRIOS TEMPORÁRIOS

### 4.1 Comentários de Fase

```c
/* FASE 2: Infraestrutura MPI+OpenMP apenas */
/* FASE 3.1: Executar testes de serialização */
// Testing (FASE 3.1 - temporary)
```
**Avaliação**: ❌ **REMOVER** — referências a fases de desenvolvimento

### 4.2 Comentários Descritivos Úteis

```c
/**
 * flatten_board - Serializa board 2D para buffer 1D
 * 
 * @board: Board 2D (int**) a serializar
 * ...
 */
```
**Avaliação**: ✓ **MANTER** — documentação técnica valiosa

---

## 5. MACROS E CONSTANTES

### 5.1 Em Uso
- ✓ `DEBUG_LOG()` — útil para debugging futuro
- ✓ `MPI_TAG_SUBPROBLEM` (101)
- ✓ `MPI_TAG_SOLUTION` (102)
- ✓ `MPI_TAG_TERMINATION` (103)
- ✓ `SUBPROBLEM_MAX` (1024)
- ✓ `POLL_INTERVAL` (5000)

### 5.2 Não Utilizadas
- ❌ `SUBPROBLEM_DEPTH_LIMIT` (2) — nunca usada

---

## 6. DUPLICAÇÃO DE CÓDIGO

### 6.1 Análise
**Nenhuma duplicação significativa detectada**.

Todas as funções de serialização, comunicação e gestão de boards seguem o princípio DRY corretamente.

---

## 7. CÓDIGO NO MAIN() A REMOVER

### 7.1 Validação de Infraestrutura (linhas ~1650-1750)
```c
if (argc < 2)
{
    // Toda a lógica de validação FASE 2, FASE 3.1, FASE 3.2, FASE 3.3
    // test_serialization()
    // test_board_utils()
    // test_mpi_communication()
    // ~100 linhas
}
```
**Ação**: ❌ **REMOVER COMPLETAMENTE**

---

## RESUMO DO INVENTÁRIO

### Linhas Totais Atuais
**1835 linhas** (sudoku_mpi.c)

### Código a Remover
| Categoria | Linhas Estimadas |
|-----------|------------------|
| `test_serialization()` | ~140 |
| `test_board_utils()` | ~130 |
| `test_mpi_communication()` | ~150 |
| `dispatch_work_to_self()` | ~20 |
| `check_termination()` | ~20 |
| Lógica de validação no main() | ~100 |
| Logs de debugging (redução) | ~50 |
| Comentários temporários | ~30 |
| **TOTAL ESTIMADO** | **~640 linhas** |

### Linhas Esperadas Após Cleanup
**~1200 linhas** (redução de ~35%)

---

## PRÓXIMOS PASSOS

✓ FASE 1 — INVENTÁRIO **COMPLETO**  
→ FASE 2 — Remover funções de teste  
→ FASE 3 — Remover código morto  
→ FASE 4 — Limpar logs  
→ FASE 5 — Limpar comentários  
→ FASE 6 — Verificar duplicação (já confirmada: OK)  
→ FASE 7 — Verificar organização  
→ FASE 8 — Compilação limpa  
→ FASE 9 — Relatório final
