/* ========================================================================
 * SUDOKU MPI + OPENMP HÍBRIDO
 * 
 * Solver híbrido de Sudoku utilizando MPI para distribuição de trabalho
 * entre processos e OpenMP para paralelização local.
 * 
 * Arquitectura:
 * - MPI: Distribuição de subproblemas entre processos (Round-Robin)
 * - OpenMP: Paralelização local do solver backtracking
 * - Reutilização integral de solve_omp() e is_solved()
 * 
 * Referências:
 * - ARQUITETURA_MPI_HIBRIDO.md
 * - REVISAO_IMPLEMENTABILIDADE.md
 * ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "sudoku.h"
#include "benchmark/benchmark.h"

/* DEBUG flag (activar em compilação: -DDEBUG_MPI) */
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

// Work Distribution
int generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count);
void dispatch_work(int ***subproblems, int count, int order, int nprocs);

// Worker & Solution Collection
void worker_loop(int order);
void collect_solutions(int order, int nprocs, int **solution_board);
void broadcast_termination(int nprocs);
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
 * IMPLEMENTAÇÃO: MPI COMMUNICATION LAYER
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
/**
 * is_valid_placement - Verifica se 'value' pode ser colocado em board[row][col]
 *
 * Verifica conflitos em linha, coluna e bloco order×order.
 * Evita gerar subproblemas impossíveis que causam "Invalid initial board"
 * ou "Nenhuma solução" nos workers.
 *
 * @return: 1 se válido, 0 se conflito
 */
static int is_valid_placement(int **board, int size, int order, int row, int col, int value)
{
    int i;
    int box_row;
    int box_col;
    int r;
    int c;

    /* Verificar linha */
    i = 0;
    while (i < size)
    {
        if (board[row][i] == value)
            return 0;
        i++;
    }

    /* Verificar coluna */
    i = 0;
    while (i < size)
    {
        if (board[i][col] == value)
            return 0;
        i++;
    }

    /* Verificar bloco order×order */
    box_row = (row / order) * order;
    box_col = (col / order) * order;
    r = box_row;
    while (r < box_row + order)
    {
        c = box_col;
        while (c < box_col + order)
        {
            if (board[r][c] == value)
                return 0;
            c++;
        }
        r++;
    }

    return 1;
}

