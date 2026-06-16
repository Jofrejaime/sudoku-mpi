#include "sudoku.h"

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
            while (i > 0)
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

static int read_header(int fd, int *order, int *size)
{
    char *line;

    line = get_next_line(fd);
    if (!line)
        return (0);
    if (!parser_parse_order_line(line, order)
        || !parser_compute_grid_size(*order, size))
    {
        free(line);
        return (0);
    }
    free(line);
    return (1);
}

static int load_rows(int fd, int **tab, int size)
{
    char    *line;
    int     i;

    i = 0;
    while (i < size)
    {
        line = get_next_line(fd);
        if (!line || !parser_parse_board_line(line, tab[i], size))
        {
            free(line);
            return (0);
        }
        free(line);
        i++;
    }
    line = get_next_line(fd);
    if (line)
    {
        free(line);
        return (0);
    }
    return (1);
}

static t_sudoku *build_sudoku(int **tab, int order)
{
    t_sudoku *sudoku;

    sudoku = malloc(sizeof(t_sudoku));
    if (!sudoku)
        return (NULL);
    sudoku->tab = tab;
    sudoku->order = order;
    return (sudoku);
}

t_sudoku *load_sudoku(const char *path)
{
    int         fd;
    int         order;
    int         size;
    int         **tab;
    t_sudoku    *sudoku;

    if (!path)
        return (NULL);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return (NULL);
    if (!read_header(fd, &order, &size))
    {
        close(fd);
        return (NULL);
    }
    tab = alloc_grid(size);
    if (!tab)
    {
        close(fd);
        return (NULL);
    }
    if (!load_rows(fd, tab, size))
    {
        close(fd);
        free_tab(tab, size);
        return (NULL);
    }
    sudoku = build_sudoku(tab, order);
    if (!sudoku)
    {
        free_tab(tab, size);
        close(fd);
        return (NULL);
    }
    close(fd);
    return (sudoku);
}