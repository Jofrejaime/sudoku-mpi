#include "sudoku.h"

void    print(int **tb, int order)
{
    int i = 0, j = 0;

    while(i < order * order)
    {
        j = 0;
        while(j < order * order)
        {
            printf("%d ", tb[i][j]);
            j++;
        }
        printf("\n");
        i++;
    }
}

void free_sudoku(t_sudoku *sudoku)
{
    int i;
    int size;

    if (!sudoku)
        return ;
    if (sudoku->tab)
    {
        size = sudoku->order * sudoku->order;
        i = 0;
        while (i < size)
        {
            free(sudoku->tab[i]);
            i++;
        }
        free(sudoku->tab);
    }
    free(sudoku);
}

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

int is_solved(int **tb, int order)
{
    int size = order * order;
    int i, j, box_row, box_col;
    unsigned long long seen, bit;

    if (!tb || order <= 0 || order > 62)
        return (0);
    
    // Verificação 1: Nenhuma célula é 0 e todas estão em range [1, size]
    i = 0;
    while (i < size)
    {
        j = 0;
        while (j < size)
        {
            if (tb[i][j] == 0 || tb[i][j] < 1 || tb[i][j] > size)
                return (0);
            j++;
        }
        i++;
    }
    
    // Verificação 2: Cada número 1..size aparece exatamente uma vez por linha
    i = 0;
    while (i < size)
    {
        seen = 0;
        j = 0;
        while (j < size)
        {
            bit = (1ULL << (tb[i][j] - 1));
            if (seen & bit)
                return (0);
            seen |= bit;
            j++;
        }
        i++;
    }
    
    // Verificação 3: Cada número 1..size aparece exatamente uma vez por coluna
    j = 0;
    while (j < size)
    {
        seen = 0;
        i = 0;
        while (i < size)
        {
            bit = (1ULL << (tb[i][j] - 1));
            if (seen & bit)
                return (0);
            seen |= bit;
            i++;
        }
        j++;
    }
    
    // Verificação 4: Cada número 1..size aparece exatamente uma vez por box
    box_row = 0;
    while (box_row < order)
    {
        box_col = 0;
        while (box_col < order)
        {
            seen = 0;
            i = 0;
            while (i < order)
            {
                j = 0;
                while (j < order)
                {
                    int row = box_row * order + i;
                    int col = box_col * order + j;
                    bit = (1ULL << (tb[row][col] - 1));
                    if (seen & bit)
                        return (0);
                    seen |= bit;
                    j++;
                }
                i++;
            }
            box_col++;
        }
        box_row++;
    }
    
    return (1);
}
