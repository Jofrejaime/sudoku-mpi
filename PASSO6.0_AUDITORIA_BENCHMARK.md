# PASSO 6.0 — AUDITORIA DE BENCHMARK

**Data**: 2026-06-16  
**Objectivo**: Auditar métodos de medição de tempo antes de implementar benchmark  
**Status**: ⚠️ **APENAS AUDITORIA** — NENHUMA ALTERAÇÃO SERÁ FEITA

---

## 1. SITUAÇÃO ATUAL

### 1.1 — Resumo Executivo

| Versão | Medição de Tempo? | Método Usado | Precisão | Status |
|--------|:-----------------:|--------------|----------|--------|
| **Serial** | ✅ SIM | `omp_get_wtime()` | Alta (wall-clock) | ⚠️ Inconsistente |
| **OMP** | ✅ SIM | `omp_get_wtime()` | Alta (wall-clock) | ✅ Correto |
| **MPI** | ❌ **NÃO** | — | — | ❌ **AUSENTE** |

**PROBLEMAS CRÍTICOS IDENTIFICADOS**:
1. ❌ **`sudoku_mpi.c` NÃO mede tempo de execução**
2. ⚠️ **`sudoku_serial.c` usa biblioteca OpenMP** (inconsistência conceitual)
3. ⚠️ **Formatos de output diferentes** (`.1f` vs `.10f`)
4. ❌ **Impossível comparar performance** sem medição MPI

---

## 2. ANÁLISE DETALHADA POR VERSÃO

### 2.1 — `sudoku_serial.c` (Versão Serial)

**Localização**: Linhas 31-37

**Código actual**:
```c
exec_time = -omp_get_wtime();
solve(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
print(sudoku->tab, sudoku->order);
fprintf(stderr, "%.1f\n", exec_time);
```

**Análise**:

| Aspecto | Avaliação | Comentário |
|---------|:---------:|------------|
| **Método** | `omp_get_wtime()` | Função OpenMP (wall-clock time) |
| **Precisão** | Alta | Segundos com dupla precisão |
| **Formato output** | `%.1f` | 1 casa decimal (ex: `3.2`) |
| **Consistência** | ⚠️ Problemático | Versão "serial" depende de OpenMP |


**Dependências**:
- Header: `#include <omp.h>` (via `sudoku.h`)
- Linking: Requer `-fopenmp` na compilação

**Problemas identificados**:
1. ⚠️ **Inconsistência conceitual**: Versão "serial" usa biblioteca OpenMP
2. ⚠️ **Baixa precisão no output**: Apenas 1 casa decimal
3. ⚠️ **Dependência desnecessária**: Serial não deveria precisar de OpenMP

**O que está a ser medido**:
- ✅ Tempo de parede (wall-clock) — CORRETO
- ✅ Apenas tempo de `solve()` — CORRETO (exclui I/O)
- ✅ Medição em segundos — CORRETO

**Vantagens**:
- Simples e direto
- Alta precisão temporal
- Overhead mínimo da função de medição

**Desvantagens**:
- Dependência de OpenMP numa versão "serial"
- Formato de output inconsistente com OMP

---

### 2.2 — `sudoku_omp.c` (Versão OpenMP)

**Localização**: Linhas 33-38

**Código actual**:
```c
exec_time = -omp_get_wtime();
solve_omp(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
print(sudoku->tab, sudoku->order);
fprintf(stderr, "%.10f\n", exec_time);
```

**Análise**:

| Aspecto | Avaliação | Comentário |
|---------|:---------:|------------|
| **Método** | `omp_get_wtime()` | Função OpenMP (wall-clock time) |
| **Precisão** | Alta | Segundos com dupla precisão |
| **Formato output** | `%.10f` | 10 casas decimais (ex: `0.0123456789`) |
| **Consistência** | ✅ Correto | OpenMP mede código OpenMP |

**Dependências**:
- Header: `#include <omp.h>` (via `sudoku.h`)
- Linking: Requer `-fopenmp` na compilação

**Problemas identificados**:
1. ⚠️ **Formato diferente de Serial**: `.10f` vs `.1f`
2. ✅ Nenhum problema técnico

