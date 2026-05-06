#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

void log_sentinel_error(size_t row, int expected, int found) {
    FILE *log_file = fopen("logs/sentinel_manifest.log", "a");
    if (log_file) {
        fprintf(log_file, "ROW_FAILURE: Row %zu | Expected %d cols | Found %d\n", row, expected, found);
        fclose(log_file);
    }
}

int main() {
    const char *filepath = "data/target_20gb.bin";
    system("mkdir -p logs");

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) { perror("File Error"); return 1; }

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { perror("mmap Error"); return 1; }

    printf("SENTINEL ACTIVE: Validating CSV Structure across 20GB...\n");

    clock_t start = clock();
    int target_cols = 0;
    int current_cols = 0;
    size_t row_count = 0;
    size_t malformed_rows = 0;

    // 1. Establish Schema (First Row)
    size_t i = 0;
    while (i < filesize && data[i] != '\n') {
        if (data[i] == ',') target_cols++;
        i++;
    }
    target_cols++; // Columns = commas + 1
    printf("SCHEMA DETECTED: Expected %d columns per row.\n", target_cols);

    // 2. The Sentinel Scan
    for (; i < filesize; i++) {
        if (data[i] == ',') {
            current_cols++;
        } else if (data[i] == '\n') {
            current_cols++; 
            row_count++;
            if (current_cols != target_cols) {
                malformed_rows++;
                // Limit logging to prevent disk I/O from choking the benchmark
                if (malformed_rows < 1000) {
                    log_sentinel_error(row_count, target_cols, current_cols);
                }
            }
            current_cols = 0;
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n--- SENTINEL REPORT ---\n");
    printf("Rows Processed: %zu\n", row_count);
    printf("Integrity: %zu malformed rows detected.\n", malformed_rows);
    printf("Throughput: %.2f MB/s\n", (filesize / 1024.0 / 1024.0) / time_spent);

    munmap(data, filesize);
    close(fd);
    return 0;
}