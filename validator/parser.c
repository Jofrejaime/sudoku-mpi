#include "sudoku.h"
#include <limits.h>

static int is_space_char(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\v' || c == '\f');
}

static int parse_positive_int(const char *str, int *value, const char **endptr)
{
    long result;

    if (!str || !value)
        return (0);
    while (is_space_char(*str))
        str++;
    if (*str < '0' || *str > '9')
        return (0);
    result = 0;
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        if (result > INT_MAX)
            return (0);
        str++;
    }
    if (result <= 0)
        return (0);
    *value = (int)result;
    if (endptr)
        *endptr = str;
    return (1);
}

static int parse_non_negative_int(const char *str, int *value, const char **endptr)
{
    long result;

    if (!str || !value)
        return (0);
    if (*str < '0' || *str > '9')
        return (0);
    result = 0;
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        if (result > INT_MAX)
            return (0);
        str++;
    }
    *value = (int)result;
    if (endptr)
        *endptr = str;
    return (1);
}

int parser_parse_order_line(const char *line, int *order)
{
    const char *end;

    if (!line || !parse_positive_int(line, order, &end))
        return (0);
    while (*end && is_space_char(*end))
        end++;
    return (*end == '\0');
}

int parser_compute_grid_size(int order, int *size)
{
    if (order <= 0 || order > INT_MAX / order)
        return (0);
    *size = order * order;
    return (1);
}

int parser_parse_board_line(const char *line, int *row, int size)
{
    const char  *p;
    int         col;
    int         value;

    if (!line || !row || size <= 0)
        return (0);
    p = line;
    col = 0;
    while (col < size)
    {
        while (*p && is_space_char(*p))
            p++;
        if (!parse_non_negative_int(p, &value, &p) || value > size)
            return (0);
        row[col] = value;
        col++;
    }
    while (*p && is_space_char(*p))
        p++;
    return (*p == '\0');
}