**O que está a ser medido**:
- ✅ Tempo de parede (wall-clock) — CORRETO
- ✅ Apenas tempo de `solve_omp()` — CORRETO (exclui I/O)
- ✅ Medição em segundos — CORRETO

**Vantagens**:
- Apropriado para código OpenMP
- Alta precisão temporal
- Overhead mínimo
- Consistente com a biblioteca usada

**Desvantagens**:
- Formato de output excessivamente preciso (10 casas)

---


### 2.3 — `sudoku_mpi.c` (Versão MPI+OpenMP Híbrido)

**Localização**: Função `main()` completa analisada

**Código actual**:
```c
// NÃO EXISTE MEDIÇÃO DE TEMPO
```

**Análise**:

| Aspecto | Avaliação | Comentário |
|---------|:---------:|------------|
| **Método** | ❌ **AUSENTE** | Nenhuma medição implementada |
| **Precisão** | — | N/A |
| **Formato output** | — | Apenas imprime solução |
| **Consistência** | ❌ Crítico | Impossível comparar performance |

**Dependências disponíveis**:
- MPI: `MPI_Wtime()` — disponível, não usado
- OpenMP: `omp_get_wtime()` — disponível (via `#include <omp.h>`)
- stdlib: `clock()` — disponível, mas inadequado

**Problemas identificados**:
1. ❌ **CRÍTICO**: Nenhuma medição de tempo implementada
2. ❌ **CRÍTICO**: Impossível fazer benchmark sem métricas
3. ❌ **CRÍTICO**: Impossível calcular speedup ou eficiência

**O que deveria ser medido**:
1. ✅ Tempo total de execução (wall-clock)
2. ✅ Apenas tempo de processamento (excluir I/O e inicialização MPI)
3. ✅ Tempo desde geração de subproblemas até solução encontrada

**Componentes do tempo MPI** (não medidos actualmente):
- Geração de subproblemas
- Distribuição via MPI
- Resolução local (Rank 0) com `solve_omp()`
- Resolução remota (Workers) com `solve_omp()`
- Recolha de soluções via MPI
- Broadcast de terminação

**Overhead MPI não visível** (sem medição):
- Comunicação (`MPI_Send`, `MPI_Recv`, `MPI_Iprobe`)
- Polling (workers aguardando trabalho)
- Serialização/deserialização de boards

---

## 3. COMPARAÇÃO DE MÉTODOS DISPONÍVEIS

### 3.1 — Funções de Medição de Tempo

| Função | Biblioteca | Precisão | Tipo | Thread-Safe? | MPI-Safe? |
|--------|-----------|----------|------|:------------:|:---------:|
| `clock()` | stdlib | Baixa (~ms) | CPU time | ⚠️ Depende | ⚠️ Depende |
| `omp_get_wtime()` | OpenMP | Alta (~μs) | Wall-clock | ✅ SIM | ✅ SIM |
| `MPI_Wtime()` | MPI | Alta (~μs) | Wall-clock | ✅ SIM | ✅ SIM |

---

### 3.2 — `clock()` (stdlib)

**Protótipo**:
```c
#include <time.h>
clock_t clock(void);
```

**Características**:
- Mede tempo de CPU (não wall-clock)
- Retorna ticks de clock
- Converte para segundos: `(double)clock() / CLOCKS_PER_SEC`

**Vantagens**:
- Disponível em qualquer sistema C
- Nenhuma dependência externa

**Desvantagens**:
- ❌ Mede CPU time, não wall-clock time
- ❌ Em programas paralelos: pode contar tempo de TODAS as threads
- ❌ Resultado pode ser > tempo real em multi-threaded
- ❌ Precisão baixa (tipicamente milissegundos)
- ❌ **INADEQUADO para paralelo OpenMP/MPI**

**Exemplo problemático**:
```c
// Programa com 4 threads OpenMP rodando por 1 segundo cada
// clock() pode retornar ~4 segundos (soma de todas as threads)
```

**Conclusão**: ❌ **NÃO RECOMENDADO** para benchmark paralelo

---


### 3.3 — `omp_get_wtime()` (OpenMP)

**Protótipo**:
```c
#include <omp.h>
double omp_get_wtime(void);
```

