#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define CHUNK_INIT 4096       // Initial capacity for lines

static inline int count_keyword_occurrences(const char *line, const char *keyword)
{
    return (keyword[0] && strstr(line, keyword)) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <log_file> <keyword>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    const char *keyword = argv[2];

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Read lines from file
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    size_t cap = CHUNK_INIT;
    size_t nlines = 0;
    char **lines = malloc(cap * sizeof(*lines));
    if (!lines) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        if (read && line[read - 1] == '\n') {
            line[read - 1] = '\0'; // strip newline
        }

        if (nlines == cap) {
            cap *= 2;
            char **tmp = realloc(lines, cap * sizeof(*lines));
            if (!tmp) {
                perror("realloc failed");
                return EXIT_FAILURE;
            }
            lines = tmp;
        }

        lines[nlines++] = strdup(line); // store copy
    }

    free(line);
    fclose(file);

    // Parallel processing
    double start_time = omp_get_wtime();

    long total = 0;
#pragma omp parallel for reduction(+:total) schedule(static)
    for (size_t i = 0; i < nlines; ++i) {
        total += count_keyword_occurrences(lines[i], keyword);
    }

    double end_time = omp_get_wtime();

    // Cleanup
    for (size_t i = 0; i < nlines; ++i) {
        free(lines[i]);
    }
    free(lines);

    // Output
    printf("The keyword '%s' appeared %ld times in '%s'\n", keyword, total, filename);
    printf("Execution time: %.6f seconds (with %d threads)\n",
           end_time - start_time, omp_get_max_threads());

    return EXIT_SUCCESS;
}
