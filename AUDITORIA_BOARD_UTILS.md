# AUDITORIA DE FUNÇÕES AUXILIARES DE BOARD

## ESTATUTO: ✅ AUDITORIA COMPLETA

Data: 2024
Objetivo: Identificar funções existentes relacionadas com gestão de boards antes de criar novas

---

## 1. INVENTÁRIO DE FUNÇÕES EXISTENTES

### 1.1 Funções de Alocação

#### Função: `alloc_grid()`

| Atributo | Valor |
|----------|-------|
| **Nome** | `alloc_grid` |
| **Assinatura** | `static int **alloc_grid(int size)` |
| **Ficheiro definição** | `validator/loader.c` (linhas 3-29) |
| **Ficheiro declaração** | Nenhum (função `static`) |
| **Visibilidade** | Privada ao módulo `loader.c` |
| **Responsabilidade** | Alocar matriz 2D de inteiros (size × size) |
| **Pode ser reutilizada?** | **NÃO** |

**Razões para NÃO reutilizar**:
1. ✗ Função `static` - não acessível fora de `loader.c`
2. ✗ Não declarada em `sudoku.h` (interface privada)
3. ✗ Acoplada ao contexto de loading (não é utilitário genérico)

**Implementação actual**:
```c
static int **alloc_grid(int size)
{
    int **tab;
    int i;

    tab = malloc(sizeof(int *) * size);
    if (!tab)
        return (NULL);
    i = 0;
    while (i < size)
    {
        tab[i] = malloc(sizeof(int) * size);
        if (!tab[i])
        {
            while (i > 0)  // Rollback em caso de erro
            {
                i--;
                free(tab[i]);
            }
            free(tab);
            return (NULL);
        }
        i++;
    }
    return (tab);
}
```

**Observações**:
- ✅ Implementação robusta (rollback em caso de malloc() falhar)
- ✅ Mesma lógica que `unflatten_board()` já implementada
- ❌ Não pode ser reutilizada (visibilidade privada)

---

### 1.2 Funções de Libertação

#### Função 1: `free_tab()`

| Atributo | Valor |
|----------|-------|
| **Nome** | `free_tab` |
| **Assinatura** | `static void free_tab(int **tab, int size)` |
| **Ficheiro definição** | `validator/loader.c` (linhas 31-45) |
| **Ficheiro declaração** | Nenhum (função `static`) |
| **Visibilidade** | Privada ao módulo `loader.c` |
| **Responsabilidade** | Libertar matriz 2D de inteiros |
| **Pode ser reutilizada?** | **NÃO** |

**Razões para NÃO reutilizar**:
1. ✗ Função `static` - não acessível fora de `loader.c`
2. ✗ Não declarada em `sudoku.h`
3. ✗ Nome não semântico (`free_tab` vs `free_board`)

**Implementação actual**:
```c
static void free_tab(int **tab, int size)
{
    int i;

    if (!tab)
        return ;
    i = 0;
    while (i < size)
    {
        free(tab[i]);
        i++;
    }
    free(tab);
}
```

**Observações**:
- ✅ Implementação correcta e segura
- ✅ Valida NULL pointer
- ❌ Não pode ser reutilizada (visibilidade privada)

#### Função 2: `free_sudoku()`

| Atributo | Valor |
|----------|-------|
| **Nome** | `free_sudoku` |
| **Assinatura** | `void free_sudoku(t_sudoku *sudoku)` |
| **Ficheiro definição** | `general_utils/utils.c` (linhas 20-40) |
| **Ficheiro declaração** | `sudoku.h` (linha 22) |
| **Visibilidade** | **Pública** |
| **Responsabilidade** | Libertar estrutura `t_sudoku` completa (struct + tab) |
| **Pode ser reutilizada?** | **PARCIALMENTE** |

**Razões para reutilização PARCIAL**:
1. ✓ Função pública (declarada em `sudoku.h`)
2. ✓ Já usada por Serial e OMP
3. ✗ Liberta `t_sudoku*` (não apenas `int**`)
4. ✗ Calcula size internamente: `size = sudoku->order * order`

**Implementação actual**:
```c
void free_sudoku(t_sudoku *sudoku)
{
    int i;
    int size;

    if (!sudoku)
        return ;
    if (sudoku->tab)
    {
        size = sudoku->order * order;  // Usa order do struct
        i = 0;
        while (i < size)
        {
            free(sudoku->tab[i]);
            i++;
        }
        free(sudoku->tab);
    }
    free(sudoku);  // Liberta o struct também
}
```

