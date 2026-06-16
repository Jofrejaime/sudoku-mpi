#ifndef SUDOKU_H
# define SUDOKU_H
# define BUFFER_SIZE 42

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <omp.h>

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "sudoku.h"
#include "benchmark/benchmark.h"



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


typedef struct s_sudoku
{
    int **tab;
    int order;
}   t_sudoku;


char *get_next_line(int fd);
t_sudoku    *load_sudoku(const char *path);
void    free_sudoku(t_sudoku *sudoku);
int     **alloc_board(int size);
void    free_board(int **board, int size);
void    copy_board(int **dest, int **src, int size);
int     is_solved(int **tb, int order);
int     parser_parse_order_line(const char *line, int *order);
int     parser_compute_grid_size(int order, int *size);
int     parser_parse_board_line(const char *line, int *row, int size);
void    print(int **tb, int order);
void    solve(int **tb, int order);
void    solve_omp(int **tb, int order);

/* ========================================================================
 * PROTÓTIPOS DE FUNÇÕES MPI
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

#endif