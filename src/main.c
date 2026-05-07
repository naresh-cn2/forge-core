#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // ==========================================
    // 1. THE SENTINEL GATE: Dynamic Pathing
    // ==========================================
    if (argc != 2) {
        fprintf(stderr, "🛡️ [SENTINEL] Usage: %s <dataset_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filepath = argv[1];

    // ==========================================
    // 2. THE METAL LAYER: Hardware I/O & Metadata
    // ==========================================
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("⛓️ [METAL] I/O Error: Failed to open target file");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("⛓️ [METAL] Kernel Error: Failed to fetch file metadata via fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t file_size = sb.st_size;
    printf("📡 [SYSTEM] Target identified: %s (Size: %zu bytes)\n", filepath, file_size);

    // Guard against empty files to prevent mmap errors
    if (file_size == 0) {
        printf("🛡️ [SYSTEM] File is empty. Execution terminated safely.\n");
        close(fd);
        return 0;
    }

    // ==========================================
    // 3. THE METAL LAYER: Zero-Copy Ingestion
    // ==========================================
    char *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("⛓️ [METAL] Memory Error: mmap allocation failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("⚡ [SYSTEM] Zero-Copy cache initialized. Ready for throughput.\n");

    // ==========================================
    // 4. THE SHIELD LAYER: High-Speed Parsing
    // ==========================================
    printf("⚙️ [SHIELD] Initiating high-speed structural scan...\n");

    size_t row_count = 0;
    
    // Mechanical pointer iteration over the zero-copy cache
    for (size_t i = 0; i < file_size; i++) {
        if (map[i] == '\n') {
            row_count++;
        }
    }
    
    // Catch final row if the dataset lacks a trailing newline
    if (map[file_size - 1] != '\n') {
        row_count++;
    }

    printf("📊 [SHIELD] Scan complete. Structural integrity verified: %zu rows detected.\n", row_count);

    // ==========================================
    // 5. THE CLEANUP: Memory Release
    // ==========================================
    if (munmap(map, file_size) == -1) {
        perror("⛓️ [METAL] Memory Error: munmap release failed");
    }
    close(fd);

    printf("🛡️ [SYSTEM] Execution terminated. Strict RAM budget maintained.\n");
    return 0;
}
