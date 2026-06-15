#include "sudoku.h"

static char *grow_line(char *line, int *capacity)
{
    char    *new_line;
    int     new_capacity;

    new_capacity = (*capacity) * 2;
    new_line = malloc(new_capacity);
    if (!new_line)
    {
        free(line);
        return (NULL);
    }
    memcpy(new_line, line, *capacity);
    free(line);
    *capacity = new_capacity;
    return (new_line);
}

char *get_next_line(int fd)
{
    static char buffer[BUFFER_SIZE];
    static int index = 0, bytes = 0;
    char *line;
    int i;
    int capacity;

    i = 0;
    capacity = BUFFER_SIZE + 1;
    if (fd < 0 || !(line = malloc(capacity)))
        return (NULL);
    while (1)
    {
        if (index >= bytes)
        {
            bytes = read(fd, buffer, BUFFER_SIZE);
            index = 0;
            if (bytes <= 0)
                break ;
        }
        if (i + 1 >= capacity)
        {
            line = grow_line(line, &capacity);
            if (!line)
                return (NULL);
        }
        line[i++] = buffer[index];
        if(buffer[index++] == '\n')
            break ;
    }
    if (i == 0)
        return (free(line), NULL);
    line[i] = '\0';
    return (line);
}