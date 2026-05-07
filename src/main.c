#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // ==========================================
    // 1. THE SENTINEL GATE: Dynamic Configuration
    // ==========================================
    // Expected Usage: ./forge-core <dataset> <delimiter> <expected_cols>
    if (argc != 4) {
        fprintf(stderr, "🛡️ [SENTINEL] Usage: %s <dataset_path> <delimiter> <expected_cols>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filepath = argv[1];
    char delimiter = argv[2][0];
    int expected_cols = atoi(argv[3]);

    // ==========================================
    // 2. THE METAL LAYER: Dynamic Ingestion
    // ==========================================
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("⛓️ [METAL] I/O Error: Failed to open target file");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("⛓️ [METAL] Kernel Error: Failed to fetch metadata");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t file_size = sb.st_size;
    if (file_size == 0) {
        printf("🛡️ [SYSTEM] File is empty. Execution terminated.\n");
        close(fd);
        return 0;
    }

    char *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("⛓️ [METAL] Memory Error: mmap failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("📡 [SYSTEM] Target: %s (%zu bytes)\n", filepath, file_size);
    printf("⚡ [SYSTEM] Zero-Copy cache initialized.\n");

    // ==========================================
    // 3. THE SHIELD LAYER: Structural Audit
    // ==========================================
    printf("⚙️ [SHIELD] Scanning | Delimiter: '%c' | Target: %d cols\n", delimiter, expected_cols);

    size_t row_count = 0;
    int current_row_cols = 1; // Minimum 1 column per row
    size_t malformed_rows = 0;

    for (size_t i = 0; i < file_size; i++) {
        if (map[i] == delimiter) {
            current_row_cols++;
        } else if (map[i] == '\n') {
            row_count++;
            
            // Integrity Check: Compare actual vs expected
            if (current_row_cols != expected_cols) {
                malformed_rows++;
            }
            current_row_cols = 1; // Reset for next line
        }
    }

    // Handle trailing data without a newline
    if (file_size > 0 && map[file_size - 1] != '\n') {
        row_count++;
        if (current_row_cols != expected_cols) {
            malformed_rows++;
        }
    }

    printf("📊 [SHIELD] Audit Complete.\n");
    printf("   >> Total Rows Processed: %zu\n", row_count);
    printf("   >> Integrity Breaches: %zu malformed rows detected\n", malformed_rows);

    // ==========================================
    // 4. THE CLEANUP
    // ==========================================
    munmap(map, file_size);
    close(fd);

    printf("🛡️ [SYSTEM] Execution terminated safely.\n");
    return 0;
}
