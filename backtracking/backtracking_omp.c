#include "sudoku.h"

static int board_size(int order)
{
    return (order * order);
}

static unsigned long long full_mask(int size)
{
    if (size >= 64)
        return (~0ULL);
    return ((1ULL << size) - 1ULL);
}

static int box_index(int order, int row, int col)
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
    int *best_row, int *best_col, unsigned long long *best_mask, int *best_count)
{
    int                 size;
    int                 min_options;
    int                 row;
    int                 col;
    unsigned long long  possible;
    int                 count;

    size = board_size(order);
    min_options = size + 1;
    *best_count = 0;
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
                    *best_count = count;
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

// Forward declaration
static int backtrack_sequential(int **tb, int order, unsigned long long *row_used,
    unsigned long long *col_used, unsigned long long *box_used);

static int backtrack_parallel(int **tb, int order, unsigned long long *row_used,
    unsigned long long *col_used, unsigned long long *box_used,
    int level, int *found_solution)
{
    int                 row;
    int                 col;
    int                 status;
    int                 box;
    int                 val;
    int                 size;
    unsigned long long  possible;
    unsigned long long  bit;
    int                 num_options;

    row = 0;
    col = 0;
    possible = 0;
    status = find_best_cell(tb, order, row_used, col_used, box_used,
            &row, &col, &possible, &num_options);
    
    if (status == 0)
        return (1);
    if (status == -1)
        return (0);
    
    box = box_index(order, row, col);
    size = board_size(order);

    // DECISION: Parallelizar ou Sequencial?
    // Adaptativo: level 0 if 2+ options, level N if 3+ options
    int should_parallelize = 0;
    if (level == 0 && num_options >= 2)
        should_parallelize = 1;
    else if (level > 0 && num_options >= 3)
        should_parallelize = 1;

    if (should_parallelize && !*found_solution)
    {
        // ==========================================
        // PARALELO: Diferentes threads exploram ramos diferentes
        // ==========================================
        #pragma omp parallel for shared(found_solution)
        for (int v = 1; v <= size; v++)
        {
            if (*found_solution)
                continue;
            
            bit = (1ULL << (v - 1));
            if (possible & bit)
            {
                // Cada thread cria suas próprias cópias
                int **tb_copy = malloc(sizeof(int *) * size);
                unsigned long long *row_copy = malloc(sizeof(unsigned long long) * size);
                unsigned long long *col_copy = malloc(sizeof(unsigned long long) * size);
                unsigned long long *box_copy = malloc(sizeof(unsigned long long) * size);

                if (!tb_copy || !row_copy || !col_copy || !box_copy)
                {
                    free(tb_copy);
                    free(row_copy);
                    free(col_copy);
                    free(box_copy);
                    continue;
                }

                // Aloca e copia tb
                int i = 0;
                while (i < size)
                {
                    tb_copy[i] = malloc(sizeof(int) * size);
                    if (!tb_copy[i])
                    {
                        while (i > 0)
                        {
                            i--;
                            free(tb_copy[i]);
                        }
                        free(tb_copy);
                        free(row_copy);
                        free(col_copy);
                        free(box_copy);
                        continue;
                    }
                    i++;
                }

                // Memcpy para copies
                i = 0;
                while (i < size)
                {
                    int j = 0;
                    while (j < size)
                    {
                        tb_copy[i][j] = tb[i][j];
                        j++;
                    }
                    i++;
                }

                i = 0;
                while (i < size)
                {
                    row_copy[i] = row_used[i];
                    col_copy[i] = col_used[i];
                    box_copy[i] = box_used[i];
                    i++;
                }

                // Tenta este valor
                tb_copy[row][col] = v;
                row_copy[row] |= bit;
                col_copy[col] |= bit;
                box_copy[box_index(order, row, col)] |= bit;

                // Backtrack SEQUENCIAL a partir daqui
                if (backtrack_sequential(tb_copy, order, row_copy, col_copy, box_copy))
                {
                    #pragma omp critical
                    {
                        if (!*found_solution)
                        {
                            *found_solution = 1;
                            // Copia resultado de volta
                            int ii = 0;
                            while (ii < size)
                            {
                                int jj = 0;
                                while (jj < size)
                                {
                                    tb[ii][jj] = tb_copy[ii][jj];
                                    jj++;
                                }
                                ii++;
                            }
                            ii = 0;
                            while (ii < size)
                            {
                                row_used[ii] = row_copy[ii];
                                col_used[ii] = col_copy[ii];
                                box_used[ii] = box_copy[ii];
                                ii++;
                            }
                        }
                    }
                }

                // Liberta memória
                i = 0;
                while (i < size)
                {
                    free(tb_copy[i]);
                    i++;
                }
                free(tb_copy);
                free(row_copy);
                free(col_copy);
                free(box_copy);
            }
        }
        return *found_solution;
    }

    // ==========================================
    // SEQUENCIAL: Backtrack normal
    // ==========================================
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

            if (backtrack_parallel(tb, order, row_used, col_used, box_used, level + 1, found_solution))
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

static int backtrack_sequential(int **tb, int order, unsigned long long *row_used,
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
            &row, &col, &possible, &val);
    
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

            if (backtrack_sequential(tb, order, row_used, col_used, box_used))
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

void solve_omp(int **tb, int order)
{
    unsigned long long  row_used[63];
    unsigned long long  col_used[63];
    unsigned long long  box_used[63];
    int found_solution;

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
    
    found_solution = 0;
    if (!backtrack_parallel(tb, order, row_used, col_used, box_used, 0, &found_solution))
        printf("Nenhuma solução\n");
}
