#include "sudoku.h"

static int  board_size(int order)
{
    return (order * order);
}

static unsigned long long full_mask(int size)
{
    if (size >= 64)
        return (~0ULL);
    return ((1ULL << size) - 1ULL);
}

static int  box_index(int order, int row, int col)
{
    return ((row / order) * order + (col / order));
}

static int count_bits(unsigned long long mask)
{
    int count;

    count = 0;
    while (mask)
    {
        if (mask & 1ULL)
            count++;
        mask >>= 1;
    }
    return (count);
}

static int init_masks(int **tb, int order, unsigned long long *row_used,
    unsigned long long *col_used, unsigned long long *box_used)
{
    int size;
    int r;
    int c;
    int val;
    int box;
    unsigned long long bit;

    size = board_size(order);
    r = 0;
    while (r < size)
    {
        row_used[r] = 0;
        col_used[r] = 0;
        box_used[r] = 0;
        r++;
    }
    r = 0;
    while (r < size)
    {
        c = 0;
        while (c < size)
        {
            val = tb[r][c];
            if (val < 0 || val > size)
                return (0);
            if (val != 0)
            {
                bit = (1ULL << (val - 1));
                box = box_index(order, r, c);
                if ((row_used[r] & bit) || (col_used[c] & bit)
                    || (box_used[box] & bit))
                    return (0);
                row_used[r] |= bit;
                col_used[c] |= bit;
                box_used[box] |= bit;
            }
            c++;
        }
        r++;
    }
    return (1);
}

static unsigned long long possible_mask(int order, int row, int col,
    unsigned long long *row_used, unsigned long long *col_used,
    unsigned long long *box_used)
{
    int                 box;
    unsigned long long  used;

    box = box_index(order, row, col);
    used = row_used[row] | col_used[col] | box_used[box];
    return (~used & full_mask(board_size(order)));
}

static int find_best_cell(int **tb, int order, unsigned long long *row_used,
    unsigned long long *col_used, unsigned long long *box_used,
    int *best_row, int *best_col, unsigned long long *best_mask)
{
    int                 size;
    int                 min_options;
    int                 row;
    int                 col;
    unsigned long long  possible;
    int                 count;

    size = board_size(order);
    min_options = size + 1;
    row = 0;
    while (row < size)
    {
        col = 0;
        while (col < size)
        {
            if (tb[row][col] == 0)
            {
                possible = possible_mask(order, row, col,
                        row_used, col_used, box_used);
                count = count_bits(possible);
                if (count == 0)
                    return (-1);
                if (count < min_options)
                {
                    min_options = count;
                    *best_row = row;
                    *best_col = col;
                    *best_mask = possible;
                }
            }
            col++;
        }
        row++;
    }
    if (min_options == size + 1)
        return (0);
    return (1);
}

static int backtrack(int **tb, int order, unsigned long long *row_used,
    unsigned long long *col_used, unsigned long long *box_used)
{
    int                 row;
    int                 col;
    int                 status;
    int                 box;
    int                 val;
    int                 size;
    unsigned long long  possible;
    unsigned long long  bit;

    row = 0;
    col = 0;
    possible = 0;
    status = find_best_cell(tb, order, row_used, col_used, box_used,
            &row, &col, &possible);
    if (status == 0)
        return (1);
    if (status == -1)
        return (0);
    box = box_index(order, row, col);
    size = board_size(order);
    val = 1;
    while (val <= size)
    {
        bit = (1ULL << (val - 1));
        if (possible & bit)
        {
            tb[row][col] = val;
            row_used[row] |= bit;
            col_used[col] |= bit;
            box_used[box] |= bit;
            if (backtrack(tb, order, row_used, col_used, box_used))
                return (1);
            row_used[row] ^= bit;
            col_used[col] ^= bit;
            box_used[box] ^= bit;
            tb[row][col] = 0;
        }
        val++;
    }
    return (0);
}

void    solve(int **tb, int order)
{
    unsigned long long  row_used[63];
    unsigned long long  col_used[63];
    unsigned long long  box_used[63];

    if (board_size(order) >= 64)
    {
        printf("Order too large to solve\n");
        return ;
    }
    if (!init_masks(tb, order, row_used, col_used, box_used))
    {
        printf("Invalid initial board\n");
        return ;
    }
    if (!backtrack(tb, order, row_used, col_used, box_used))
        printf("Nenhuma solução\n");
}
