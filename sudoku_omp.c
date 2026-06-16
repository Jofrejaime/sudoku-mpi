
#include "sudoku.h"

static void print_usage(void)
{
    fprintf(stderr, "USAGE:\n");
    fprintf(stderr, "  ./sudoku_omp <ficheiro_sudoku>\n");
    fprintf(stderr, "\nEXAMPLES:\n");
    fprintf(stderr, "  ./sudoku_omp maps/map1.txt\n");
    fprintf(stderr, "  ./sudoku_omp Sample\\ instances/9x9.txt\n");
    fprintf(stderr, "\nFICHEIROS DISPONÍVEIS:\n");
    fprintf(stderr, "  maps/map1.txt, maps/map2.txt, maps/map3.txt, maps/map4.txt, maps/map5.txt\n");
    fprintf(stderr, "  Sample instances/9x9.txt, Sample instances/16x16.txt\n");
}

int main(int argc, char *argv[])
{
    t_sudoku *sudoku;
    double exec_time;

    if (argc != 2)
    {
        print_usage();
        return (1);
    }
    sudoku = load_sudoku(argv[1]);
    if (!sudoku)
    {
        fprintf(stderr, "erro: ficheiro invalido ou nao encontrado: %s\n", argv[1]);
        print_usage();
        return (1);
    }
    exec_time = -omp_get_wtime();
    solve_omp(sudoku->tab, sudoku->order);
    exec_time += omp_get_wtime();
    print(sudoku->tab, sudoku->order);
    fprintf(stderr, "%.10f\n", exec_time);
    free_sudoku(sudoku);
    return (0);
}