int generate_subproblems_mpi(int **board, int order, int ****subproblems, int *count)
{
    int size;
    int row, col;
    int value;
    int i;
    int **sub;

    size = order * order;
    *count = 0;

    /* Encontrar primeira célula vazia */
    if (!find_first_empty(board, size, &row, &col))
    {
        fprintf(stderr, "[ERROR] generate_subproblems: no empty cells\n");
        return -1;
    }

    DEBUG_LOG(rank, "First empty cell: [%d][%d]", row, col);

    /* Alocar array de ponteiros para subproblemas */
    *subproblems = malloc(sizeof(int **) * size);
    if (!*subproblems)
    {
        fprintf(stderr, "[ERROR] generate_subproblems: malloc failed\n");
        return -1;
    }

    /* Gerar subproblema apenas para valores sem conflito imediato.
     * Valores inválidos causariam "Invalid initial board" ou "Nenhuma solução"
     * nos workers, poluindo o output e desperdiçando trabalho. */
    value = 1;
    while (value <= size && *count < SUBPROBLEM_MAX)
    {
        if (is_valid_placement(board, size, order, row, col, value))
        {
            sub = alloc_board(size);
            if (!sub)
            {
                fprintf(stderr, "[ERROR] generate_subproblems: alloc_board failed\n");
                for (i = 0; i < *count; i++)
                    free_board((*subproblems)[i], size);
                free(*subproblems);
                return -1;
            }

            copy_board(sub, board, size);
            sub[row][col] = value;

            (*subproblems)[*count] = sub;
            (*count)++;
        }
        value++;
    }

    DEBUG_LOG(0, "Generated %d subproblems", *count);

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
    
    DEBUG_LOG(0, "Distributing %d subproblems across %d processes", count, nprocs);
    
    // Distribuir subproblemas usando Round-Robin
    for (i = 0; i < count; i++)
    {
        dest_rank = i % nprocs;
        
        if (dest_rank > 0)
        {
            // Ranks > 0: enviar via MPI
            send_subproblem(subproblems[i], order, dest_rank);
            DEBUG_LOG(0, "Sent subproblem %d to Rank %d", i, dest_rank);
        }
    }
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
    
    DEBUG_LOG(0, "Broadcasting termination to %d workers", nprocs - 1);
    
    for (rank = 1; rank < nprocs; rank++)
    {
        ret = MPI_Send(&dummy, 1, MPI_INT, rank, MPI_TAG_TERMINATION,
                      MPI_COMM_WORLD);
        if (ret != MPI_SUCCESS)
        {
            fprintf(stderr, "[ERROR] broadcast_termination: MPI_Send to rank %d failed\n", rank);
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
    
    (void)nprocs;
    
    DEBUG_LOG(0, "Waiting for solution from workers");
    
    received_solution = recv_solution(order);
    
    if (!received_solution)
    {
        fprintf(stderr, "[ERROR] collect_solutions: recv_solution failed\n");
        return;
    }
    
    // Copiar para solution_board
    for (row = 0; row < size; row++)
    {
        for (col = 0; col < size; col++)
        {
            solution_board[row][col] = received_solution[row][col];
        }
    }
    
    free_board(received_solution, size);
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
    
    DEBUG_LOG(rank, "Entering worker loop");
    
    while (1)
    {
        // Receber subproblema (COM POLLING para terminação)
        board = recv_subproblem(order);
        
        // Se NULL → terminação recebida
        if (!board)
        {
            DEBUG_LOG(rank, "Termination received");
            break;
        }
        
        DEBUG_LOG(rank, "Solving subproblem with solve_omp()");
        solve_omp(board, order);
        
        // Verificar se encontrou solução
        if (is_solved(board, order))
        {
            fprintf(stderr, "[RANK %d] Solution found\n", rank);
            send_solution(board, order);
            free_board(board, size);
            break;
        }
        else
        {
            DEBUG_LOG(rank, "No solution in this subproblem");
            free_board(board, size);
        }
    }
    
    DEBUG_LOG(rank, "Worker loop terminated");
}

/* ========================================================================
 * IMPLEMENTAÇÃO: UTILITY
 * ======================================================================== */

/**
 * print_usage - Imprime instruções de uso
 */
static void print_usage(void)
{
    fprintf(stderr, "\nUSAGE:\n");
    fprintf(stderr, "  mpirun -np <N> sudoku_mpi.exe <ficheiro_sudoku>\n\n");
    fprintf(stderr, "EXAMPLES:\n");
    fprintf(stderr, "  mpirun -np 4 sudoku_mpi.exe \"Sample instances/9x9.txt\"\n");
    fprintf(stderr, "  OMP_NUM_THREADS=2 mpirun -np 4 sudoku_mpi.exe \"Sample instances/16x16.txt\"\n\n");
}

/* ========================================================================
 * IMPLEMENTAÇÃO: MAIN
 * ======================================================================== */

/**
 * main - Entry point do programa híbrido MPI + OpenMP
 * 
 * Fluxo:
 *   Rank 0: Carrega Sudoku, gera subproblemas, distribui trabalho,
 *           resolve localmente, recolhe solução, envia terminação
 *   Workers: Recebem subproblemas, resolvem com solve_omp(),
 *            enviam solução se encontrada
 */
int main(int argc, char *argv[])
{
    int rank, nprocs;
    int provided;
    
    // Inicializar MPI com suporte para threads (MPI_THREAD_SERIALIZED)
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
    
    if (provided < MPI_THREAD_SERIALIZED)
    {
        fprintf(stderr, "[ERROR] Insufficient MPI thread support\n");
        fprintf(stderr, "  Required: MPI_THREAD_SERIALIZED, Provided: %d\n", provided);
        MPI_Finalize();
        return 1;
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    
    DEBUG_LOG(rank, "Started (processes: %d)", nprocs);
    
    configure_openmp(rank, nprocs);
    
    // Validar argumentos
    if (argc < 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "[ERROR] Missing Sudoku file argument\n");
            print_usage();
        }
        MPI_Finalize();
        return 1;
    }
    
    // Variáveis para distribuição MPI
    t_sudoku *sudoku = NULL;
    int ***subproblems = NULL;
    int subproblems_count = 0;
    int size = 0;
    t_benchmark *bench = NULL;
    int **rank0_board = NULL;
    int **solution_board = NULL;
    int solution_found = 0;
    
    // Inicializar benchmark (todos os ranks)
    if (rank == 0)
    {
        bench = benchmark_init(nprocs, omp_get_max_threads());
        if (!bench)
        {
            fprintf(stderr, "[ERROR] Failed to initialize benchmark\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
    
    // Sincronizar todos antes de iniciar medição
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Iniciar medição (apenas Rank 0 registra)
    if (rank == 0)
        benchmark_start(bench);
    
    if (rank == 0)
    {
        // Carregar Sudoku
        sudoku = load_sudoku(argv[1]);
        if (!sudoku)
        {
            fprintf(stderr, "[ERROR] Failed to load Sudoku from: %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        
        size = sudoku->order * sudoku->order;
        DEBUG_LOG(0, "Loaded Sudoku: order=%d, size=%d×%d", 
                  sudoku->order, size, size);
        
        // Gerar subproblemas
        int ret = generate_subproblems_mpi(sudoku->tab, sudoku->order,
                                          &subproblems, &subproblems_count);
        if (ret != 0 || subproblems_count == 0)
        {
            fprintf(stderr, "[ERROR] Failed to generate subproblems\n");
            free_sudoku(sudoku);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        
        // Broadcast parâmetros para todos os ranks
        MPI_Bcast(&sudoku->order, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&subproblems_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
        DEBUG_LOG(0, "Broadcast parameters: order=%d, subproblems=%d",
                  sudoku->order, subproblems_count);
        
        // Distribuir trabalho (Round-Robin)
        dispatch_work(subproblems, subproblems_count, sudoku->order, nprocs);
        
        // Resolver subproblema local (Rank 0)
        rank0_board = subproblems[0];
        DEBUG_LOG(0, "Solving local subproblem with solve_omp()");
        solve_omp(rank0_board, sudoku->order);
        
        // Verificar se Rank 0 encontrou solução
        if (is_solved(rank0_board, sudoku->order))
        {
            fprintf(stderr, "[RANK 0] Solution found locally\n");
            solution_board = alloc_board(size);
            if (solution_board)
            {
                copy_board(solution_board, rank0_board, size);
                solution_found = 1;
            }
        }
        else
        {
            DEBUG_LOG(0, "No solution locally, waiting for workers");
            
            solution_board = alloc_board(size);
            if (!solution_board)
            {
                fprintf(stderr, "[ERROR] Failed to allocate solution_board\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            
            collect_solutions(sudoku->order, nprocs, solution_board);
            solution_found = 1;
        }
        
        // Enviar terminação para todos os workers
        broadcast_termination(nprocs);
    }
    else
    {
        // Workers: Receber parâmetros e entrar no loop
        int order_recv;
        int count_recv;
        
        MPI_Bcast(&order_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&count_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        DEBUG_LOG(rank, "Received parameters: order=%d", order_recv);
        
        worker_loop(order_recv);
    }
    
    // Sincronizar todos após terminação
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Parar medição (apenas Rank 0 tem bench)
    if (rank == 0)
        benchmark_stop(bench);
    
    // Calcular métricas — operação colectiva: TODOS os ranks participam.
    // Workers passam bench = NULL; contribuem com local_elapsed = 0.0
    // para o MPI_Reduce e retornam imediatamente.
    benchmark_compute_metrics(bench);
    
    // Relatório, CSV e cleanup (apenas Rank 0)
    if (rank == 0)
    {
        // Imprimir relatório
        benchmark_print_report(bench, rank);
        
        // Salvar CSV
        benchmark_save_csv(bench, rank, "benchmark_results.csv");
        
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
        
        // Libertar memória
        free_sudoku(sudoku);
        for (int i = 0; i < subproblems_count; i++)
        {
            free_board(subproblems[i], size);
        }
        free(subproblems);
        if (solution_board)
            free_board(solution_board, size);
        benchmark_free(bench);
    }
    
    DEBUG_LOG(rank, "Finalizing MPI");
    MPI_Finalize();
    
    return 0;
}