#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "benchmark.h"

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_init
 * ======================================================================== */
t_benchmark *benchmark_init(int mpi_processes, int omp_threads)
{
    t_benchmark *bench;

    bench = malloc(sizeof(t_benchmark));
    if (!bench)
        return NULL;

    bench->start_time    = 0.0;
    bench->end_time      = 0.0;
    bench->elapsed_time  = 0.0;

    bench->mpi_processes = mpi_processes;
    bench->omp_threads   = omp_threads;
    bench->total_workers = mpi_processes * omp_threads;

    return bench;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_start
 *
 * Regista t0 no Rank 0.
 * A sincronização (MPI_Barrier) é feita no main() antes desta chamada.
 * ======================================================================== */
void benchmark_start(t_benchmark *bench)
{
    if (!bench)
        return;
    bench->start_time = MPI_Wtime();
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_stop
 *
 * Regista tf no Rank 0.
 * A sincronização (MPI_Barrier) é feita no main() antes desta chamada.
 * ======================================================================== */
void benchmark_stop(t_benchmark *bench)
{
    if (!bench)
        return;
    bench->end_time = MPI_Wtime();
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_compute_metrics
 *
 * elapsed_time = max(tempo de todos os processos) via MPI_Reduce(MPI_MAX).
 *
 * Operação colectiva — TODOS os processos devem chamar esta função.
 * Workers passam bench = NULL e contribuem com local_elapsed = 0.0.
 * ======================================================================== */
void benchmark_compute_metrics(t_benchmark *bench)
{
    double local_elapsed;
    double global_elapsed;
    int    rank;

    local_elapsed = 0.0;
    if (bench)
        local_elapsed = bench->end_time - bench->start_time;

    MPI_Reduce(&local_elapsed, &global_elapsed, 1, MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);

    if (!bench)
        return;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0)
        return;

    bench->elapsed_time = global_elapsed;
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_print_report
 * ======================================================================== */
void benchmark_print_report(const t_benchmark *bench, int rank)
{
    if (!bench || rank != 0)
        return;

    fprintf(stderr, "\n");
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "MPI+OMP BENCHMARK\n");
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "MPI Processes : %d\n",     bench->mpi_processes);
    fprintf(stderr, "OMP Threads   : %d\n",     bench->omp_threads);
    fprintf(stderr, "Total Workers : %d\n",     bench->total_workers);
    fprintf(stderr, "------------------------------------\n");
    fprintf(stderr, "Exec Time     : %.6f s\n", bench->elapsed_time);
    fprintf(stderr, "====================================\n");
    fprintf(stderr, "\n");
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_save_csv
 *
 * Colunas: mpi,omp,total_workers,time
 * Speedup e eficiência calculados em pós-processamento.
 * ======================================================================== */
void benchmark_save_csv(const t_benchmark *bench, int rank, const char *filename)
{
    FILE *file;
    int   file_exists;

    if (!bench || rank != 0 || !filename)
        return;

    file_exists = 0;
    file = fopen(filename, "r");
    if (file)
    {
        file_exists = 1;
        fclose(file);
    }

    file = fopen(filename, "a");
    if (!file)
    {
        fprintf(stderr, "[WARNING] Could not open CSV file: %s\n", filename);
        return;
    }

    if (!file_exists)
        fprintf(file, "mpi,omp,total_workers,time\n");

    fprintf(file, "%d,%d,%d,%.6f\n",
            bench->mpi_processes,
            bench->omp_threads,
            bench->total_workers,
            bench->elapsed_time);

    fclose(file);
}

/* ========================================================================
 * IMPLEMENTAÇÃO: benchmark_free
 * ======================================================================== */
void benchmark_free(t_benchmark *bench)
{
    if (bench)
        free(bench);
}