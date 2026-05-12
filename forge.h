#ifndef FORGE_H
#define FORGE_H

#include <stdint.h>
#include <pthread.h>
#include <immintrin.h>

// --- SCHEMA ARCHITECTURE ---
typedef enum {
    TYPE_STRING = 0,
    TYPE_INT    = 1,
} ColType;

typedef struct {
    char delimiter;
    int expected_cols;
    uint32_t validation_mask; 
} ForgeSchema;

// --- THREADING & TELEMETRY ---
typedef struct {
    int thread_id;
    char *map_base;
    size_t start_offset;
    size_t end_offset;
    ForgeSchema *schema;
    
    // Telemetry: Per-column error tracking
    size_t valid_rows;
    size_t column_errors[32]; 
} ForgeThreadTask;

void *forge_worker(void *arg);

#endif
