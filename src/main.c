#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "forge.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct stat sb;
    fstat(fd, &sb);
    char *map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }

    // Configure Schema: Col 0 (INT), Col 1 (STR), Col 2 (INT)
    ForgeSchema schema = {
        .delimiter = ',',
        .expected_cols = 3,
        .validation_mask = (1 << 0) | (1 << 2) 
    };

    int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    ForgeThreadTask *tasks = malloc(sizeof(ForgeThreadTask) * num_threads);
    size_t chunk_size = sb.st_size / num_threads;

    printf("\n🚀 [FORGE-CORE v3.3 | TYPED TELEMETRY ENGINE]\n");
    printf("Hardware: AVX2-SIMD | Columns: %d | Mask: 0x%X\n", 
            schema.expected_cols, schema.validation_mask);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_threads; i++) {
        tasks[i].thread_id = i;
        tasks[i].map_base = map;
        tasks[i].start_offset = i * chunk_size;
        tasks[i].end_offset = (i == num_threads - 1) ? sb.st_size : (i + 1) * chunk_size;
        tasks[i].schema = &schema;
        tasks[i].valid_rows = 0;
        memset(tasks[i].column_errors, 0, sizeof(size_t) * 32);

        pthread_create(&threads[i], NULL, forge_worker, &tasks[i]);
    }

    size_t total_v = 0;
    size_t col_totals[32] = {0};
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_v += tasks[i].valid_rows;
        for (int c = 0; c < 32; c++) col_totals[c] += tasks[i].column_errors[c];
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n--- TELEMETRY AUDIT REPORT ---\n");
    printf("✅ Valid Rows:  %zu\n", total_v);
    printf("📊 Throughput:  %.2f M rows/sec\n", (total_v / elapsed) / 1e6);
    printf("⏱️  Audit Time:  %.4fs\n", elapsed);

    printf("\n--- COLUMN INTEGRITY AUDIT ---\n");
    for (int c = 0; c < schema.expected_cols; c++) {
        const char *type = (schema.validation_mask >> c) & 1 ? "INT" : "STR";
        printf("Col %-2d [%-3s]: %-12zu errors %s\n", 
               c, type, col_totals[c], col_totals[c] > 0 ? "⚠️" : "✅");
    }
    printf("------------------------------\n\n");

    munmap(map, sb.st_size);
    close(fd);
    free(threads);
    free(tasks);
    return 0;
}