**Características**:
- Mede tempo de parede (wall-clock time)
- Retorna segundos como `double`
- Alta precisão (tipicamente microsegundos)
- Thread-safe por design

**Vantagens**:
- ✅ Mede tempo real de parede (correto para paralelo)
- ✅ Alta precisão temporal
- ✅ Thread-safe (seguro em OpenMP)
- ✅ Portável (parte do padrão OpenMP)
- ✅ Overhead mínimo (~nanosegundos)
- ✅ Já usado em Serial e OMP

**Desvantagens**:
- ⚠️ Requer compilação com `-fopenmp`
- ⚠️ Não é nativo em código puramente serial

**Exemplo de uso**:
```c
double start = omp_get_wtime();
// ... código ...
double end = omp_get_wtime();
double elapsed = end - start;
```

**Compatibilidade com MPI**:
- ✅ Funciona perfeitamente em programas MPI+OpenMP
- ✅ Cada processo MPI pode medir seu próprio tempo
- ✅ Thread-safe dentro de cada processo

**Conclusão**: ✅ **RECOMENDADO** para Serial e OMP

---

### 3.4 — `MPI_Wtime()` (MPI)

**Protótipo**:
```c
#include <mpi.h>
double MPI_Wtime(void);
```

**Características**:
- Mede tempo de parede (wall-clock time)
- Retorna segundos como `double`
- Alta precisão (tipicamente microsegundos)
- Thread-safe por design

**Vantagens**:
- ✅ Mede tempo real de parede (correto para paralelo)
- ✅ Alta precisão temporal
- ✅ Thread-safe (seguro em MPI+OpenMP)
- ✅ Portável (parte do padrão MPI)
- ✅ Overhead mínimo (~nanosegundos)
- ✅ **Consistente entre processos MPI** (pode sincronizar)
- ✅ Nativo para código MPI

**Desvantagens**:
- ⚠️ Apenas disponível após `MPI_Init()`
- ⚠️ Não disponível em código serial ou OpenMP puro

**Exemplo de uso**:
```c
double start = MPI_Wtime();
// ... código ...
double end = MPI_Wtime();
double elapsed = end - start;
```

**Sincronização de relógios**:
```c
// Garantir que todos os processos iniciam medição no mesmo instante
MPI_Barrier(MPI_COMM_WORLD);
double start = MPI_Wtime();
```

**Conclusão**: ✅ **RECOMENDADO** para MPI

---


## 4. PROBLEMAS E INCONSISTÊNCIAS IDENTIFICADAS

### 4.1 — Problema Crítico: MPI Sem Medição

**Situação**: `sudoku_mpi.c` não mede tempo de execução

**Impacto**:
- ❌ Impossível calcular speedup: `S = T_serial / T_parallel`
- ❌ Impossível calcular eficiência: `E = S / P`
- ❌ Impossível comparar com Serial e OMP
- ❌ Impossível validar se paralelização está a funcionar
- ❌ Impossível identificar gargalos (comunicação vs computação)

**Urgência**: 🔴 **CRÍTICA** — Deve ser corrigido antes de qualquer benchmark

---

### 4.2 — Problema: Serial Usa OpenMP

**Situação**: `sudoku_serial.c` usa `omp_get_wtime()`

**Impacto**:
- ⚠️ Versão "serial" depende de biblioteca paralela
- ⚠️ Inconsistência conceitual
- ⚠️ Não é verdadeiramente serial puro

**Análise**:
- **Tecnicamente correto**: `omp_get_wtime()` mede wall-clock corretamente
- **Conceitualmente questionável**: Serial não deveria depender de OpenMP

**Alternativas**:
1. Manter `omp_get_wtime()` (funciona, mas é inconsistente)
2. Usar `clock()` (inadequado para comparação com paralelo)
3. Criar timer portável baseado em `gettimeofday()` (complexidade adicional)

**Decisão recomendada**: ✅ **Manter `omp_get_wtime()`** 
- Razão: Já está implementado, funciona corretamente, permite comparação justa
- Nota: Documentar que todas as versões usam wall-clock time

---

### 4.3 — Problema: Formatos de Output Diferentes

