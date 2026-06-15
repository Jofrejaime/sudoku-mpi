#ifndef SUDOKU_H
# define SUDOKU_H
# define BUFFER_SIZE 42

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <omp.h>

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
#endif