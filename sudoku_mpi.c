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

// MPI Communication (FASE 3.3)
void send_subproblem(int **board, int order, int dest_rank);
int **recv_subproblem(int order);
void send_solution(int **board, int order);
int **recv_solution(int order);

// Work Distribution (FASE 3.4)
int generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count);
void dispatch_work(int ***subproblems, int count, int order, int nprocs);
void dispatch_work_to_self(int **board, int order, int rank);

// Worker & Solution Collection (FASE 3.5)
void worker_loop(int order);
void collect_solutions(int order, int nprocs, int **solution_board);
void broadcast_termination(int nprocs);
int check_termination(void);

// Testing (FASE 3.1 - temporary)
static int test_serialization(void);
static int test_mpi_communication(int rank);

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
 * IMPLEMENTAÇÃO: MPI COMMUNICATION LAYER (FASE 3.3)
 * ======================================================================== */

/**
 * send_subproblem - Envia subproblema para worker via MPI
 * 
 * @board: Board 2D a enviar
 * @order: Ordem do Sudoku (NOT size!)
 * @dest_rank: Rank destino
 * 
 * Fluxo:
 *   1. Calcular size = order * order
 *   2. Alocar buffer temporário
 *   3. flatten_board() para serializar
 *   4. MPI_Send() com tag MPI_TAG_SUBPROBLEM
 *   5. free(buffer)
 * 
 * Gestão de memória:
 *   - Aloca buffer temporário (size²)
 *   - Liberta buffer após envio
 *   - Não liberta board (caller é dono)
 * 
 * Tratamento de erros:
 *   - malloc falha → abort
 *   - MPI_Send falha → abort
 */
