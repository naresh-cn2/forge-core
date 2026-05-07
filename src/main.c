#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// 1. ARCHITECTURAL DEFINITIONS
typedef enum { FORGE_INT, FORGE_FLOAT, FORGE_STRING } ForgeType;
typedef enum { VALID, ERR_NULL, ERR_TYPE } ValidationResult;
typedef enum { FORGE_ALL, FORGE_STRICT, FORGE_QUARANTINE } ForgePolicy;

typedef struct {
    struct timespec start_total, end_total;
    struct timespec start_ingest, end_ingest;
    struct timespec start_logic, end_logic;
    struct timespec start_scribe, end_scribe;
} ForgeTelemetry;

typedef struct { 
    char delimiter; int expected_cols; int header; 
    ForgeType types[16]; int required[16]; ForgePolicy policy;
} ForgeConfig;

typedef struct { char *data; size_t length; int is_sanitized; } ForgeToken;

typedef struct { 
    ForgeToken *tokens; char *string_pool; 
    size_t pool_offset; size_t count; size_t capacity; 
} ForgeArena;

// 2. HELPER FUNCTIONS
double diff_ms(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}

ForgeArena* arena_init(size_t max_tokens, size_t pool_size) {
    ForgeArena *arena = malloc(sizeof(ForgeArena));
    arena->tokens = malloc(sizeof(ForgeToken) * max_tokens);
    arena->string_pool = malloc(pool_size);
    arena->pool_offset = 0; arena->count = 0; arena->capacity = max_tokens;
    return arena;
}

void forge_trim(ForgeToken *t) {
    while (t->length > 0 && isspace((unsigned char)*t->data)) { t->data++; t->length--; }
    while (t->length > 0 && isspace((unsigned char)t->data[t->length - 1])) { t->length--; }
}

void arena_store_clean(ForgeArena *arena, const char *raw, size_t len) {
    if (arena->count >= arena->capacity) return;
    ForgeToken *t = &arena->tokens[arena->count];
    if (len >= 2 && raw[0] == '"' && raw[len-1] == '"') {
        char *ptr = &arena->string_pool[arena->pool_offset];
        size_t c_len = 0;
        for (size_t i = 1; i < len - 1; i++) {
            if (raw[i] == '"' && raw[i+1] == '"') { ptr[c_len++] = '"'; i++; }
            else { ptr[c_len++] = raw[i]; }
        }
        t->data = ptr; t->length = c_len; t->is_sanitized = 1;
        arena->pool_offset += c_len;
    } else {
        t->data = (char*)raw; t->length = len; t->is_sanitized = 0;
    }
    forge_trim(t);
    arena->count++;
}

ValidationResult validate_token(ForgeToken *t, ForgeType expected, int required) {
    if (t->length == 0) return required ? ERR_NULL : VALID;
    if (expected == FORGE_STRING) return VALID;
    char buffer[256]; snprintf(buffer, sizeof(buffer), "%.*s", (int)t->length, t->data);
    char *endptr;
    if (expected == FORGE_INT) { strtol(buffer, &endptr, 10); return (*endptr == '\0') ? VALID : ERR_TYPE; }
    if (expected == FORGE_FLOAT) { strtod(buffer, &endptr); return (*endptr == '\0') ? VALID : ERR_TYPE; }
    return ERR_TYPE;
}

void forge_scribe(ForgeArena *arena, ForgeConfig *cfg) {
    FILE *f_main = fopen("audit_data.json", "w");
    FILE *f_q = (cfg->policy == FORGE_QUARANTINE) ? fopen("quarantine.json", "w") : NULL;
    fprintf(f_main, "[\n"); if (f_q) fprintf(f_q, "[\n");
    size_t total_rows = arena->count / cfg->expected_cols;
    int first_main = 1, first_q = 1;
    for (size_t r = (cfg->header ? 1 : 0); r < total_rows; r++) {
        int row_valid = 1; char *err = "NONE";
        for (int c = 0; c < cfg->expected_cols; c++) {
            ValidationResult res = validate_token(&arena->tokens[r * cfg->expected_cols + c], cfg->types[c], cfg->required[c]);
            if (res != VALID) { row_valid = 0; err = (res == ERR_NULL) ? "NULL" : "TYPE"; break; }
        }
        if (cfg->policy == FORGE_STRICT && !row_valid) continue;
        FILE *target = (cfg->policy == FORGE_QUARANTINE && !row_valid) ? f_q : f_main;
        int *is_first = (target == f_q) ? &first_q : &first_main;
        fprintf(target, "%s  { \"row\": %zu, \"valid\": %s, \"msg\": \"%s\", \"data\": {", 
                *is_first ? "" : ",\n", r, row_valid ? "true" : "false", err);
        *is_first = 0;
        for (int c = 0; c < cfg->expected_cols; c++) {
            ForgeToken *t = &arena->tokens[r * cfg->expected_cols + c];
            fprintf(target, "\"c%d\": \"%.*s\"%s", c, (int)t->length, t->data, (c == cfg->expected_cols-1) ? "" : ", ");
        }
        fprintf(target, "} }");
    }
    fprintf(f_main, "\n]\n"); fclose(f_main); if (f_q) { fprintf(f_q, "\n]\n"); fclose(f_q); }
}

