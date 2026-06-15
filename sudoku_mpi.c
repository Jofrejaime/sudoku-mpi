/* ========================================================================
 * SUDOKU MPI + OPENMP HÍBRIDO
 * 
 * Arquitectura:
 * - MPI: Distribuição de subproblemas entre processos
 * - OpenMP: Paralelização local dentro de cada processo
 * - Reutilização integral de solve_omp() e is_solved()
 * 
 * Referências:
 * - ARQUITETURA_MPI_HIBRIDO.md
 * - REVISAO_IMPLEMENTABILIDADE.md
 * 
 * FASE 2: Infraestrutura MPI+OpenMP apenas
 * - MPI_Init_thread() com MPI_THREAD_SERIALIZED
 * - Configuração automática/manual de threads OpenMP
 * - Validação de ambiente híbrido
 * ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "sudoku.h"

/* DEBUG flag (pode ser activado em compilação: -DDEBUG_MPI) */
#ifdef DEBUG_MPI
    #define DEBUG_LOG(rank, fmt, ...) \
        fprintf(stderr, "[DEBUG Rank %d] " fmt "\n", rank, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(rank, fmt, ...) /* nop */
#endif

/* ========================================================================
 * CONSTANTES MPI
 * ======================================================================== */

// MPI Tags conforme arquitectura aprovada
#define MPI_TAG_SUBPROBLEM    101
#define MPI_TAG_SOLUTION      102
#define MPI_TAG_TERMINATION   103

// Parâmetros de geração de subproblemas
#define SUBPROBLEM_DEPTH_LIMIT 2
#define SUBPROBLEM_MAX 1024

// Polling interval (5ms conforme validação PASSO 1)
#define POLL_INTERVAL 5000  // microseconds

/* ========================================================================
 * PROTÓTIPOS DE FUNÇÕES
 * ======================================================================== */

// Configuration
void configure_openmp(int rank, int nprocs);

// Serialization (FASE 3.1)
void flatten_board(int **board, int size, int *buffer);
int **unflatten_board(int *buffer, int size);

// Testing (FASE 3.1 - temporary)
static int test_serialization(void);

// Utility
static void print_usage(void);

// Main entry
int main(int argc, char *argv[]);

/* ========================================================================
 * IMPLEMENTAÇÃO: CONFIGURAÇÃO OPENMP
 * ======================================================================== */

/**
 * configure_openmp - Configura threads OpenMP por processo MPI
 * 
 * @rank: Rank MPI actual
 * @nprocs: Número total de processos MPI
 * 
 * Comportamento:
 * - MODO MANUAL: Se OMP_NUM_THREADS definido, aplica explicitamente
 * - MODO AUTOMÁTICO: Calcula threads = max(1, cores / nprocs)
 * 
 * Razão:
 * Evitar oversubscription quando múltiplos processos MPI executam
 * na mesma máquina. Cada processo deve usar apenas sua quota de cores.
 */
void configure_openmp(int rank, int nprocs)
{
    int available_procs;
    int num_threads;
    char *omp_env;
    
    // Detectar cores disponíveis
    available_procs = omp_get_num_procs();
    
    // WARNING: Detectar oversubscription potencial
    if (nprocs > available_procs)
    {
        if (rank == 0)
        {
            fprintf(stderr, "[WARNING] Oversubscription detected: "
                    "%d MPI processes on %d cores\n", 
                    nprocs, available_procs);
            fprintf(stderr, "[WARNING] Performance may be degraded\n");
        }
    }
    
    // MODO MANUAL: Respeitar OMP_NUM_THREADS se definido
    omp_env = getenv("OMP_NUM_THREADS");
    if (omp_env != NULL)
    {
        num_threads = atoi(omp_env);
        if (num_threads > 0)
        {
            // CORRIGIDO: Aplicar explicitamente mesmo em modo manual
            omp_set_num_threads(num_threads);
            
            DEBUG_LOG(rank, "OpenMP: Manual mode, threads=%d", num_threads);
            
            if (rank == 0)
            {
                fprintf(stderr, "[OpenMP] Manual configuration: %d threads per process\n",
                        num_threads);
            }
            return;
        }
    }
    
    // MODO AUTOMÁTICO: Distribuir cores entre processos MPI
    num_threads = available_procs / nprocs;
    if (num_threads < 1)
        num_threads = 1;  // Mínimo 1 thread por processo
    
    omp_set_num_threads(num_threads);
    
    DEBUG_LOG(rank, "OpenMP: Auto mode, threads=%d", num_threads);
    
    if (rank == 0)
    {
        fprintf(stderr, "[OpenMP] Auto-configured: %d threads per process "
                "(cores=%d, processes=%d)\n",
                num_threads, available_procs, nprocs);
    }
}

/* ========================================================================
 * IMPLEMENTAÇÃO: SERIALIZAÇÃO (FASE 3.1)
 * ======================================================================== */

/**
 * flatten_board - Serializa board 2D para buffer 1D
 * 
 * @board: Board 2D (int**) a serializar
 * @size: Dimensão do board (size × size)
 * @buffer: Buffer 1D pré-alocado (deve ter tamanho size²)
 * 
 * Formato de serialização:
 *   Row-major order: board[row][col] → buffer[row * size + col]
 * 
 * Exemplo 4×4:
 *   board:          buffer:
 *   1  2  3  4      [1, 2, 3, 4,
 *   5  6  7  8   →   5, 6, 7, 8,
 *   9  10 11 12      9, 10, 11, 12,
 *   13 14 15 16      13, 14, 15, 16]
 * 
 * Complexidade:
 *   Temporal: O(size²) - percorre cada elemento uma vez
 *   Espacial: O(1) - apenas índices auxiliares
 * 
 * Garantias:
 *   - Preserva todos os valores (nenhuma perda de informação)
 *   - Ordem determinística (sempre row-major)
 *   - Compatível com MPI_Send() (buffer contíguo)
 * 
 * Pré-condições:
 *   - board != NULL
 *   - buffer != NULL (pré-alocado com size² elementos)
 *   - size > 0
 */
void flatten_board(int **board, int size, int *buffer)
{
    int index;
    int row;
    int col;
    
    index = 0;
    row = 0;
    while (row < size)
    {
        col = 0;
        while (col < size)
        {
            buffer[index] = board[row][col];
            index++;
            col++;
        }
        row++;
    }
}

/**
 * unflatten_board - Deserializa buffer 1D para board 2D
 * 
 * @buffer: Buffer 1D com dados serializados
 * @size: Dimensão do board (size × size)
 * 
 * @return: Board 2D (int**) alocado dinamicamente, ou NULL em erro
 * 
 * Processo:
 *   1. Aloca array de ponteiros (size ponteiros)
 *   2. Para cada linha: aloca array de ints (size elementos)
 *   3. Copia dados de buffer para board usando row-major order
 * 
 * Complexidade:
 *   Temporal: O(size²) - aloca e copia size² elementos
 *   Espacial: O(size²) - aloca novo board completo
 * 
 * Gestão de memória:
 *   - Aloca memória dinamicamente (caller deve liberar com free_board)
 *   - Em caso de erro de alocação: libera memória parcial e retorna NULL
 *   - Rollback automático em caso de falha
 * 
 * Edge cases:
 *   - buffer == NULL → return NULL
 *   - size <= 0 → return NULL
 *   - malloc falha → liberta tudo alocado até agora, return NULL
 */
int **unflatten_board(int *buffer, int size)
{
    int **board;
    int index;
    int row;
    int col;
    
    // Validação de entrada
    if (!buffer || size <= 0)
        return NULL;
    
    // Passo 1: Alocar array de ponteiros para linhas
    board = malloc(sizeof(int *) * size);
    if (!board)
        return NULL;
    
    // Passo 2: Alocar cada linha e copiar dados
    row = 0;
    while (row < size)
    {
        board[row] = malloc(sizeof(int) * size);
        
        // Tratamento de erro: rollback se malloc falha
        if (!board[row])
        {
            // Liberar todas as linhas alocadas até agora
            while (row > 0)
            {
                row--;
                free(board[row]);
            }
            free(board);
            return NULL;
        }
        
        // Passo 3: Copiar dados do buffer para esta linha
        col = 0;
        while (col < size)
        {
            index = row * size + col;
            board[row][col] = buffer[index];
            col++;
        }
        
        row++;
    }
    
    return board;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: TESTES (FASE 3.1 - TEMPORÁRIO)
 * ======================================================================== */

/**
 * test_serialization - Testa flatten/unflatten com múltiplos tamanhos
 * 
 * Testa:
 *   - 4×4 (pequeno)
 *   - 9×9 (Sudoku padrão)
 *   - 16×16 (Sudoku grande)
 * 
 * Para cada tamanho:
 *   1. Aloca e preenche board original
 *   2. Flatten para buffer 1D
 *   3. Unflatten de volta para board 2D
 *   4. Compara elemento a elemento
 *   5. Libera memória
 * 
 * @return: 1 se todos os testes passaram, 0 se algum falhou
 */
static int test_serialization(void)
{
    int sizes[] = {4, 9, 16};
    int num_tests = 3;
    int test_idx;
    int size;
    int **original;
    int *buffer;
    int **reconstructed;
    int row, col;
    int all_passed = 1;
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, " SERIALIZATION TESTS\n");
    fprintf(stderr, "========================================\n");
    
    for (test_idx = 0; test_idx < num_tests; test_idx++)
    {
        size = sizes[test_idx];
        
        fprintf(stderr, "\nTest %d: Board %d×%d\n", test_idx + 1, size, size);
        
        // Passo 1: Alocar e preencher board original
        original = malloc(sizeof(int *) * size);
        if (!original)
        {
            fprintf(stderr, "  [FAIL] Memory allocation failed\n");
            all_passed = 0;
            continue;
        }
        
        for (row = 0; row < size; row++)
        {
            original[row] = malloc(sizeof(int) * size);
            if (!original[row])
            {
                fprintf(stderr, "  [FAIL] Memory allocation failed\n");
                while (row > 0)
                {
                    row--;
                    free(original[row]);
                }
                free(original);
                all_passed = 0;
                continue;
            }
            
            // Preencher com padrão: value = row * size + col + 1
            for (col = 0; col < size; col++)
            {
                original[row][col] = row * size + col + 1;
            }
        }
        
        // Passo 2: Alocar buffer e flatten
        buffer = malloc(sizeof(int) * size * size);
        if (!buffer)
        {
            fprintf(stderr, "  [FAIL] Buffer allocation failed\n");
            for (row = 0; row < size; row++)
                free(original[row]);
            free(original);
            all_passed = 0;
            continue;
        }
        
        flatten_board(original, size, buffer);
        fprintf(stderr, "  Flatten: OK (serialized %d elements)\n", size * size);
        
        // Passo 3: Unflatten de volta
        reconstructed = unflatten_board(buffer, size);
        if (!reconstructed)
        {
            fprintf(stderr, "  [FAIL] Unflatten failed\n");
            free(buffer);
            for (row = 0; row < size; row++)
                free(original[row]);
            free(original);
            all_passed = 0;
            continue;
        }
        
        fprintf(stderr, "  Unflatten: OK (reconstructed %d×%d board)\n", 
                size, size);
        
        // Passo 4: Comparar elemento a elemento
        int mismatch = 0;
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                if (original[row][col] != reconstructed[row][col])
                {
                    if (!mismatch)  // Imprimir apenas primeiro erro
                    {
                        fprintf(stderr, 
                                "  [FAIL] Mismatch at [%d][%d]: "
                                "original=%d, reconstructed=%d\n",
                                row, col, 
                                original[row][col], 
                                reconstructed[row][col]);
                    }
                    mismatch = 1;
                }
            }
        }
        
        if (!mismatch)
        {
            fprintf(stderr, "  Verification: OK (all %d elements match)\n",
                    size * size);
            fprintf(stderr, "  [PASS] Test %d×%d\n", size, size);
        }
        else
        {
            fprintf(stderr, "  [FAIL] Test %d×%d\n", size, size);
            all_passed = 0;
        }
        
        // Passo 5: Liberar memória
        free(buffer);
        for (row = 0; row < size; row++)
        {
            free(original[row]);
            free(reconstructed[row]);
        }
        free(original);
        free(reconstructed);
    }
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    if (all_passed)
        fprintf(stderr, " ALL TESTS PASSED ✓\n");
    else
        fprintf(stderr, " SOME TESTS FAILED ✗\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    
    return all_passed;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: TEST BOARD UTILITIES
 * ======================================================================== */

/**
 * test_board_utils - Testa as funções de manipulação de boards
 * 
 * Valida:
 * 1. alloc_board() - Alocação de matriz 2D
 * 2. free_board() - Libertação de matriz 2D
 * 3. copy_board() - Cópia elemento a elemento
 * 
 * @return: 1 se todos os testes passaram, 0 caso contrário
 */
static int test_board_utils(void)
{
    int test_sizes[] = {4, 9, 16};
    int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    int all_passed = 1;
    int t, size, row, col;
    int **board1, **board2;
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, " TESTE: BOARD UTILITIES\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Testes: alloc_board, free_board, copy_board\n");
    fprintf(stderr, "Tamanhos: 4×4, 9×9, 16×16\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    
    for (t = 0; t < num_tests; t++)
    {
        size = test_sizes[t];
        fprintf(stderr, "Teste %d/%d: Board %d×%d\n", t + 1, num_tests, size, size);
        
        // Teste 1: Alocar board1
        board1 = alloc_board(size);
        if (!board1)
        {
            fprintf(stderr, "  [FAIL] alloc_board() returned NULL\n");
            all_passed = 0;
            continue;
        }
        fprintf(stderr, "  alloc_board(board1): OK\n");
        
        // Teste 2: Preencher board1 com padrão
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                board1[row][col] = row * size + col + 1;
            }
        }
        fprintf(stderr, "  Fill pattern: OK (values 1..%d)\n", size * size);
        
        // Teste 3: Alocar board2
        board2 = alloc_board(size);
        if (!board2)
        {
            fprintf(stderr, "  [FAIL] alloc_board(board2) returned NULL\n");
            free_board(board1, size);
            all_passed = 0;
            continue;
        }
        fprintf(stderr, "  alloc_board(board2): OK\n");
        
        // Teste 4: Inicializar board2 com zeros (para verificar que copy realmente copia)
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                board2[row][col] = 0;
            }
        }
        
        // Teste 5: Copiar board1 → board2
        copy_board(board2, board1, size);
        fprintf(stderr, "  copy_board(board1 → board2): OK\n");
        
        // Teste 6: Verificar que board2 == board1
        int mismatch = 0;
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                if (board2[row][col] != board1[row][col])
                {
                    if (!mismatch)  // Imprimir apenas primeiro erro
                    {
                        fprintf(stderr, 
                                "  [FAIL] Mismatch at [%d][%d]: "
                                "board1=%d, board2=%d\n",
                                row, col, 
                                board1[row][col], 
                                board2[row][col]);
                    }
                    mismatch = 1;
                }
            }
        }
        
        if (!mismatch)
        {
            fprintf(stderr, "  Verification: OK (all %d elements copied correctly)\n",
                    size * size);
        }
        else
        {
            fprintf(stderr, "  [FAIL] Copy verification failed\n");
            all_passed = 0;
        }
        
        // Teste 7: Modificar board2 e verificar que board1 não foi afetado
        board2[0][0] = -999;
        if (board1[0][0] == -999)
        {
            fprintf(stderr, "  [FAIL] Deep copy failed (boards are aliased)\n");
            all_passed = 0;
        }
        else
        {
            fprintf(stderr, "  Deep copy: OK (boards are independent)\n");
        }
        
        // Teste 8: Libertar ambos os boards
        free_board(board1, size);
        free_board(board2, size);
        fprintf(stderr, "  free_board(board1, board2): OK\n");
        
        if (!mismatch)
        {
            fprintf(stderr, "  [PASS] Test %d×%d\n", size, size);
        }
        else
        {
            fprintf(stderr, "  [FAIL] Test %d×%d\n", size, size);
        }
        
        fprintf(stderr, "\n");
    }
    
    fprintf(stderr, "========================================\n");
    if (all_passed)
        fprintf(stderr, " ALL TESTS PASSED ✓\n");
    else
        fprintf(stderr, " SOME TESTS FAILED ✗\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    
    return all_passed;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: UTILITY
 * ======================================================================== */

/**
 * print_usage - Imprime instruções de uso
 * 
 * Nota FASE 2: Aceita execução sem argumentos para validação de infraestrutura
 */
static void print_usage(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "USAGE:\n");
    fprintf(stderr, "  mpirun -np <N> %s [ficheiro_sudoku]\n", "sudoku_mpi.exe");
    fprintf(stderr, "\n");
    fprintf(stderr, "PHASE 2 - Infrastructure validation:\n");
    fprintf(stderr, "  mpirun -np 4 %s\n", "sudoku_mpi.exe");
    fprintf(stderr, "  (validates MPI+OpenMP infrastructure without solving)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "EXAMPLES:\n");
    fprintf(stderr, "  mpirun -np 4 %s Sample\\ instances/9x9.txt\n", "sudoku_mpi.exe");
    fprintf(stderr, "  OMP_NUM_THREADS=2 mpirun -np 4 %s Sample\\ instances/16x16.txt\n", "sudoku_mpi.exe");
    fprintf(stderr, "\n");
}

/* ========================================================================
 * IMPLEMENTAÇÃO: MAIN
 * ======================================================================== */

/**
 * main - Entry point do programa híbrido MPI + OpenMP
 * 
 * FASE 2: Validação de infraestrutura MPI+OpenMP
 * 
 * Fluxo actual:
 * 1. MPI_Init_thread() com suporte MPI_THREAD_SERIALIZED
 * 2. Obter rank e nprocs
 * 3. Configurar OpenMP automaticamente ou manualmente
 * 4. Validar configuração híbrida
 * 5. MPI_Finalize()
 * 
 * Fases futuras adicionarão:
 * - Carregamento de Sudoku (Rank 0)
 * - Geração de subproblemas
 * - Distribuição MPI
 * - Resolução com solve_omp()
 * - Recolha de solução
 */
int main(int argc, char *argv[])
{
    int rank, nprocs;
    int provided;
    
    /* ====================================================================
     * FASE 2.2: MPI INIT COM THREAD SUPPORT
     * ==================================================================== */
    
    // Inicializar MPI com suporte para threads
    // MPI_THREAD_SERIALIZED: Apenas uma thread por vez pode chamar MPI
    // (Suficiente porque apenas main thread chama MPI, OpenMP threads
    //  nunca chamam funções MPI)
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
    
    // Validar nível de thread support obtido
    if (provided < MPI_THREAD_SERIALIZED)
    {
        fprintf(stderr, "[MPI ERROR] Insufficient thread support\n");
        fprintf(stderr, "  Required: MPI_THREAD_SERIALIZED\n");
        fprintf(stderr, "  Provided: %d\n", provided);
        fprintf(stderr, "  Cannot proceed with hybrid MPI+OpenMP\n");
        MPI_Finalize();
        return 1;
    }
    
    /* ====================================================================
     * FASE 2.3: OBTER RANK E NPROCS
     * ==================================================================== */
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    
    DEBUG_LOG(rank, "Started (total processes: %d)", nprocs);
    
    /* ====================================================================
     * FASE 2.4: CONFIGURAÇÃO OPENMP
     * ==================================================================== */
    
    configure_openmp(rank, nprocs);
    
    /* ====================================================================
     * FASE 2.6: VALIDAÇÃO DE INFRAESTRUTURA
     * ==================================================================== */
    
    // FASE 2: Permitir execução sem argumentos (validação de infraestrutura)
    // FASE 3.1: Executar testes de serialização
    // FASE 3+: Requerer ficheiro Sudoku
    if (argc < 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, " HYBRID MPI+OpenMP INITIALIZED\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, "MPI Processes:     %d\n", nprocs);
            fprintf(stderr, "OpenMP Threads:    %d (per process)\n", 
                    omp_get_max_threads());
            fprintf(stderr, "Total Parallelism: %d × %d = %d parallel threads\n",
                    nprocs, omp_get_max_threads(), 
                    nprocs * omp_get_max_threads());
            fprintf(stderr, "Thread Support:    ");
            
            switch (provided)
            {
                case MPI_THREAD_SINGLE:
                    fprintf(stderr, "MPI_THREAD_SINGLE\n");
                    break;
                case MPI_THREAD_FUNNELED:
                    fprintf(stderr, "MPI_THREAD_FUNNELED\n");
                    break;
                case MPI_THREAD_SERIALIZED:
                    fprintf(stderr, "MPI_THREAD_SERIALIZED ✓\n");
                    break;
                case MPI_THREAD_MULTIPLE:
                    fprintf(stderr, "MPI_THREAD_MULTIPLE ✓\n");
                    break;
                default:
                    fprintf(stderr, "UNKNOWN\n");
            }
            
            fprintf(stderr, "========================================\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "[PHASE 2] Infrastructure validated ✓\n");
            
            // FASE 3.1: Executar testes de serialização
            fprintf(stderr, "[PHASE 3.1] Running serialization tests...\n");
            
            int test_result = test_serialization();
            
            if (test_result)
            {
                fprintf(stderr, "[PHASE 3.1] Serialization validated ✓\n");
            }
            else
            {
                fprintf(stderr, "[PHASE 3.1] Serialization tests FAILED ✗\n");
                fprintf(stderr, "[ERROR] Cannot proceed to communication phase\n");
                MPI_Finalize();
                return 1;
            }
            
            // FASE 3.2: Executar testes de board utilities
            fprintf(stderr, "[PHASE 3.2] Running board utilities tests...\n");
            
            int board_utils_result = test_board_utils();
            
            if (board_utils_result)
            {
                fprintf(stderr, "[PHASE 3.2] Board utilities validated ✓\n");
            }
            else
            {
                fprintf(stderr, "[PHASE 3.2] Board utilities tests FAILED ✗\n");
                fprintf(stderr, "[ERROR] Cannot proceed to communication phase\n");
                MPI_Finalize();
                return 1;
            }
            
            fprintf(stderr, "[PHASE 3.3] Next: Implement MPI communication\n");
            fprintf(stderr, "\n");
            
            print_usage();
        }
        
        MPI_Finalize();
        return 0;
    }
    
    /* ====================================================================
     * FASE 3+: PROCESSAMENTO DE SUDOKU
     * ==================================================================== */
    
    if (rank == 0)
    {
        fprintf(stderr, "[TODO] Sudoku processing not yet implemented\n");
        fprintf(stderr, "[TODO] File: %s\n", argv[1]);
        fprintf(stderr, "[PHASE 3] Will implement: load, generate, dispatch, solve, collect\n");
    }
    
    /* ====================================================================
     * FINALIZAÇÃO MPI
     * 
     * JUSTIFICAÇÃO: MPI_Barrier() REMOVIDO
     * 
     * MPI_Finalize() implicitamente sincroniza todos os processos antes
     * de terminar. MPI_Barrier() explícito é redundante neste contexto.
     * 
     * Apenas seria necessário se houvesse operações após barreira mas
     * antes de Finalize (não é o caso).
     * ==================================================================== */
    
    DEBUG_LOG(rank, "Finalizing MPI");
    
    MPI_Finalize();
    
    return 0;
}