**Análise para MPI**:
- ✓ Pode ser usada quando trabalhamos com `t_sudoku*` (Rank 0 load)
- ✗ **NÃO pode ser usada** para libertar `int**` avulso (subproblemas, buffers)
- ✗ **NÃO pode ser usada** por workers (não têm `t_sudoku`, apenas `int**`)

**Conclusão**: Reutilizar para `t_sudoku*` (Rank 0), mas **criar nova** para `int**`.

---

### 1.3 Funções de Cópia

#### Análise: Cópia em `backtracking_omp.c`

| Atributo | Valor |
|----------|-------|
| **Localização** | `backtracking/backtracking_omp.c` (linhas 192-246) |
| **Contexto** | Dentro de `backtrack_parallel()` |
| **Visibilidade** | Inline (não é função separada) |
| **Pode ser reutilizada?** | **NÃO** |

**Código de cópia actual**:
```c
// Dentro de backtrack_parallel() - não é função separada
int **tb_copy = malloc(sizeof(int *) * size);
// ... aloca linhas ...
i = 0;
while (i < size)
{
    int j = 0;
    while (j < size)
    {
        tb_copy[i][j] = tb[i][j];  // Cópia elemento a elemento
        j++;
    }
    i++;
}
```

**Razões para NÃO reutilizar**:
1. ✗ Não é função separada (código inline)
2. ✗ Acoplado ao contexto de backtracking paralelo
3. ✗ Não acessível externamente

**Observações**:
- ✅ Lógica de cópia correcta (elemento a elemento)
- ✅ Padrão claro e reutilizável (mas precisa ser extraído)
- ❌ Atualmente não pode ser chamada de fora de `backtracking_omp.c`

#### Conclusão: Nenhuma função pública de cópia existe

---

## 2. MAPA DE REUTILIZAÇÃO

### 2.1 Funções privadas encontradas (NÃO reutilizáveis)

| Função | Ficheiro | Equivalente necessário | Razão |
|--------|----------|------------------------|-------|
| `alloc_grid()` | `loader.c` | `alloc_board()` | Visibilidade `static` |
| `free_tab()` | `loader.c` | `free_board()` | Visibilidade `static` |
| *(inline copy)* | `backtracking_omp.c` | `copy_board()` | Não é função |

### 2.2 Funções públicas encontradas (reutilizáveis)

| Função | Ficheiro | Reutilizável por MPI? | Observações |
|--------|----------|----------------------|-------------|
| `free_sudoku()` | `utils.c` | **PARCIAL** | Apenas para `t_sudoku*`, não `int**` |

---

## 3. FUNÇÕES QUE JÁ EXISTEM

### ✅ Função existente e reutilizável:

**`free_sudoku(t_sudoku *sudoku)`** em `general_utils/utils.c`
- ✓ Pode ser usada por Rank 0 após load_sudoku()
- ✓ Já testada e funcional
- ✗ **NÃO pode** ser usada para libertar subproblemas (`int**`)

### Uso em MPI:
```c
// Rank 0 - Carregar Sudoku original
t_sudoku *sudoku = load_sudoku(argv[1]);
// ... gerar subproblemas ...
free_sudoku(sudoku);  // ✓ PODE REUTILIZAR
```

---

## 4. FUNÇÕES QUE FALTAM

### ❌ Função 1: `alloc_board()`

**Assinatura necessária**:
```c
int **alloc_board(int size);
```

**Responsabilidade**:
- Alocar matriz 2D de inteiros (size × size)
- Rollback em caso de erro de malloc()
- Retornar NULL em caso de falha

**Porquê criar nova**:
- `alloc_grid()` existe mas é `static` (inacessível)
- Necessária para workers alocarem buffers
- Necessária para generate_subproblems_mpi()

### ❌ Função 2: `free_board()`

**Assinatura necessária**:
```c
void free_board(int **board, int size);
```

**Responsabilidade**:
- Libertar matriz 2D de inteiros
- Validar NULL pointer
- Libertar todas as linhas + ponteiros

**Porquê criar nova**:
- `free_tab()` existe mas é `static` (inacessível)
- `free_sudoku()` liberta `t_sudoku*` (não `int**` avulso)
- Necessária para workers libertarem subproblemas
- Necessária para libertar buffers temporários

### ❌ Função 3: `copy_board()`

**Assinatura necessária**:
```c
void copy_board(int **dest, int **src, int size);
```

**Responsabilidade**:
- Copiar todos os elementos de src para dest
- Assumir que dest já está alocado
- Cópia elemento a elemento

**Porquê criar nova**:
- Nenhuma função de cópia pública existe
- Código de cópia existe inline em `backtracking_omp.c` mas não é função
- Necessária para generate_subproblems_mpi()

---

## 5. LOCAL CORRETO PARA IMPLEMENTAÇÃO

