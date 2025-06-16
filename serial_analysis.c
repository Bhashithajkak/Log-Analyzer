
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // To get the execution time

#define MAX_LINE_LENGTH 8192  // Large buffer for performance

int count_keyword_occurrences(const char *line, const char *keyword) {
    return (keyword[0] && strstr(line, keyword)) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <log_file> <keyword>\n", argv[0]);
        return EXIT_FAILURE;
    }

    clock_t start_time = clock();
    const char *filename = argv[1];
    const char *keyword = argv[2];
    FILE *file;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int total_count = 0;

    file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        total_count += count_keyword_occurrences(line, keyword);
    }

    fclose(file);
    free(line);
    clock_t end_time = clock();
    printf("The keyword '%s' appeared %d times in '%s'\n", keyword, total_count, filename);

    double time_taken = ((double)(end_time-start_time))/CLOCKS_PER_SEC;
    printf("Execution time : %.6f seconds\n", time_taken);

    return EXIT_SUCCESS;
}
