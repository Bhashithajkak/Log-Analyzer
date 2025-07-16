#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <mpi.h>

#define CHUNK_INIT 4096
#define VERBOSE 0  // Set to 1 to print matched lines

static inline int count_keyword_occurrences(const char *line, const char *keyword) {
    return (keyword[0] && strstr(line, keyword)) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        if (rank == 0)
            fprintf(stderr, "Usage: %s <log_file> <keyword>\n", argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    const char *keyword = argv[2];

    char **lines = NULL;
    size_t nlines = 0;

    if (rank == 0) {
        FILE *file = fopen(filename, "r");
        if (!file) {
            perror("Error opening file");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        size_t cap = CHUNK_INIT;
        lines = malloc(cap * sizeof(char *));
        if (!lines) {
            perror("malloc failed");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        while ((read = getline(&line, &len, file)) != -1) {
            if (read && line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }

            if (nlines == cap) {
                cap *= 2;
                char **tmp = realloc(lines, cap * sizeof(*lines));
                if (!tmp) {
                    perror("realloc failed");
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                lines = tmp;
            }

            lines[nlines++] = strdup(line);
        }

        free(line);
        fclose(file);
    }

    // Broadcast line count
    MPI_Bcast(&nlines, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

    // Distribute line counts
    size_t lines_per_proc = nlines / size;
    size_t remainder = nlines % size;
    size_t my_share = lines_per_proc + (rank < remainder ? 1 : 0);

    char **my_lines = malloc(my_share * sizeof(char *));
    if (!my_lines) {
        perror("malloc failed");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    if (rank == 0) {
        size_t offset = 0;
        for (int i = 0; i < size; ++i) {
            size_t count = lines_per_proc + (i < remainder ? 1 : 0);
            for (size_t j = 0; j < count; ++j) {
                int len = strlen(lines[offset + j]);
                if (i == 0) {
                    my_lines[j] = strdup(lines[offset + j]);
                } else {
                    MPI_Send(&len, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    MPI_Send(lines[offset + j], len + 1, MPI_CHAR, i, 1, MPI_COMM_WORLD);
                }
            }
            offset += count;
        }
    } else {
        for (size_t i = 0; i < my_share; ++i) {
            int len;
            MPI_Recv(&len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            my_lines[i] = malloc(len + 1);
            if (!my_lines[i]) {
                perror("malloc failed");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            MPI_Recv(my_lines[i], len + 1, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    // Start timing (wall clock)
    double start_time = MPI_Wtime();

    // Set number of OpenMP threads
    omp_set_num_threads(omp_get_max_threads());

    // OpenMP parallel processing of local data
    long local_total = 0;

#pragma omp parallel for reduction(+:local_total) schedule(static)
    for (size_t i = 0; i < my_share; ++i) {
        if (count_keyword_occurrences(my_lines[i], keyword)) {
            local_total++;
#if VERBOSE
#pragma omp critical
            printf("[MATCH from rank %d] %s\n", rank, my_lines[i]);
#endif
        }
    }

    // Reduce counts from all processes
    long global_total = 0;
    MPI_Reduce(&local_total, &global_total, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double end_time = MPI_Wtime();

    if (rank == 0) {
        printf("The keyword '%s' appeared %ld times in '%s'\n", keyword, global_total, filename);
        printf("Execution time: %.6f seconds (with %d MPI processes and %d OpenMP threads per process)\n",
               end_time - start_time, size, omp_get_max_threads());
    }

    // Cleanup
    for (size_t i = 0; i < my_share; ++i) {
        free(my_lines[i]);
    }
    free(my_lines);

    if (rank == 0) {
        for (size_t i = 0; i < nlines; ++i) {
            free(lines[i]);
        }
        free(lines);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