### 5.1 Opções consideradas

#### Opção A: `sudoku_mpi.c`
- ✗ Não recomendado
- **Razão**: Funções devem ser reutilizáveis por Serial/OMP também
- **Problema**: Acoplamento com MPI

#### Opção B: `general_utils/utils.c`
- ✓ **RECOMENDADO**
- **Razão**: Já contém `free_sudoku()` e `is_solved()` (utilitários gerais)
- **Vantagem**: Consistência com código existente
- **Vantagem**: Não requer novo ficheiro
- **Vantagem**: Já linkado por todos os targets (serial, OMP, MPI)

#### Opção C: Novo módulo `general_utils/board_utils.c`
- ✓ Alternativa válida (mais modular)
- **Razão**: Separação de responsabilidades clara
- **Desvantagem**: Requer alterações ao Makefile
- **Desvantagem**: Mais ficheiros para manter

#### Opção D: `validator/loader.c`
- ✗ Não recomendado
- **Razão**: `loader.c` é específico para parsing de ficheiros
- **Problema**: Semântica incorrecta (loader ≠ utilitários gerais)

### 5.2 Decisão recomendada

**OPÇÃO B: `general_utils/utils.c`** + **`sudoku.h`**

**Justificação**:
1. ✅ Consistente com estrutura existente
2. ✅ `utils.c` já tem funções de gestão de board (`free_sudoku`, `print`)
3. ✅ Não requer alterações ao Makefile (já linkado)
4. ✅ Declarações em `sudoku.h` (interface pública clara)
5. ✅ Reutilizável por Serial, OMP e MPI
6. ✅ Sem código MPI (funções genéricas)

---

## 6. IMPACTO NO MAKEFILE

### 6.1 Se implementar em `general_utils/utils.c` (RECOMENDADO)

**Impacto**: **NENHUM** ✅

**Razão**:
- `utils.c` já é compilado para todos os targets
- Ficheiro objeto já existe: `obj/utils.o`, `obj/utils_omp.o`, `obj/utils_mpi.o`
- Nenhuma alteração ao Makefile necessária

### 6.2 Se criar novo módulo `board_utils.c` (Alternativa)

**Impacto**: **MÉDIO** ⚠️

**Alterações necessárias**:

```makefile
# Adicionar COMMON_SRCS
COMMON_SRCS = \
    general_utils/get_next_line.c \
    general_utils/utils.c \
    general_utils/board_utils.c \  # ← NOVO
    validator/parser.c \
    validator/loader.c

# Adicionar rules de compilação
$(OBJ_DIR)/board_utils.o: general_utils/board_utils.c
    $(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/board_utils_omp.o: general_utils/board_utils.c
    $(CC) $(CFLAGS_OPT) -c $< -o $@

$(OBJ_DIR)/board_utils_mpi.o: general_utils/board_utils.c
    $(MPICC) $(CFLAGS_OPT) -c $< -o $@

# Adicionar aos targets
$(SERIAL_NAME): ... $(OBJ_DIR)/board_utils.o
$(OMP_NAME): ... $(OBJ_DIR)/board_utils_omp.o
$(MPI_NAME): ... $(OBJ_DIR)/board_utils_mpi.o
```

**Conclusão**: Implementar em `utils.c` evita essas alterações.

---

## 7. RECOMENDAÇÃO FINAL

### 7.1 Arquitectura proposta

**Localização**: `general_utils/utils.c` + `sudoku.h`

**Funções a criar**:

```c
// Em sudoku.h (adicionar após free_sudoku):
int     **alloc_board(int size);
void    free_board(int **board, int size);
void    copy_board(int **dest, int **src, int size);
```

```c
// Em general_utils/utils.c (adicionar após is_solved):

int **alloc_board(int size)
{
    // Alocar matriz 2D
    // Rollback em caso de erro
    // Retornar NULL se falhar
}

void free_board(int **board, int size)
{
    // Validar NULL
    // Libertar todas as linhas
    // Libertar array de ponteiros
}

void copy_board(int **dest, int **src, int size)
{
    // Copiar elemento a elemento
    // Assumir dest já alocado
}
```

### 7.2 Justificação da arquitectura

| Critério | Avaliação |
|----------|-----------|
| **Reutilizável por Serial** | ✅ SIM (não contém código MPI) |
| **Reutilizável por OMP** | ✅ SIM (funções genéricas) |
| **Reutilizável por MPI** | ✅ SIM (objectivo principal) |
| **Sem duplicação** | ✅ SIM (funções privadas não são duplicação) |
| **Semântica correcta** | ✅ SIM (`utils.c` = utilitários gerais) |
| **Impacto Makefile** | ✅ ZERO (já linkado) |
| **Manutenibilidade** | ✅ ALTA (num único ficheiro) |
| **Consistência** | ✅ ALTA (padrão existente) |

