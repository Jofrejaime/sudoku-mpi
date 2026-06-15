# PASSO 3.2 — IMPLEMENTAÇÃO DE FUNÇÕES AUXILIARES DE BOARD

**Data**: 2024  
**Estatuto**: ✅ **COMPLETO E VALIDADO**  
**Fase**: FASE 3.2 — Board Utilities Implementation

---

## OBJECTIVO

Implementar funções auxiliares genéricas para manipulação de boards (`int**`) necessárias para:
- Alocação dinâmica de subproblemas (workers)
- Libertação de memória (todos os processos)
- Cópia de boards para geração de subproblemas (Rank 0)

**Requisitos**:
- ✅ Reutilizáveis por Serial, OpenMP e MPI
- ✅ Sem código MPI/OpenMP (funções genéricas)
- ✅ Robustas (validação NULL, rollback em erro)
- ✅ Zero impacto no Makefile

---

## AUDITORIA PRÉVIA

**Documento**: `AUDITORIA_BOARD_UTILS.md`

### Funções encontradas (NÃO reutilizáveis):

| Função | Ficheiro | Visibilidade | Reutilizável? | Razão |
|--------|----------|--------------|---------------|-------|
| `alloc_grid()` | `loader.c` | `static` | ❌ NÃO | Função privada |
| `free_tab()` | `loader.c` | `static` | ❌ NÃO | Função privada |
| `free_sudoku()` | `utils.c` | Pública | 🟡 PARCIAL | Apenas `t_sudoku*` |
| *(inline copy)* | `backtracking_omp.c` | N/A | ❌ NÃO | Não é função |

**Conclusão**: Necessário criar 3 novas funções públicas.

---

## ARQUITECTURA APROVADA

### Localização:
- **Implementação**: `general_utils/utils.c`
- **Declaração**: `sudoku.h`

### Justificação:
1. ✅ `utils.c` já contém utilitários de board (`free_sudoku`, `is_solved`)
2. ✅ Consistente com estrutura existente
3. ✅ **Zero impacto no Makefile** (já linkado por todos os targets)
4. ✅ Semântica correcta (utilitários gerais)

---

## FUNÇÕES IMPLEMENTADAS

### 1. `alloc_board()`

**Assinatura**:
```c
int **alloc_board(int size);
```

**Responsabilidade**:
- Alocar matriz 2D de inteiros (size × size)
- Rollback automático em caso de erro de malloc()
- Retornar NULL se falhar

**Implementação**:
```c
int **alloc_board(int size)
{
    int **board;
    int i;

    board = malloc(sizeof(int *) * size);
    if (!board)
        return (NULL);
    i = 0;
    while (i < size)
    {
        board[i] = malloc(sizeof(int) * size);
        if (!board[i])
        {
            // Rollback: liberar todas as linhas já alocadas
            while (i > 0)
            {
                i--;
                free(board[i]);
            }
            free(board);
            return (NULL);
        }
        i++;
    }
    return (board);
}
```

**Características**:
- ✅ Rollback automático (sem memory leaks)
- ✅ Complexidade: O(size²) temporal, O(size²) espacial
- ✅ Padrão consistente com `alloc_grid()` (loader.c)

---

### 2. `free_board()`

**Assinatura**:
```c
void free_board(int **board, int size);
```

**Responsabilidade**:
- Libertar matriz 2D de inteiros
- Validar NULL pointer (safe to call com board=NULL)
- Libertar todas as linhas + array de ponteiros

**Implementação**:
```c
void free_board(int **board, int size)
{
    int i;

    if (!board)
        return ;
    i = 0;
    while (i < size)
    {
        free(board[i]);
        i++;
    }
    free(board);
}
```

**Características**:
- ✅ Safe NULL check (pode ser chamada com board=NULL)
- ✅ Complexidade: O(size) temporal, O(1) espacial
- ✅ Padrão consistente com `free_tab()` (loader.c)

**Diferença de `free_sudoku()`**:
- `free_sudoku()` liberta `t_sudoku*` (struct + tab)
- `free_board()` liberta apenas `int**` (sem struct)
- **Complementares, não substitutas**

---

### 3. `copy_board()`

**Assinatura**:
```c
void copy_board(int **dest, int **src, int size);
```

**Responsabilidade**:
- Copiar todos os elementos de src → dest
- Assumir que dest já está alocado (usa `alloc_board()` primeiro)
- Deep copy (elemento a elemento)

**Implementação**:
```c
void copy_board(int **dest, int **src, int size)
{
    int i;
    int j;

    i = 0;
    while (i < size)
    {
        j = 0;
        while (j < size)
        {
            dest[i][j] = src[i][j];
            j++;
        }
        i++;
    }
}
```