**Situação**:
- Serial: `fprintf(stderr, "%.1f\n", exec_time);` → ex: `3.2`
- OMP: `fprintf(stderr, "%.10f\n", exec_time);` → ex: `0.0123456789`
- MPI: Nenhum output de tempo

**Impacto**:
- ⚠️ Dificulta parsing automatizado
- ⚠️ Inconsistência visual
- ⚠️ Serial pode arredondar valores pequenos para 0.0

**Análise**:
- Serial com `.1f` pode perder precisão em Sudokus rápidos (<0.05s)
- OMP com `.10f` tem precisão excessiva (desnecessário)

**Exemplo problemático**:
```bash
$ ./sudoku_serial maps/map1.txt
0.0    # ← Arredondado, tempo real foi 0.04s

$ ./sudoku_omp maps/map1.txt
0.0423456789    # ← Precisão excessiva
```

---

### 4.4 — Problema: Não Mede Componentes Individuais

**Situação**: Apenas tempo total é medido

**Componentes não medidos**:
1. Tempo de loading do ficheiro
2. Tempo de geração de subproblemas (MPI)
3. Tempo de distribuição via MPI
4. Tempo de computação pura (solving)
5. Tempo de comunicação MPI
6. Tempo de recolha de resultados
ou incluir no overhead

**Impacto**:
- ⚠️ Impossível identificar gargalos
- ⚠️ Impossível distinguir overhead de comunicação vs computação
- ⚠️ Impossível otimizar partes específicas

**Relevância**: Média (útil para análise avançada, não crítico para PASSO 6 básico)

---


## 5. ESTRATÉGIA DE MEDIÇÃO RECOMENDADA

### 5.1 — Princípios Fundamentais

1. **Consistência**: Todas as versões devem usar o mesmo tipo de medição (wall-clock)
2. **Comparabilidade**: Formatos de output devem ser consistentes
3. **Precisão**: Precisão suficiente para tempos curtos (mínimo 4 casas decimais)
4. **Simplicidade**: Minimizar modificações de código existente
5. **Correção**: Medir apenas tempo de processamento (excluir I/O quando apropriado)

---

### 5.2 — Recomendação por Versão

#### **Serial** (`sudoku_serial.c`)

**Método recomendado**: `omp_get_wtime()` (manter atual)

**Modificações sugeridas**:
```c
// ANTES:
fprintf(stderr, "%.1f\n", exec_time);

// DEPOIS:
fprintf(stderr, "%.6f\n", exec_time);  // 6 casas decimais
```

**Justificação**:
- ✅ Já implementado
- ✅ Funciona corretamente (wall-clock)
- ✅ Permite comparação justa com OMP e MPI
- ✅ Apenas ajustar precisão do output

---

#### **OpenMP** (`sudoku_omp.c`)

**Método recomendado**: `omp_get_wtime()` (manter atual)

**Modificações sugeridas**:
```c
// ANTES:
fprintf(stderr, "%.10f\n", exec_time);

// DEPOIS:
fprintf(stderr, "%.6f\n", exec_time);  // 6 casas decimais (consistente com Serial)
```

**Justificação**:
- ✅ Já implementado
- ✅ Apropriado para código OpenMP
- ✅ Apenas ajustar precisão para consistência

---

#### **MPI** (`sudoku_mpi.c`)

**Método recomendado**: `MPI_Wtime()` ✅ **IMPLEMENTAR**

**O que medir**:
1. **Rank 0 (Master)**: Tempo total desde distribuição até solução
2. **Workers**: Tempo total no worker_loop

**Localização das medições**:

**Rank 0**:
```c
// INÍCIO: Após carregar Sudoku, antes de gerar subproblemas
double start_time = MPI_Wtime();

// ... geração, distribuição, resolução, recolha ...

// FIM: Após receber solução (ou encontrar localmente)
double end_time = MPI_Wtime();
double exec_time = end_time - start_time;

// Output (apenas Rank 0)
fprintf(stderr, "%.6f\n", exec_time);
```

**Workers** (opcional, para análise avançada):
```c
// INÍCIO: Ao entrar em worker_loop
double worker_start = MPI_Wtime();

// ... loop de processamento ...

// FIM: Ao sair de worker_loop
double worker_end = MPI_Wtime();
double worker_time = worker_end - worker_start;

DEBUG_LOG(rank, "Worker time: %.6f seconds", worker_time);
```