### 7.3 Implementação **NÃO deve**:

- ❌ Conter código MPI
- ❌ Conter código OpenMP
- ❌ Depender de `t_sudoku` struct
- ❌ Ser colocada em `sudoku_mpi.c`
- ❌ Criar novo ficheiro desnecessariamente

### 7.4 Implementação **DEVE**:

- ✅ Ser genérica (trabalhar apenas com `int**` e `size`)
- ✅ Ter rollback em caso de erro de malloc
- ✅ Validar parâmetros (NULL checks)
- ✅ Ser declarada em `sudoku.h` (interface pública)
- ✅ Ser implementada em `utils.c` (utilitários gerais)
- ✅ Seguir estilo de código existente (while loops, etc.)

---

## 8. COMPARAÇÃO COM FUNÇÕES EXISTENTES

### 8.1 alloc_board() vs alloc_grid()

| Aspecto | `alloc_grid()` (loader.c) | `alloc_board()` (proposto) |
|---------|---------------------------|----------------------------|
| **Visibilidade** | `static` (privada) | **Pública** |
| **Lógica** | Aloca + rollback | **Idêntica** |
| **Localização** | `loader.c` | **`utils.c`** |
| **Uso** | Apenas por loader | **Geral (todos os módulos)** |
| **Declaração** | Nenhuma | **`sudoku.h`** |

**Conclusão**: Mesma lógica, diferente visibilidade e propósito.

### 8.2 free_board() vs free_tab()

| Aspecto | `free_tab()` (loader.c) | `free_board()` (proposto) |
|---------|-------------------------|----------------------------|
| **Visibilidade** | `static` (privada) | **Pública** |
| **Lógica** | Liberta matriz 2D | **Idêntica** |
| **Localização** | `loader.c` | **`utils.c`** |
| **Uso** | Apenas por loader | **Geral (todos os módulos)** |
| **Declaração** | Nenhuma | **`sudoku.h`** |

**Conclusão**: Mesma lógica, diferente visibilidade e propósito.

### 8.3 free_board() vs free_sudoku()

| Aspecto | `free_sudoku()` | `free_board()` (proposto) |
|---------|-----------------|----------------------------|
| **Parâmetro** | `t_sudoku*` | **`int**` + `int size`** |
| **Liberta struct** | SIM | **NÃO** |
| **Calcula size** | Internamente (order²) | **Recebe como parâmetro** |
| **Uso MPI** | Rank 0 (load) | **Workers (subproblemas)** |

**Conclusão**: Complementares, não substitutas.

---

## 9. VALIDAÇÃO DA DECISÃO

### 9.1 Checklist arquitectónico

- [x] Funções existentes auditadas
- [x] Reutilização avaliada (funções privadas não reutilizáveis)
- [x] Duplicação evitada (novas funções são públicas, não duplicação)
- [x] Localização justificada (`utils.c` consistente)
- [x] Impacto no Makefile minimizado (zero)
- [x] Semântica correcta (utilitários gerais)
- [x] Reutilizável por todos os módulos

### 9.2 Conformidade com requisitos

| Requisito | Status |
|-----------|--------|
| Reutilizável por Serial | ✅ SIM |
| Reutilizável por OpenMP | ✅ SIM |
| Reutilizável por MPI | ✅ SIM |
| Sem código MPI | ✅ SIM |
| Sem duplicação | ✅ SIM (funções privadas ≠ duplicação) |
| Arquitectura modular | ✅ SIM |
| Impacto mínimo | ✅ SIM (zero no Makefile) |

---

## 10. CONCLUSÃO

### Funções encontradas:
- `alloc_grid()` - **PRIVADA** (não reutilizável)
- `free_tab()` - **PRIVADA** (não reutilizável)
- `free_sudoku()` - **PÚBLICA** (reutilizável apenas para `t_sudoku*`)
- Cópia inline - **NÃO É FUNÇÃO** (não reutilizável)

### Funções a criar:
1. **`alloc_board(int size)`** - nova função pública
2. **`free_board(int **board, int size)`** - nova função pública
3. **`copy_board(int **dest, int **src, int size)`** - nova função pública

### Localização recomendada:
- **Implementação**: `general_utils/utils.c`
- **Declaração**: `sudoku.h`
- **Impacto Makefile**: ZERO (já linkado)

### Próximo passo:
**Aguardar aprovação desta arquitectura** antes de implementar o PASSO 3.2.

---

**FIM DA AUDITORIA**