**Características**:
- ✅ Deep copy (boards independentes)
- ✅ Complexidade: O(size²) temporal, O(1) espacial
- ✅ Padrão consistente com código inline em `backtracking_omp.c`

---

## DECLARAÇÕES PÚBLICAS

**Ficheiro**: `sudoku.h` (linhas após `free_sudoku()`)

```c
// Board utilities (generic functions for int** manipulation)
int     **alloc_board(int size);
void    free_board(int **board, int size);
void    copy_board(int **dest, int **src, int size);
```

---

## TESTES IMPLEMENTADOS

### Função de teste: `test_board_utils()`

**Localização**: `sudoku_mpi.c` (após `test_serialization()`)

**Cenários testados**:

| # | Teste | Tamanho | Validação |
|---|-------|---------|-----------|
| 1 | `alloc_board()` → board1 | 4×4 | Retorno não-NULL |
| 2 | Preencher board1 | 4×4 | Padrão: `value = row*size + col + 1` |
| 3 | `alloc_board()` → board2 | 4×4 | Retorno não-NULL |
| 4 | Inicializar board2 com zeros | 4×4 | Preparação para copy |
| 5 | `copy_board(board1 → board2)` | 4×4 | Deep copy |
| 6 | Verificar board2 == board1 | 4×4 | Elemento a elemento |
| 7 | Modificar board2, verificar board1 | 4×4 | Independência (deep copy) |
| 8 | `free_board(board1, board2)` | 4×4 | Libertação sem leaks |

**Repetido para**: 4×4, 9×9, 16×16

---

## RESULTADOS DOS TESTES

### Execução: `mpirun -np 4 ./sudoku_mpi.exe`

```
========================================
 TESTE: BOARD UTILITIES
========================================
Testes: alloc_board, free_board, copy_board
Tamanhos: 4×4, 9×9, 16×16
========================================

Teste 1/3: Board 4×4
  alloc_board(board1): OK
  Fill pattern: OK (values 1..16)
  alloc_board(board2): OK
  copy_board(board1 → board2): OK
  Verification: OK (all 16 elements copied correctly)
  Deep copy: OK (boards are independent)
  free_board(board1, board2): OK
  [PASS] Test 4×4

Teste 2/3: Board 9×9
  alloc_board(board1): OK
  Fill pattern: OK (values 1..81)
  alloc_board(board2): OK
  copy_board(board1 → board2): OK
  Verification: OK (all 81 elements copied correctly)
  Deep copy: OK (boards are independent)
  free_board(board1, board2): OK
  [PASS] Test 9×9

Teste 3/3: Board 16×16
  alloc_board(board1): OK
  Fill pattern: OK (values 1..256)
  alloc_board(board2): OK
  copy_board(board1 → board2): OK
  Verification: OK (all 256 elements copied correctly)
  Deep copy: OK (boards are independent)
  free_board(board1, board2): OK
  [PASS] Test 16×16

========================================
 ALL TESTS PASSED ✓
========================================
```

**Estatuto**: ✅ **TODOS OS TESTES PASSARAM**

---

## VALIDAÇÃO DE COMPILAÇÃO

### Comando: `make`

```bash
# Serial
gcc -Wall -Wextra -Werror -I. -O3 -fopenmp -c general_utils/utils.c -o obj/utils.o
gcc -Wall -Wextra -Werror -I. -O3 -fopenmp obj/sudoku_serial.o ... obj/utils.o ... -o sudoku_serial.exe
✓ Serial version: sudoku_serial.exe

# OpenMP
gcc -Wall -Wextra -Werror -I. -O3 -fopenmp -c general_utils/utils.c -o obj/utils_omp.o
gcc -Wall -Wextra -Werror -I. -O3 -fopenmp obj/sudoku_omp.o ... obj/utils_omp.o ... -o sudoku_omp.exe
✓ OMP version: sudoku_omp.exe

# MPI
mpicc -Wall -Wextra -Werror -I. -O3 -fopenmp -c general_utils/utils.c -o obj/utils_mpi.o
mpicc -Wall -Wextra -Werror -I. -O3 -fopenmp obj/sudoku_mpi.o ... obj/utils_mpi.o ... -o sudoku_mpi.exe
✓ MPI version: sudoku_mpi.exe
```

**Resultado**: ✅ Compilação sem warnings/erros nos 3 targets

---

## IMPACTO NO PROJETO

### Ficheiros modificados:

| Ficheiro | Alterações | Linhas adicionadas |
|----------|------------|-------------------|
| `sudoku.h` | Declarações públicas | +3 linhas |
| `general_utils/utils.c` | Implementações | +67 linhas |
| `sudoku_mpi.c` | Função de teste | +159 linhas |
| **Makefile** | **NENHUMA** | **0 linhas** ✅ |

### Impacto no Makefile:
**ZERO** — `utils.c` já era compilado para todos os targets:
- `obj/utils.o` (Serial)
- `obj/utils_omp.o` (OpenMP)
- `obj/utils_mpi.o` (MPI)

---

## REUTILIZAÇÃO

### Por Serial (`sudoku_serial.c`):
```c
int **board = alloc_board(size);
// ... usar board ...
free_board(board, size);
```

### Por OpenMP (`sudoku_omp.c`):
```c
int **board = alloc_board(size);
// ... usar board ...
free_board(board, size);
```

### Por MPI (`sudoku_mpi.c`):
```c
// Rank 0: gerar subproblemas
int **subproblem = alloc_board(size);
copy_board(subproblem, original, size);
subproblem[row][col] = value;  // Modificar

// Workers: receber e alocar
int **received_board = alloc_board(size);
// ... unflatten_board() ou recv ...

// Todos: libertar
free_board(subproblem, size);
```

---

## CONFORMIDADE ARQUITECTÓNICA

### Checklist:

- [x] Funções genéricas (sem MPI/OpenMP)
- [x] Reutilizáveis por Serial ✓
- [x] Reutilizáveis por OpenMP ✓
- [x] Reutilizáveis por MPI ✓
- [x] Sem duplicação de código (funções privadas ≠ duplicação)
- [x] Localização correcta (`utils.c` = utilitários gerais)
- [x] Declarações públicas em `sudoku.h`
- [x] Impacto zero no Makefile
- [x] Rollback em caso de erro (alloc_board)
- [x] NULL-safe (free_board)
- [x] Deep copy (copy_board)
- [x] Testadas exaustivamente (4×4, 9×9, 16×16)
- [x] Compilação sem warnings (-Werror)
- [x] Todos os testes passaram ✓

---

## JUSTIFICAÇÃO: NÃO É DUPLICAÇÃO

### Funções privadas vs públicas:

| Aspecto | `alloc_grid()` (loader.c) | `alloc_board()` (utils.c) |
|---------|---------------------------|----------------------------|
| **Visibilidade** | `static` (PRIVADA) | **Pública** |
| **Propósito** | Parsing de ficheiros | **Manipulação genérica** |
| **Acessível por** | Apenas `loader.c` | **Todos os módulos** |
| **Declaração** | Nenhuma | **`sudoku.h`** |

**Conclusão**: Mesma lógica, **diferente interface e propósito**.  
**NÃO é duplicação** — é criação de API pública necessária.

---

## PRÓXIMOS PASSOS (FASE 3.3)

Com as funções de board disponíveis, podemos agora implementar:

### 1. Geração de subproblemas (Rank 0)
```c
int **subproblem = alloc_board(size);
copy_board(subproblem, original_board, size);
subproblem[row][col] = value;  // Fixar célula
```

### 2. Comunicação MPI
```c
// Rank 0: enviar subproblema
int *buffer = flatten_board(subproblem, size);
MPI_Send(buffer, size*size, MPI_INT, worker_rank, ...);
free(buffer);

// Workers: receber subproblema
int *buffer = malloc(size * size * sizeof(int));
MPI_Recv(buffer, size*size, MPI_INT, 0, ...);
int **board = unflatten_board(buffer, size);
free(buffer);
```

### 3. Libertação de memória
```c
// Qualquer processo
free_board(board, size);
```

---

## CONCLUSÃO

### PASSO 3.2: ✅ **COMPLETO E VALIDADO**

**Entregáveis**:
1. ✅ 3 funções implementadas (`alloc_board`, `free_board`, `copy_board`)
2. ✅ Declaradas publicamente em `sudoku.h`
3. ✅ Testes automatizados (4×4, 9×9, 16×16)
4. ✅ Todos os testes passaram
5. ✅ Zero impacto no Makefile
6. ✅ Compilação sem warnings nos 3 targets

**Qualidade**:
- ✅ Funções robustas (rollback, NULL-safe, deep copy)
- ✅ Reutilizáveis por todos os módulos (Serial, OMP, MPI)
- ✅ Consistentes com código existente
- ✅ Sem duplicação (API pública vs funções privadas)

**Status**: Pronto para **FASE 3.3 — MPI Communication**

---

**PASSO 3.2 CONCLUÍDO COM SUCESSO ✓**