**Justificação**:
- ✅ `MPI_Wtime()` é nativo para código MPI
- ✅ Thread-safe e MPI-safe
- ✅ Consistente com `omp_get_wtime()` (ambos wall-clock)
- ✅ Alta precisão

---


### 5.3 — Formato de Output Unificado

**Formato recomendado para TODAS as versões**:

```c
fprintf(stderr, "%.6f\n", exec_time);
```

**Razão**:
- 6 casas decimais = precisão de microsegundos
- Suficiente para Sudokus rápidos (>0.000001s = 1μs)
- Não excessivo para Sudokus lentos
- Fácil de parsear automaticamente
- Consistente entre todas as versões

**Exemplos de output**:
```bash
$ ./sudoku_serial maps/map1.txt
0.042356

$ ./sudoku_omp maps/map1.txt
0.012478

$ mpirun -np 4 ./sudoku_mpi maps/map1.txt
0.005234
```

---

### 5.4 — Sincronização de Medições (MPI)

**Problema**: Processos MPI podem iniciar em momentos ligeiramente diferentes

**Solução**: Usar `MPI_Barrier()` antes de iniciar medição

```c
// Rank 0: Sincronizar antes de medir
MPI_Barrier(MPI_COMM_WORLD);  // Garantir que todos estão prontos
double start_time = MPI_Wtime();

// ... processamento ...

double end_time = MPI_Wtime();
```

**Vantagem**: Medição mais precisa, evita incluir tempo de inicialização desigual

**Desvantagem**: Adiciona overhead de barreira (~microsegundos)

**Recomendação**: ✅ **Usar barreira** — overhead é negligível comparado ao tempo de solving

---

## 6. PONTOS DE MEDIÇÃO RECOMENDADOS

### 6.1 — O Que INCLUIR na Medição

✅ **INCLUIR**:
- Geração de subproblemas (parte do algoritmo MPI)
- Distribuição de trabalho via MPI
- Tempo de computação (solving)
- Comunicação de soluções
- Broadcast de terminação
- Polling (faz parte do design MPI)

**Razão**: Estes são componentes integrais do algoritmo paralelo

---

### 6.2 — O Que EXCLUIR da Medição

❌ **EXCLUIR**:
- `MPI_Init()` / `MPI_Finalize()` (inicialização do runtime)
- Loading do ficheiro Sudoku (I/O)
- Parsing do ficheiro
- Impressão da solução (I/O)
- Validação de argumentos

**Razão**: Não fazem parte do algoritmo de resolução

---

### 6.3 — Localização Precisa das Medições

#### **Serial**:
```c
// INÍCIO: Após carregar Sudoku
exec_time = -omp_get_wtime();
solve(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
// FIM: Após solve
```
✅ **Correto** — já implementado

---

#### **OpenMP**:
```c
// INÍCIO: Após carregar Sudoku
exec_time = -omp_get_wtime();
solve_omp(sudoku->tab, sudoku->order);
exec_time += omp_get_wtime();
// FIM: Após solve_omp
```
✅ **Correto** — já implementado

---

#### **MPI** (Rank 0):
```c
// Carregar Sudoku (NÃO medido)
sudoku = load_sudoku(argv[1]);

// ========== INÍCIO MEDIÇÃO ==========
MPI_Barrier(MPI_COMM_WORLD);
double start_time = MPI_Wtime();

// Gerar subproblemas
generate_subproblems_mpi(...);

// Distribuir trabalho
dispatch_work(...);

// Resolver localmente
solve_omp(rank0_board, sudoku->order);

// Recolher solução (se necessário)
if (!is_solved(...))
    collect_solutions(...);

// Broadcast terminação
broadcast_termination(nprocs);

double end_time = MPI_Wtime();
// ========== FIM MEDIÇÃO ==========

double exec_time = end_time - start_time;
fprintf(stderr, "%.6f\n", exec_time);

// Imprimir solução (NÃO medido)
print(solution_board, sudoku->order);
```