ForgeConfig load_manifest(const char *path) {
    ForgeConfig cfg = {',', 3, 1, {FORGE_STRING}, {1}, FORGE_ALL};
    FILE *f = fopen(path, "r"); if (!f) return cfg;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "delimiter=", 10) == 0) cfg.delimiter = line[10];
        else if (strncmp(line, "expected_cols=", 14) == 0) cfg.expected_cols = atoi(&line[14]);
        else if (strncmp(line, "policy=", 7) == 0) cfg.policy = (ForgePolicy)atoi(&line[7]);
        else if (strncmp(line, "types=", 6) == 0) {
            char *p = line + 6; for (int i = 0; i < cfg.expected_cols; i++) { cfg.types[i] = (ForgeType)(*p - '0'); p += 2; }
        }
        else if (strncmp(line, "required=", 9) == 0) {
            char *p = line + 9; for (int i = 0; i < cfg.expected_cols; i++) { cfg.required[i] = (*p - '0'); p += 2; }
        }
    }
    fclose(f); return cfg;
}

// 3. MAIN EXECUTION
int main(int argc, char *argv[]) {
    if (argc != 2) return 1;
    ForgeTelemetry tlm;
    clock_gettime(CLOCK_MONOTONIC, &tlm.start_total);

    // --- PHASE 1: INGESTION ---
    clock_gettime(CLOCK_MONOTONIC, &tlm.start_ingest);
    ForgeConfig config = load_manifest("schema.forge");
    int fd = open(argv[1], O_RDONLY);
    struct stat sb; fstat(fd, &sb);
    char *map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    clock_gettime(CLOCK_MONOTONIC, &tlm.end_ingest);

    // --- PHASE 2: LOGIC (Parsing + Validation) ---
    clock_gettime(CLOCK_MONOTONIC, &tlm.start_logic);
    ForgeArena *arena = arena_init(4000000, 1024 * 1024 * 10); // 4M tokens, 10MB Pool
    int in_quotes = 0; const char *field_start = map;
    for (size_t i = 0; i < sb.st_size; i++) {
        char c = map[i];
        if (c == '"') {
            if (in_quotes && (i + 1 < sb.st_size) && map[i+1] == '"') { i++; continue; }
            in_quotes = !in_quotes;
        } else if ((c == config.delimiter || c == '\n') && !in_quotes) {
            arena_store_clean(arena, field_start, &map[i] - field_start);
            field_start = &map[i + 1];
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &tlm.end_logic);

    // --- PHASE 3: SCRIBE (Serialization) ---
    clock_gettime(CLOCK_MONOTONIC, &tlm.start_scribe);
    forge_scribe(arena, &config);
    clock_gettime(CLOCK_MONOTONIC, &tlm.end_scribe);

    clock_gettime(CLOCK_MONOTONIC, &tlm.end_total);

    // --- TELEMETRY REPORT ---
    double total = diff_ms(tlm.start_total, tlm.end_total);
    double ingest = diff_ms(tlm.start_ingest, tlm.end_ingest);
    double logic = diff_ms(tlm.start_logic, tlm.end_logic);
    double scribe = diff_ms(tlm.start_scribe, tlm.end_scribe);

    printf("\n📊 [FORGE-CORE v1.5 TELEMETRY]\n");
    printf("------------------------------------------\n");
    printf("1. Ingest (mmap):   %8.2f ms  (%5.1f%%)\n", ingest, (ingest/total)*100);
    printf("2. Logic (Parse):   %8.2f ms  (%5.1f%%)\n", logic, (logic/total)*100);
    printf("3. Scribe (JSON):   %8.2f ms  (%5.1f%%)\n", scribe, (scribe/total)*100);
    printf("------------------------------------------\n");
    printf("TOTAL EXECUTION:    %8.2f ms\n", total);
    printf("THROUGHPUT:         %.2f Million Rows/Sec\n\n", (arena->count/3.0) / (total/1000.0) / 1000000.0);

    munmap(map, sb.st_size); close(fd);
    return 0;
}
