#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <immintrin.h>
#include "forge.h"

static inline uint32_t validate_chunk(__m256i chunk, __m256i v_zero, __m256i v_nine) {
    __m256i v_bias = _mm256_sub_epi8(chunk, v_zero);
    __m256i v_err_bytes = _mm256_subs_epu8(v_bias, v_nine);
    __m256i v_is_digit = _mm256_cmpeq_epi8(v_err_bytes, _mm256_setzero_si256());
    return ~_mm256_movemask_epi8(v_is_digit);
}

static inline void audit_and_attribute(uint32_t violations, uint32_t d_mask, uint32_t n_mask, 
                                      int *current_col, uint32_t schema_mask, size_t *col_errors) {
    int col = *current_col;
    int last_pos = 0;
    uint32_t separators = d_mask | n_mask;

    while (separators) {
        int pos = __builtin_ctz(separators);
        if ((schema_mask >> col) & 1) {
            uint32_t segment_mask = ((1U << pos) - 1) & ~((1U << last_pos) - 1);
            col_errors[col] += __builtin_popcount(violations & segment_mask);
        }
        if ((d_mask >> pos) & 1) col++;
        else { *current_col = 0; return; }
        last_pos = pos + 1;
        separators &= (separators - 1);
    }

    if ((schema_mask >> col) & 1) {
        uint32_t remainder_mask = ~((1U << last_pos) - 1);
        col_errors[col] += __builtin_popcount(violations & remainder_mask);
    }
    *current_col = col;
}

void *forge_worker(void *arg) {
    ForgeThreadTask *task = (ForgeThreadTask *)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(task->thread_id % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    const char *data = task->map_base;
    size_t i = task->start_offset;
    const __m256i v_delim = _mm256_set1_epi8(task->schema->delimiter);
    const __m256i v_newline = _mm256_set1_epi8('\n');
    const __m256i v_zero = _mm256_set1_epi8(0x30);
    const __m256i v_nine = _mm256_set1_epi8(9);
    
    const uint32_t s_mask = task->schema->validation_mask;
    const int target_cols = task->schema->expected_cols;
    int cur_col = 0;

    while (i < task->end_offset) {
        size_t line_len = 0;
        int line_cols = 1;

        while (i + line_len < task->end_offset) {
            if (i + line_len + 32 <= task->end_offset) {
                __m256i chunk = _mm256_loadu_si256((const __m256i *)&data[i + line_len]);
                uint32_t n_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_newline));
                uint32_t d_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, v_delim));
                uint32_t violations = validate_chunk(chunk, v_zero, v_nine);
                
                int temp_col = cur_col;
                audit_and_attribute(violations, d_mask, n_mask, &temp_col, s_mask, task->column_errors);
                
                if (n_mask == 0) {
                    line_cols += __builtin_popcount(d_mask);
                    cur_col = temp_col;
                    line_len += 32;
                    continue;
                } else {
                    int first_n = __builtin_ctz(n_mask);
                    line_cols += __builtin_popcount(d_mask & ((1U << first_n) - 1));
                    line_len += first_n;
                    cur_col = 0; 
                    break;
                }
            }
            if (data[i + line_len] == '\n') { cur_col = 0; break; }
            if (data[i + line_len] == task->schema->delimiter) { line_cols++; cur_col++; }
            else if ((s_mask >> cur_col) & 1) {
                if (data[i + line_len] < '0' || data[i + line_len] > '9') task->column_errors[cur_col]++;
            }
            line_len++;
        }
        task->valid_rows += (line_cols == target_cols);
        i += line_len + 1;
    }
    return NULL;
}