void send_subproblem(int **board, int order, int dest_rank)
{
    int size;
    int *buffer;
    int ret;
    
    size = order * order;
    
    // Alocar buffer temporário
    buffer = malloc(sizeof(int) * size * size);
    if (!buffer)
    {
        fprintf(stderr, "[ERROR] send_subproblem: malloc failed\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    // Serializar board → buffer
    flatten_board(board, size, buffer);
    
    // Enviar via MPI
    ret = MPI_Send(buffer, size * size, MPI_INT, dest_rank,
                   MPI_TAG_SUBPROBLEM, MPI_COMM_WORLD);
    if (ret != MPI_SUCCESS)
    {
        fprintf(stderr, "[ERROR] send_subproblem: MPI_Send failed (ret=%d)\n", ret);
        free(buffer);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    // Libertar buffer temporário
    free(buffer);
}

/**
 * recv_subproblem - Recebe subproblema de Rank 0 via MPI (com polling)
 * 
 * @order: Ordem do Sudoku (NOT size!)
 * 
 * @return: Board 2D alocado dinamicamente, ou NULL se receber terminação
 * 
 * Fluxo (COM POLLING para evitar deadlocks):
 *   1. Loop infinito com MPI_Iprobe() (polling)
 *   2. Se tag = MPI_TAG_TERMINATION:
 *        - Consumir mensagem (MPI_Recv dummy)
 *        - return NULL (worker deve terminar)
 *   3. Se tag = MPI_TAG_SUBPROBLEM:
 *        - MPI_Recv() para receber buffer
 *        - unflatten_board() para deserializar
 *        - return board
 *   4. Se nenhuma mensagem: usleep(POLL_INTERVAL) e repetir
 * 
 * Gestão de memória:
 *   - Aloca buffer temporário → liberta após unflatten
 *   - Aloca board 2D → CALLER deve libertar com free_board()
 * 
 * Tratamento de erros:
 *   - malloc falha → return NULL
 *   - MPI_Recv falha → abort
 *   - unflatten_board falha → return NULL
 * 
 * IMPORTANTE: Polling evita deadlock quando:
 *   - Worker aguarda subproblema
 *   - Outro worker encontra solução
 *   - Rank 0 envia terminação (tag=103)
 *   - Worker bloqueado em MPI_Recv(tag=101) não receberia terminação
 */
int **recv_subproblem(int order)
{
    int size;
    int *buffer;
    int **board;
    int ret;
    int flag;
    MPI_Status status;
    
    size = order * order;
    
    // Polling loop: aguardar mensagem (subproblem OU termination)
    while (1)
    {
        // Verificar se há mensagem disponível (qualquer tag)
        ret = MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        if (ret != MPI_SUCCESS)
        {
            fprintf(stderr, "[ERROR] recv_subproblem: MPI_Iprobe failed\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        
        if (flag)  // Mensagem disponível
        {
            // Caso 1: Terminação recebida
            if (status.MPI_TAG == MPI_TAG_TERMINATION)
            {
                // Consumir mensagem de terminação (dummy recv)
                int dummy;
                ret = MPI_Recv(&dummy, 1, MPI_INT, 0, MPI_TAG_TERMINATION,
                              MPI_COMM_WORLD, &status);
                if (ret != MPI_SUCCESS)
                {
                    fprintf(stderr, "[ERROR] recv_subproblem: MPI_Recv(TERM) failed\n");
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                
                // Retornar NULL para indicar terminação
                return NULL;
            }
            
            // Caso 2: Subproblema recebido
            else if (status.MPI_TAG == MPI_TAG_SUBPROBLEM)
            {
                // Alocar buffer temporário
                buffer = malloc(sizeof(int) * size * size);
                if (!buffer)
                {
                    fprintf(stderr, "[ERROR] recv_subproblem: malloc failed\n");
                    return NULL;
                }
                
                // Receber via MPI
                ret = MPI_Recv(buffer, size * size, MPI_INT, 0,
                              MPI_TAG_SUBPROBLEM, MPI_COMM_WORLD, &status);
                if (ret != MPI_SUCCESS)
                {
                    fprintf(stderr, "[ERROR] recv_subproblem: MPI_Recv failed\n");
                    free(buffer);
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                
                // Deserializar buffer → board
                board = unflatten_board(buffer, size);
                
                // Libertar buffer temporário
                free(buffer);
                
                if (!board)
                {
                    fprintf(stderr, "[ERROR] recv_subproblem: unflatten failed\n");
                    return NULL;
                }
                
                return board;
            }
            
            // Caso 3: Tag desconhecida (erro)
            else
            {
                fprintf(stderr, "[ERROR] recv_subproblem: unexpected tag %d\n",
                        status.MPI_TAG);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        
        // Nenhuma mensagem disponível: aguardar antes de tentar novamente
        usleep(POLL_INTERVAL);
    }
    
    // Nunca chega aqui (loop infinito até receber mensagem)
    return NULL;
}

/**
 * send_solution - Envia solução para Rank 0 via MPI
 * 
 * @board: Board 2D com solução
 * @order: Ordem do Sudoku (NOT size!)
 * 
 * Idêntico a send_subproblem() mas usa MPI_TAG_SOLUTION
 * Destino: sempre Rank 0
 * 
 * Gestão de memória:
 *   - Aloca buffer temporário → liberta após envio
 *   - Não liberta board (caller é dono)
 */
void send_solution(int **board, int order)
{
    int size;
    int *buffer;
    int ret;
    
    size = order * order;
    
    // Alocar buffer temporário
    buffer = malloc(sizeof(int) * size * size);
    if (!buffer)
    {
        fprintf(stderr, "[ERROR] send_solution: malloc failed\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    // Serializar board → buffer
    flatten_board(board, size, buffer);
    
    // Enviar para Rank 0 via MPI
    ret = MPI_Send(buffer, size * size, MPI_INT, 0,
                   MPI_TAG_SOLUTION, MPI_COMM_WORLD);
    if (ret != MPI_SUCCESS)
    {
        fprintf(stderr, "[ERROR] send_solution: MPI_Send failed (ret=%d)\n", ret);
        free(buffer);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    // Libertar buffer temporário
    free(buffer);
}

/**
 * recv_solution - Recebe solução de qualquer worker via MPI
 * 
 * @order: Ordem do Sudoku (NOT size!)
 * 
 * @return: Board 2D alocado dinamicamente, ou NULL em erro
 * 
 * Diferença de recv_subproblem():
 *   - Origem: MPI_ANY_SOURCE (não sabemos qual worker resolveu)
 *   - Tag: MPI_TAG_SOLUTION
 * 
 * Gestão de memória:
 *   - Aloca buffer temporário → liberta após unflatten
 *   - Aloca board 2D → CALLER deve libertar com free_board()
 */
int **recv_solution(int order)
{
    int size;
    int *buffer;
    int **board;
    int ret;
    MPI_Status status;
    
    size = order * order;
    
    // Alocar buffer temporário
    buffer = malloc(sizeof(int) * size * size);
    if (!buffer)
    {
        fprintf(stderr, "[ERROR] recv_solution: malloc failed\n");
        return NULL;
    }
    
    // Receber de QUALQUER worker via MPI
    ret = MPI_Recv(buffer, size * size, MPI_INT, MPI_ANY_SOURCE,
                   MPI_TAG_SOLUTION, MPI_COMM_WORLD, &status);
    if (ret != MPI_SUCCESS)
    {
        fprintf(stderr, "[ERROR] recv_solution: MPI_Recv failed (ret=%d)\n", ret);
        free(buffer);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    // Deserializar buffer → board
    board = unflatten_board(buffer, size);
    
    // Libertar buffer temporário
    free(buffer);
    
    if (!board)
    {
        fprintf(stderr, "[ERROR] recv_solution: unflatten_board failed\n");
        return NULL;
    }
    
    return board;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: WORK DISTRIBUTION (FASE 3.4)
 * ======================================================================== */

/**
 * find_first_empty - Encontra primeira célula vazia no board
 * 
 * @board: Board do Sudoku
 * @size: Tamanho do board (order²)
 * @row: Ponteiro para armazenar linha encontrada
 * @col: Ponteiro para armazenar coluna encontrada
 * 
 * @return: 1 se encontrou célula vazia, 0 se board está completo
 * 
 * Estratégia: Varredura simples linha por linha
 */
static int find_first_empty(int **board, int size, int *row, int *col)
{
    int i, j;
    
    i = 0;
    while (i < size)
    {
        j = 0;
        while (j < size)
        {
            if (board[i][j] == 0)
            {
                *row = i;
                *col = j;
                return 1;
            }
            j++;
        }
        i++;
    }
    
    return 0;  // Não há células vazias
}

/**
 * generate_subproblems_mpi - Gera subproblemas independentes para distribuição
 * 
 * @board: Board original do Sudoku
 * @order: Ordem do Sudoku (NOT size!)
 * @subproblems: Ponteiro para array de subproblemas (output)
 * @count: Ponteiro para contador de subproblemas gerados (output)
 * 
 * @return: 0 em sucesso, -1 em erro
 * 
 * Estratégia de geração:
 *   1. Encontrar primeira célula vazia
 *   2. Para cada valor possível (1..size):
 *      - Criar cópia do board
 *      - Fixar célula com valor
 *      - Adicionar aos subproblemas
 *   3. Limitar por SUBPROBLEM_MAX
 * 
 * IMPORTANTE:
 *   - NÃO valida se valor é correto (solver fará isso)
 *   - NÃO implementa backtracking (reutiliza solve_omp)
 *   - Apenas gera states iniciais diferentes
 * 
 * Gestão de memória:
 *   - Aloca array de ponteiros (*subproblems)
 *   - Aloca cada subproblema com alloc_board()
 *   - Caller deve libertar com free_board() para cada subproblema
 */
int generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count)
{
    int size;
    int row, col;
    int value;
    int i;
    int **sub;
    
    size = order * order;
    *count = 0;
    
    // Encontrar primeira célula vazia
    if (!find_first_empty(board, size, &row, &col))
    {
        fprintf(stderr, "[ERROR] generate_subproblems: no empty cells\n");
        return -1;
    }
    
    fprintf(stderr, "[GENERATION] First empty cell: [%d][%d]\n", row, col);
    
    // Alocar array de ponteiros para subproblemas
    *subproblems = malloc(sizeof(int **) * size);
    if (!*subproblems)
    {
        fprintf(stderr, "[ERROR] generate_subproblems: malloc failed\n");
        return -1;
    }
    
    // Gerar subproblema para cada valor possível
    value = 1;
    while (value <= size && *count < SUBPROBLEM_MAX)
    {
        // Alocar subproblema
        sub = alloc_board(size);
        if (!sub)
        {
            fprintf(stderr, "[ERROR] generate_subproblems: alloc_board failed\n");
            // Libertar subproblemas já alocados
            for (i = 0; i < *count; i++)
                free_board((*subproblems)[i], size);
            free(*subproblems);
            return -1;
        }
        
        // Copiar board original
        copy_board(sub, board, size);
        
        // Fixar célula vazia com valor
        sub[row][col] = value;
        
        // Adicionar aos subproblemas
        (*subproblems)[*count] = sub;
        (*count)++;
        
        value++;
    }
    
    fprintf(stderr, "[GENERATION] Generated %d subproblems\n", *count);
    
    return 0;
}

/**
 * dispatch_work - Distribui subproblemas pelos ranks usando Round-Robin
 * 
 * @subproblems: Array de subproblemas gerados
 * @count: Número total de subproblemas
 * @order: Ordem do Sudoku
 * @nprocs: Número de processos MPI
 * 
 * Estratégia Round-Robin:
 *   Sub 0 → Rank 0
 *   Sub 1 → Rank 1
 *   Sub 2 → Rank 2
 *   Sub 3 → Rank 3
 *   Sub 4 → Rank 0
 *   ...
 * 
 * IMPORTANTE:
 *   - Rank 0 NÃO envia para si próprio (guardado localmente)
 *   - Apenas ranks > 0 recebem via send_subproblem()
 * 
 * Logging: Imprime estatísticas de distribuição
 */
void dispatch_work(int ***subproblems, int count, int order, int nprocs)
{
    int i;
    int dest_rank;
    int *task_count;  // Contador de tarefas por rank
    
    // Alocar contador de tarefas
    task_count = calloc(nprocs, sizeof(int));
    if (!task_count)
    {
        fprintf(stderr, "[ERROR] dispatch_work: calloc failed\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, " WORK DISTRIBUTION (ROUND-ROBIN)\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Total subproblems: %d\n", count);
    fprintf(stderr, "MPI processes: %d\n", nprocs);
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    
    // Distribuir subproblemas
    for (i = 0; i < count; i++)
    {
        dest_rank = i % nprocs;
        
        if (dest_rank == 0)
        {
            // Rank 0: não envia para si próprio (guardado localmente)
            fprintf(stderr, "[DISPATCH] Subproblem %d → Rank 0 (local)\n", i);
        }
        else
        {
            // Ranks > 0: enviar via MPI
            send_subproblem(subproblems[i], order, dest_rank);
            fprintf(stderr, "[DISPATCH] Subproblem %d → Rank %d (sent)\n", i, dest_rank);
        }
        
        task_count[dest_rank]++;
    }
    
    // Imprimir estatísticas
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, " DISTRIBUTION SUMMARY\n");
    fprintf(stderr, "========================================\n");
    for (i = 0; i < nprocs; i++)
    {
        fprintf(stderr, "Rank %d → %d tasks\n", i, task_count[i]);
    }
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    
    free(task_count);
}

/**
 * dispatch_work_to_self - Guarda localmente o subproblema de Rank 0
 * 
 * @board: Subproblema destinado ao Rank 0
 * @order: Ordem do Sudoku
 * @rank: Rank atual (deve ser 0)
 * 
 * Propósito:
 *   - Rank 0 não envia para si próprio via MPI
 *   - Apenas armazena localmente para processamento posterior
 * 
 * IMPORTANTE:
 *   - Esta função atualmente é apenas logging
 *   - O board já está disponível em subproblems[0]
 *   - Em fases futuras, aqui se chamaria solve_omp()
 */
void dispatch_work_to_self(int **board, int order, int rank)
{
    int size = order * order;
    
    (void)board;  // Unused (por enquanto)
    (void)size;   // Unused (por enquanto)
    
    if (rank != 0)
    {
        fprintf(stderr, "[WARNING] dispatch_work_to_self called by rank %d (should be 0)\n", rank);
        return;
    }
    
    fprintf(stderr, "[RANK 0] Local subproblem ready for processing\n");
    fprintf(stderr, "[RANK 0] (solve_omp will be called in future phase)\n");
}

/**
 * check_termination - Verifica se há mensagem de terminação (não-bloqueante)
 * 
 * @return: 1 se recebeu terminação, 0 caso contrário
 * 
 * Utiliza MPI_Iprobe() para verificar sem bloquear
 */
int check_termination(void)
{
    int flag;
    MPI_Status status;
    int ret;
    
    ret = MPI_Iprobe(0, MPI_TAG_TERMINATION, MPI_COMM_WORLD, &flag, &status);
    if (ret != MPI_SUCCESS)
    {
        fprintf(stderr, "[ERROR] check_termination: MPI_Iprobe failed\n");
        return 0;
    }
    
    if (flag)
    {
        // Consumir mensagem de terminação
        int dummy;
        MPI_Recv(&dummy, 1, MPI_INT, 0, MPI_TAG_TERMINATION,
                MPI_COMM_WORLD, &status);
        return 1;
    }
    
    return 0;
}

/**
 * broadcast_termination - Notifica todos os workers para terminar
 * 
 * @nprocs: Número total de processos
 * 
 * Envia MPI_TAG_TERMINATION para todos os ranks > 0
 */
void broadcast_termination(int nprocs)
{
    int rank;
    int dummy = 0;
    int ret;
    
    fprintf(stderr, "[RANK 0] Broadcasting termination to all workers...\n");
    
    for (rank = 1; rank < nprocs; rank++)
    {
        ret = MPI_Send(&dummy, 1, MPI_INT, rank, MPI_TAG_TERMINATION,
                      MPI_COMM_WORLD);
        if (ret != MPI_SUCCESS)
        {
            fprintf(stderr, "[ERROR] broadcast_termination: MPI_Send to rank %d failed\n", rank);
        }
        else
        {
            fprintf(stderr, "[RANK 0] Sent termination to Rank %d\n", rank);
        }
    }
}

/**
 * collect_solutions - Aguarda solução de qualquer worker
 * 
 * @order: Ordem do Sudoku
 * @nprocs: Número de processos
 * @solution_board: Buffer onde armazenar solução recebida
 * 
 * Utiliza MPI_Iprobe() + polling para não bloquear indefinidamente
 * 
 * IMPORTANTE: solution_board deve estar pré-alocado
 */
void collect_solutions(int order, int nprocs, int **solution_board)
{
    int **received_solution;
    int size = order * order;
    int row, col;
    
    (void)nprocs;  // Unused
    
    fprintf(stderr, "[RANK 0] Waiting for solutions from workers...\n");
    
    // Receber solução (recv_solution usa MPI_ANY_SOURCE)
    received_solution = recv_solution(order);
    
    if (!received_solution)
    {
        fprintf(stderr, "[ERROR] collect_solutions: recv_solution failed\n");
        return;
    }
    
    fprintf(stderr, "[RANK 0] Solution received from a worker\n");
    
    // Copiar para solution_board
    for (row = 0; row < size; row++)
    {
        for (col = 0; col < size; col++)
        {
            solution_board[row][col] = received_solution[row][col];
        }
    }
    
    // Libertar board temporário
    free_board(received_solution, size);
    
    fprintf(stderr, "[RANK 0] Solution copied to solution_board\n");
}

/**
 * worker_loop - Loop principal dos workers
 * 
 * @order: Ordem do Sudoku
 * 
 * Fluxo:
 *   1. Receber subproblema (recv_subproblem com polling)
 *   2. Se NULL → terminação recebida → sair
 *   3. Resolver com solve_omp() ← REUTILIZAÇÃO
 *   4. Se resolvido (is_solved()) ← REUTILIZAÇÃO:
 *      - Enviar solução
 *      - Sair
 *   5. Senão: continuar loop
 * 
 * IMPORTANTE:
 *   - NÃO cria solver novo
 *   - REUTILIZA solve_omp() existente
 *   - REUTILIZA is_solved() existente
 */
void worker_loop(int order)
{
    int **board;
    int size = order * order;
    int rank;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    fprintf(stderr, "[RANK %d] Entering worker loop\n", rank);
    
    while (1)
    {
        // Passo 1: Receber subproblema (COM POLLING para terminação)
        fprintf(stderr, "[RANK %d] Waiting for subproblem...\n", rank);
        board = recv_subproblem(order);
        
        // Passo 2: Se NULL → terminação recebida
        if (!board)
        {
            fprintf(stderr, "[RANK %d] Termination received, exiting loop\n", rank);
            break;
        }
        
        fprintf(stderr, "[RANK %d] Subproblem received, solving...\n", rank);
        
        // Passo 3: Resolver com solve_omp() ← REUTILIZAÇÃO
        fprintf(stderr, "[RANK %d] Calling solve_omp() ← REUSE\n", rank);
        solve_omp(board, order);
        
        // Passo 4: Verificar se encontrou solução com is_solved() ← REUTILIZAÇÃO
        if (is_solved(board, order))
        {
            fprintf(stderr, "[RANK %d] Solution found! ✓\n", rank);
            fprintf(stderr, "[RANK %d] Sending solution to Rank 0\n", rank);
            
            // Enviar solução
            send_solution(board, order);
            
            // Libertar memória
            free_board(board, size);
            
            fprintf(stderr, "[RANK %d] Solution sent, exiting loop\n", rank);
            break;
        }
        else
        {
            fprintf(stderr, "[RANK %d] No solution in this subproblem\n", rank);
            
            // Libertar memória e continuar
            free_board(board, size);
        }
    }
    
    fprintf(stderr, "[RANK %d] Worker loop terminated\n", rank);
}

/* ========================================================================
 * IMPLEMENTAÇÃO: TESTE MPI COMMUNICATION (FASE 3.3 - TEMPORÁRIO)
 * ======================================================================== */

/**
 * test_mpi_communication - Testa comunicação MPI básica entre Rank 0 e Rank 1
 * 
 * @rank: Rank do processo atual
 * 
 * @return: 1 se teste passou, 0 caso contrário
 * 
 * Cenário:
 *   Rank 0:
 *     1. Cria board 4×4 (order=2)
 *     2. Preenche com padrão sequencial (1..16)
 *     3. send_subproblem(board, 2, 1)
 *     4. recv_solution(2)
 *     5. Compara com original
 * 
 *   Rank 1:
 *     1. recv_subproblem(2)
 *     2. Valida todos os elementos
 *     3. send_solution(board, 2)
 * 
 * Validações:
 *   - Integridade dos dados (elemento a elemento)
 *   - Sem memory leaks
 *   - Sem deadlocks
 */
static int test_mpi_communication(int rank)
{
    int order = 2;
    int size = order * order;  // 4
    int **board_original = NULL;
    int **board_received = NULL;
    int row, col;
    int all_match = 1;
    
    if (rank == 0)
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, " TESTE: MPI COMMUNICATION\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "Cenário: Rank 0 ↔ Rank 1\n");
        fprintf(stderr, "Board: 4×4 (order=2)\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "\n");
        
        // Passo 1: Criar board 4×4
        board_original = alloc_board(size);
        if (!board_original)
        {
            fprintf(stderr, "[RANK 0] [FAIL] alloc_board failed\n");
            return 0;
        }
        fprintf(stderr, "[RANK 0] alloc_board: OK\n");
        
        // Passo 2: Preencher com padrão sequencial
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                board_original[row][col] = row * size + col + 1;
            }
        }
        fprintf(stderr, "[RANK 0] Fill pattern: OK (values 1..16)\n");
        
        // Passo 3: Enviar subproblema para Rank 1
        send_subproblem(board_original, order, 1);
        fprintf(stderr, "[RANK 0] send_subproblem(→ Rank 1): OK\n");
        
        // Passo 4: Receber solução de Rank 1
        board_received = recv_solution(order);
        if (!board_received)
        {
            fprintf(stderr, "[RANK 0] [FAIL] recv_solution failed\n");
            free_board(board_original, size);
            return 0;
        }
        fprintf(stderr, "[RANK 0] recv_solution(← Rank 1): OK\n");
        
        // Passo 5: Comparar elemento a elemento
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                if (board_original[row][col] != board_received[row][col])
                {
                    if (all_match)  // Imprimir apenas primeiro erro
                    {
                        fprintf(stderr,
                                "[RANK 0] [FAIL] Mismatch at [%d][%d]: "
                                "original=%d, received=%d\n",
                                row, col,
                                board_original[row][col],
                                board_received[row][col]);
                    }
                    all_match = 0;
                }
            }
        }
        
        if (all_match)
        {
            fprintf(stderr, "[RANK 0] Verification: OK (all 16 elements match)\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, " [PASS] MPI COMMUNICATION TEST ✓\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, "\n");
        }
        else
        {
            fprintf(stderr, "\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, " [FAIL] MPI COMMUNICATION TEST ✗\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, "\n");
        }
        
        // Libertar memória
        free_board(board_original, size);
        free_board(board_received, size);
        
        return all_match;
    }
    else if (rank == 1)
    {
        // Passo 1: Receber subproblema de Rank 0
        board_received = recv_subproblem(order);
        if (!board_received)
        {
            fprintf(stderr, "[RANK 1] [FAIL] recv_subproblem failed\n");
            return 0;
        }
        fprintf(stderr, "[RANK 1] recv_subproblem(← Rank 0): OK\n");
        
        // Passo 2: Validar elementos (esperado: 1..16)
        for (row = 0; row < size; row++)
        {
            for (col = 0; col < size; col++)
            {
                int expected = row * size + col + 1;
                if (board_received[row][col] != expected)
                {
                    fprintf(stderr,
                            "[RANK 1] [FAIL] Invalid value at [%d][%d]: "
                            "expected=%d, got=%d\n",
                            row, col, expected, board_received[row][col]);
                    all_match = 0;
                }
            }
        }
        
        if (all_match)
        {
            fprintf(stderr, "[RANK 1] Validation: OK (all 16 elements correct)\n");
        }
        
        // Passo 3: Enviar solução de volta para Rank 0
        send_solution(board_received, order);
        fprintf(stderr, "[RANK 1] send_solution(→ Rank 0): OK\n");
        
        // Libertar memória
        free_board(board_received, size);
        
        return all_match;
    }
    
    // Outros ranks não participam do teste
    return 1;
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
            
            fprintf(stderr, "[PHASE 3.3] Next: Test MPI communication\n");
            fprintf(stderr, "\n");
            
            print_usage();
        }
        
        // FASE 3.3: Executar teste de comunicação MPI (requer nprocs >= 2)
        if (nprocs >= 2)
        {
            fprintf(stderr, "[PHASE 3.3] Running MPI communication test...\n");
            
            int mpi_comm_result = test_mpi_communication(rank);
            
            if (rank == 0)
            {
                if (mpi_comm_result)
                {
                    fprintf(stderr, "[PHASE 3.3] MPI communication validated ✓\n");
                }
                else
                {
                    fprintf(stderr, "[PHASE 3.3] MPI communication test FAILED ✗\n");
                    fprintf(stderr, "[ERROR] Cannot proceed to work distribution\n");
                    MPI_Finalize();
                    return 1;
                }
            }
        }
        else
        {
            if (rank == 0)
            {
                fprintf(stderr, "[PHASE 3.3] Skipped MPI communication test (requires nprocs >= 2)\n");
            }
        }
        
        if (rank == 0)
        {
            fprintf(stderr, "[PHASE 3.4] Next: Implement work distribution\n");
            fprintf(stderr, "\n");
        }
        
        MPI_Finalize();
        return 0;
    }
    
    /* ====================================================================
     * FASE 3.5: SOLVER HÍBRIDO MPI+OPENMP COMPLETO
     * ==================================================================== */
    
    // Variáveis para distribuição MPI
    t_sudoku *sudoku = NULL;
    int ***subproblems = NULL;
    int subproblems_count = 0;
    int size = 0;
    
    if (rank == 0)
    {
        int **rank0_board = NULL;
        int **solution_board = NULL;
        int solution_found = 0;
        
        fprintf(stderr, "\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, " PHASE 3.5: HYBRID MPI+OpenMP SOLVER\n");
        fprintf(stderr, "========================================\n");
        fprintf(stderr, "\n");
        
        // Passo 1: Carregar Sudoku
        fprintf(stderr, "[RANK 0] Loading Sudoku from: %s\n", argv[1]);
        sudoku = load_sudoku(argv[1]);
        if (!sudoku)
        {
            fprintf(stderr, "[ERROR] Failed to load Sudoku\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        fprintf(stderr, "[RANK 0] Sudoku loaded: order=%d, size=%d×%d\n",
                sudoku->order, sudoku->order * sudoku->order,
                sudoku->order * sudoku->order);
        
        size = sudoku->order * sudoku->order;
        
        // Passo 2: Gerar subproblemas
        fprintf(stderr, "[RANK 0] Generating subproblems...\n");
        int ret = generate_subproblems_mpi(sudoku->tab, sudoku->order,
                                          &subproblems, &subproblems_count);
        if (ret != 0 || subproblems_count == 0)
        {
            fprintf(stderr, "[ERROR] Failed to generate subproblems\n");
            free_sudoku(sudoku);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        
        // Passo 3: Broadcast parâmetros para todos os ranks
        fprintf(stderr, "[RANK 0] Broadcasting parameters...\n");
        MPI_Bcast(&sudoku->order, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&subproblems_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
        fprintf(stderr, "[RANK 0] Broadcast: order=%d, count=%d\n",
                sudoku->order, subproblems_count);
        
        // Passo 4: Distribuir trabalho (Round-Robin)
        fprintf(stderr, "[RANK 0] Dispatching work...\n");
        dispatch_work(subproblems, subproblems_count, sudoku->order, nprocs);
        
        // Passo 5: Resolver subproblema local (Rank 0)
        rank0_board = subproblems[0];  // Primeiro subproblema
        fprintf(stderr, "[RANK 0] Solving local subproblem with solve_omp() ← REUSE\n");
        solve_omp(rank0_board, sudoku->order);
        
        // Passo 6: Verificar se Rank 0 encontrou solução
        if (is_solved(rank0_board, sudoku->order))
        {
            fprintf(stderr, "[RANK 0] Solution found locally! ✓\n");
            solution_board = alloc_board(size);
            if (solution_board)
            {
                copy_board(solution_board, rank0_board, size);
                solution_found = 1;
            }
        }
        else
        {
            fprintf(stderr, "[RANK 0] No solution in local subproblem\n");
            fprintf(stderr, "[RANK 0] Waiting for solutions from workers...\n");
            
            // Alocar buffer para solução
            solution_board = alloc_board(size);
            if (!solution_board)
            {
                fprintf(stderr, "[ERROR] Failed to allocate solution_board\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            
            // Aguardar solução de workers
            collect_solutions(sudoku->order, nprocs, solution_board);
            solution_found = 1;
        }
        
        // Passo 7: Enviar terminação para todos os workers
        fprintf(stderr, "[RANK 0] Sending termination to all workers\n");
        broadcast_termination(nprocs);
        
        // Passo 8: Imprimir solução
        fprintf(stderr, "\n");
        fprintf(stderr, "========================================\n");
        if (solution_found && solution_board)
        {
            fprintf(stderr, " SOLUTION FOUND ✓\n");
            fprintf(stderr, "========================================\n");
            fprintf(stderr, "\n");
            print(solution_board, sudoku->order);
        }
        else
        {
            fprintf(stderr, " NO SOLUTION FOUND ✗\n");
            fprintf(stderr, "========================================\n");
        }
        fprintf(stderr, "\n");
        
        // Libertar memória
        free_sudoku(sudoku);
        for (int i = 0; i < subproblems_count; i++)
        {
            free_board(subproblems[i], size);
        }
        free(subproblems);
        if (solution_board)
            free_board(solution_board, size);
    }
    else
    {
        // Workers: Receber parâmetros broadcast
        int order_recv;
        int count_recv;
        
        MPI_Bcast(&order_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&count_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        fprintf(stderr, "[RANK %d] Received parameters: order=%d, count=%d\n",
                rank, order_recv, count_recv);
        fprintf(stderr, "[RANK %d] Starting worker loop\n", rank);
        
        // Entrar no loop de worker
        worker_loop(order_recv);
        
        fprintf(stderr, "[RANK %d] Worker loop completed\n", rank);
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
