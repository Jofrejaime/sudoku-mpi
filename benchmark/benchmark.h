#ifndef BENCHMARK_H
# define BENCHMARK_H

# include <mpi.h>

/* ========================================================================
 * BENCHMARK MODULE - MPI+OpenMP Hybrid Performance Measurement
 *
 * Mede tempo de execução paralela via MPI_Reduce(MAX).
 * Speedup e eficiência são calculados em pós-processamento a partir do CSV.
 * ======================================================================== */

typedef struct s_benchmark
{
    double start_time;
    double end_time;
    double elapsed_time;  /* Tempo do processo mais lento (MPI_Reduce MAX) */

    int    mpi_processes;
    int    omp_threads;
    int    total_workers;
}   t_benchmark;

/* ========================================================================
 * FUNÇÕES PÚBLICAS
 * ======================================================================== */

/** Inicializa estrutura de benchmark. */
t_benchmark *benchmark_init(int mpi_processes, int omp_threads);

/** Inicia medição de tempo (apenas Rank 0). */
void benchmark_start(t_benchmark *bench);

/** Para medição de tempo (apenas Rank 0). */
void benchmark_stop(t_benchmark *bench);

/**
 * Calcula elapsed_time = max(tempo de todos os processos) via MPI_Reduce.
 * Operação colectiva — TODOS os processos devem chamar esta função.
 */
void benchmark_compute_metrics(t_benchmark *bench);

/** Imprime relatório formatado (apenas Rank 0). */
void benchmark_print_report(const t_benchmark *bench, int rank);

/**
 * Salva resultados em CSV (apenas Rank 0, modo append).
 * Colunas: mpi,omp,total_workers,time
 */
void benchmark_save_csv(const t_benchmark *bench, int rank, const char *filename);

/** Liberta recursos do benchmark. */
void benchmark_free(t_benchmark *bench);

#endif /* BENCHMARK_H */