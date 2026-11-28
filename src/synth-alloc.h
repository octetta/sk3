#ifndef SKRED_SYNTH_ALLOC_H
#define SKRED_SYNTH_ALLOC_H

/*
 * synth-alloc.h — declarations for the voice/wave heap allocators.
 *
 * Call order:
 *   1. synth_config_defaults()          — sets voice_max/wave_table_max to
 *                                         compiled-in defaults (rounded to
 *                                         VOICE_ALIGN).  synth_init() does
 *                                         this automatically if you forget,
 *                                         but calling it from argv parsing
 *                                         lets you override with -v/-w first.
 *   2. synth_config_set_voices(n)       — optional, from argv
 *   3. synth_init()                     — calls synth_alloc_voices/waves
 *   4. ... run ...
 *   5. synth_free()                     — calls synth_free_voices/waves
 */

void synth_config_defaults(void);
void synth_config_set_voices(int n);
void synth_config_set_waves(int n);

void synth_alloc_voices(int voice_max);
void synth_free_voices(void);

void synth_alloc_waves(int wave_table_max);
void synth_free_waves(void);

/* ------------------------------------------------------------------ */
/* Lightweight allocation registry (control-thread only)              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;      /* field / buffer name                     */
    const char *kind;      /* "voice", "wave-meta", "wave-data", ...  */
    size_t     elem_size;  /* sizeof(element)                         */
    int        count;      /* number of elements                      */
    size_t     bytes;      /* count * elem_size                       */
    int        is_dynamic; /* 0 = synth_init static, 1 = runtime      */
} synth_mem_rec_t;

#define SYNTH_MEM_REC_MAX 512

/* Record or update one allocation. Safe to call from control thread only.
 * If an entry with the same name already exists, updates in place.
 * Silently drops if the fixed table is full or arguments are invalid. */
void synth_mem_register(const char *name, const char *kind,
                        size_t elem_size, int count, int is_dynamic);

/* Unregister an allocation by name. */
void synth_mem_unregister(const char *name);

/* Clear static allocations (called on synth_free_voices/synth_free_waves). */
void synth_mem_clear_static(void);

/* Forward declaration for skode context */
struct skode_s;

/* Print large (>= 4 KiB) records to Skode context (for /s 8). */
void synth_mem_report(struct skode_s *ctx);

/* Print compact memory summary line (for /s 0). */
void synth_mem_summary(struct skode_s *ctx);

/* Total bytes currently tracked by the registry. */
size_t synth_mem_total_bytes(void);

#endif /* SKRED_SYNTH_ALLOC_H */