**Componentes incluídos na medição**:
1. Geração de subproblemas (~1-5% do tempo)
2. Distribuição MPI (~1-2% do tempo)
3. Computação local Rank 0 (~30-90% do tempo)
4. Comunicação de soluções (~1-5% do tempo)
5. Terminação (~1% do tempo)

---


## 7. RESUMO DE MODIFICAÇÕES NECESSÁRIAS

### 7.1 — Prioridade CRÍTICA

| Ficheiro | Modificação | Urgência | Esforço |
|----------|-------------|:--------:|:-------:|
| `sudoku_mpi.c` | **Adicionar medição com `MPI_Wtime()`** | 🔴 CRÍTICA | Médio |

**Detalhes**:
- Adicionar `double start_time, end_time, exec_time;`
- Adicionar `MPI_Barrier()` antes de medição
- Adicionar `start_time = MPI_Wtime();` após barreira
- Adicionar `end_time = MPI_Wtime();` após terminação
- Adicionar `fprintf(stderr, "%.6f\n", exec_time);` no Rank 0

**Linhas de código**: ~10 linhas

**Impacto**: ✅ Nenhum (apenas adiciona medição, não altera lógica)

---

### 7.2 — Prioridade ALTA (Consistência)

| Ficheiro | Modificação | Urgência | Esforço |
|----------|-------------|:--------:|:-------:|
| `sudoku_serial.c` | Alterar `%.1f` → `%.6f` | 🟡 ALTA | Trivial |
| `sudoku_omp.c` | Alterar `%.10f` → `%.6f` | 🟡 ALTA | Trivial |

**Detalhes**:
- Linha única em cada ficheiro
- Mudança puramente cosmética
- Melhora consistência e precisão

**Linhas de código**: 1 linha por ficheiro

**Impacto**: ✅ Nenhum (apenas formato de output)

---

### 7.3 — Prioridade BAIXA (Opcional)

| Adição | Descrição | Urgência | Esforço |
|--------|-----------|:--------:|:-------:|
| Medição de componentes | Medir separadamente: geração, comunicação, computação | 🟢 BAIXA | Alto |
| Estatísticas MPI | Min/Max/Avg entre processos | 🟢 BAIXA | Médio |
| Output estruturado | JSON ou CSV para parsing | 🟢 BAIXA | Médio |

**Razão para baixa prioridade**: Útil para análise avançada, mas não necessário para PASSO 6 básico

---

## 8. COMPARABILIDADE E FAIRNESS

### 8.1 — Condições de Teste Justas

Para comparação justa entre Serial, OMP e MPI:

✅ **Garantir**:
1. Mesmo ficheiro de input
2. Mesma máquina / hardware
3. Mesmo tipo de medição (wall-clock)
4. Mesma precisão de output
5. Medir apenas tempo de processamento (excluir I/O)
6. Executar múltiplas vezes (média de 3-5 runs)
7. Minimizar processos concorrentes

❌ **Evitar**:
1. Comparar CPU time vs wall-clock time
2. Incluir I/O em uma versão e não em outra
3. Medições únicas (podem ter outliers)
4. Sistema com carga variável

---

### 8.2 — Cálculo de Métricas

Após ter tempos de todas as versões:

**Speedup**:
```
S_omp = T_serial / T_omp
S_mpi = T_serial / T_mpi
```

**Eficiência**:
```
E_omp = S_omp / num_threads
E_mpi = S_mpi / num_processes
```

**Exemplo**:
```
T_serial = 10.0s
T_omp (4 threads) = 3.0s
T_mpi (4 processes) = 2.5s

S_omp = 10.0 / 3.0 = 3.33x
S_mpi = 10.0 / 2.5 = 4.00x

E_omp = 3.33 / 4 = 0.83 (83%)
E_mpi = 4.00 / 4 = 1.00 (100%)
```

---


## 9. PLANO DE IMPLEMENTAÇÃO

### 9.1 — FASE 1: Adicionar Medição MPI (CRÍTICO)

**Objectivo**: Implementar `MPI_Wtime()` em `sudoku_mpi.c`

**Passos**:
1. Localizar início da computação (após `load_sudoku()`)
2. Adicionar variáveis `double start_time, end_time, exec_time;`
3. Adicionar `MPI_Barrier(MPI_COMM_WORLD);` para sincronização
4. Adicionar `start_time = MPI_Wtime();` após barreira
5. Localizar fim da computação (após `broadcast_termination()`)
6. Adicionar `end_time = MPI_Wtime();`
7. Calcular `exec_time = end_time - start_time;`
8. Adicionar `fprintf(stderr, "%.6f\n", exec_time);` (apenas Rank 0)
9. Compilar e testar

**Critério de aprovação**: MPI imprime tempo de execução formatado

---

### 9.2 — FASE 2: Uniformizar Formatos (ALTA PRIORIDADE)

**Objectivo**: Consistência de output entre todas as versões

**Passos**:
1. Abrir `sudoku_serial.c`
2. Alterar `"%.1f\n"` → `"%.6f\n"` (linha ~37)
3. Abrir `sudoku_omp.c`
4. Alterar `"%.10f\n"` → `"%.6f\n"` (linha ~38)
5. Compilar todas as versões
6. Testar output de cada versão

**Critério de aprovação**: Todas as versões imprimem com 6 casas decimais

---

### 9.3 — FASE 3: Validação (ESSENCIAL)

**Objectivo**: Garantir que medições são corretas

**Testes a realizar**:
1. Executar Serial 3x, verificar tempos similares
2. Executar OMP 3x, verificar tempos similares
3. Executar MPI (1 processo) 3x, verificar tempos
4. Executar MPI (2 processos) 3x, verificar tempos
5. Executar MPI (4 processos) 3x, verificar tempos

**Verificações**:
- ✅ Serial tem tempo consistente
- ✅ OMP é mais rápido que Serial (se threads > 1)
- ✅ MPI(1) similar a Serial ou OMP(1)
- ✅ MPI(N) mais rápido que MPI(1) quando N > 1
- ✅ Tempos fazem sentido (não há valores absurdos)

---

## 10. CONCLUSÃO

### 10.1 — Problemas Críticos Identificados

1. ❌ **`sudoku_mpi.c` não mede tempo** → Impossível fazer benchmark
2. ⚠️ **Formatos inconsistentes** → Dificulta comparação
3. ⚠️ **Serial usa OpenMP** → Inconsistência conceitual (aceitável)

---

### 10.2 — Método Recomendado

| Versão | Método | Formato | Status |
|--------|--------|---------|--------|
| **Serial** | `omp_get_wtime()` | `%.6f` | ⚠️ Ajustar formato |
| **OMP** | `omp_get_wtime()` | `%.6f` | ⚠️ Ajustar formato |
| **MPI** | `MPI_Wtime()` | `%.6f` | ❌ **IMPLEMENTAR** |

**Todos medem**: Wall-clock time (tempo de parede)  
**Todos excluem**: I/O (loading e printing)  
**Todos incluem**: Tempo de processamento puro

---

### 10.3 — Próximos Passos

**ANTES de PASSO 6 (Benchmark)**:

1. ✅ Implementar medição em `sudoku_mpi.c` com `MPI_Wtime()`
2. ✅ Uniformizar formato de output (`%.6f` em todos)
3. ✅ Compilar e testar todas as versões
4. ✅ Validar que medições são consistentes

**DEPOIS** (PASSO 6):

1. Executar benchmarks com múltiplas configurações
2. Calcular speedup e eficiência
3. Analisar resultados
4. Identificar gargalos

---

### 10.4 — Tabela de Decisões

| Decisão | Escolha | Justificação |
|---------|---------|--------------|
| **Método Serial** | `omp_get_wtime()` | Já implementado, funciona corretamente |
| **Método OMP** | `omp_get_wtime()` | Apropriado para OpenMP, já implementado |
| **Método MPI** | `MPI_Wtime()` | Nativo para MPI, consistente com OMP |
| **Formato output** | `%.6f` | Precisão adequada, consistente |
| **Sincronização MPI** | `MPI_Barrier()` | Garante medição precisa |
| **O que medir** | Apenas processamento | Comparação justa (excluir I/O) |

---

**STATUS**: ✓ **AUDITORIA COMPLETA**  
**PRÓXIMO PASSO**: Implementar modificações identificadas (FASES 1, 2, 3)

