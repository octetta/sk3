#ifndef SKRED_SEQ_H
#define SKRED_SEQ_H

#include <stdint.h>

#define SEQ_FRAMES_PER_CALLBACK (128)

#define PATTERNS_MAX (128)
#define STEP_MAX (4096)

enum {
  SEQ_STOPPED = 0,
  SEQ_RUNNING = 1,
  SEQ_PAUSED = 2,
};

#define QUEUE_SIZE (2048) // the actual queue max depth

#define SEQ_OPCODE_ARG_MAX (8)
#define SEQ_PROGRAM_OP_MAX (256)
#define SEQ_TEMPO_MIN_BPM (1.0f)
#define SEQ_TEMPO_MAX_BPM (960.0f)
#define SEQ_MAX_CATCHUP_TICKS (64)

typedef struct {
  uint16_t code;
  uint8_t argc;
  char mode;
  uint8_t var_mask;
  uint8_t stream_mask;
  float arg[SEQ_OPCODE_ARG_MAX];
} opcode_event_t;

typedef struct {
  int voice;
  uint8_t voice_var;
  uint8_t source_valid;
  int pattern;
  int step;
  int tag;
  opcode_event_t opcode;
} event_t;

typedef struct {
  opcode_event_t opcode;
} program_op_t;

typedef struct {
  uint16_t count;
  program_op_t op[SEQ_PROGRAM_OP_MAX];
} event_program_t;

/* Canonical type aliases for namespacing */
typedef opcode_event_t seq_opcode_event_t;
typedef event_t seq_event_t;
typedef program_op_t seq_program_op_t;
typedef event_program_t seq_event_program_t;

void seq(uint64_t now, void (*event_fn)(const event_t *event),
  void (*program_fn)(int pattern, int step, const event_program_t *program));
int seq_next_boundary(uint64_t now, uint64_t limit, uint64_t *boundary);
void seq_init(void);
void seq_rewind(void);
uint64_t seq_master_tick(void);
void pattern_reset(int p);
int queue_event(uint64_t when, const event_t *event, int tag);
int tempo_set_subdivision(float bpm, float subdivision);
int tempo_set_subdivision_locked(float bpm, float subdivision, int in_audio_thread);
int tempo_set(float bpm);
float tempo_bpm_get(void);
float tempo_subdivision_get(void);
float tempo_step_seconds_get(void);
uint64_t seq_overhead_ns_get(uint64_t *max_ns);
void seq_overhead_reset(void);

/* The master pattern is used as the downbeat reference when queueing other
 * patterns with zq1.  While the master pattern is running, any other pattern
 * with a pending queue waits for the master pattern's step 0 before starting.
 * Default is 0.  Pass -1 to disable master-pattern sync entirely (each pattern
 * queues independently on its own step 0). */
void seq_master_pattern_set(int p);
int  seq_master_pattern_get(void);

void seq_modulo_set(int pattern, int m);
void seq_modulo_set_locked(int pattern, int m);
void seq_mute_set(int pattern, int state);
void seq_control_events_set(int pattern, int state);
int seq_step_set(int pattern, int step, const char *source,
  const event_program_t *program);
char *seq_step_get(int pattern, int step);
int seq_step_append(int pattern, const char *source,
  const event_program_t *program);
void seq_pattern_length_set(int pattern, int len);
void seq_step_goto(int pattern, int step);
void seq_step_goto_locked(int pattern, int step);
void seq_state_set(int p, int state);
void seq_state_set_locked(int p, int state);
void seq_state_queue(int p, int state);
void seq_state_all(int state);
void seq_state_multi(int state, const int *patterns, int count);
extern int seq_current_pattern;
int seq_pattern_generation(int pattern);
void seq_edit_lock(void);
void seq_edit_unlock(void);

extern int requested_seq_frames_per_callback;
extern int seq_frames_per_callback;

extern int seq_pointer[PATTERNS_MAX];
extern int seq_modulo[PATTERNS_MAX];
extern int seq_counter[PATTERNS_MAX];
extern int seq_state[PATTERNS_MAX];
extern int seq_pending_state[PATTERNS_MAX];
extern int seq_mute[PATTERNS_MAX];
extern int seq_control_events[PATTERNS_MAX];
extern int seq_pattern_length[PATTERNS_MAX];
extern int seq_explicit_length[PATTERNS_MAX];
extern float seq_pattern_rate[PATTERNS_MAX];
extern float seq_pattern_hz[PATTERNS_MAX];

#define SCENE_MAX (32)
#define SCENE_LANES (8)

typedef struct {
  int pattern[SCENE_LANES];
} seq_scene_t;

extern seq_scene_t seq_scenes[SCENE_MAX];
extern int seq_active_scene;
extern int seq_pending_scene;
extern int seq_lane_pattern[SCENE_LANES];

int seq_pattern_length_get(int pattern);
void seq_pattern_length_set(int pattern, int len);

float seq_pattern_rate_get(int pattern);
void seq_pattern_rate_set(int pattern, float rate);

float seq_pattern_hz_get(int pattern);
void seq_pattern_hz_set(int pattern, float hz);

void seq_scene_define(int scene_id, const int *patterns, int count);
void seq_scene_cue(int scene_id);
void seq_scene_now(int scene_id);
int seq_scene_active_get(void);
int seq_scene_pattern_get(int scene_id, int lane);

const char *seq_step_source(int pattern, int step);
const event_program_t *seq_step_program(int pattern, int step);

#ifndef SKRED_TEXT_T_DEFINED
#define SKRED_TEXT_T_DEFINED
#define TEXT_MAX (32+1)
typedef char text_t[TEXT_MAX];
#endif

extern text_t seq_text[PATTERNS_MAX];

int seq_queued(void);
int seq_capacity(void);

int seq_foreach(int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*), void *user);

int seq_kill_by_tag(int tag);

char *seq_stats(void);

int seq_kill_all(void);

/* Canonical seq_* function aliases */
#define seq_pattern_reset pattern_reset
#define seq_queue_event queue_event
#define seq_tempo_set tempo_set
#define seq_tempo_set_subdivision tempo_set_subdivision
#define seq_tempo_bpm_get tempo_bpm_get
#define seq_tempo_subdivision_get tempo_subdivision_get
#define seq_tempo_step_seconds_get tempo_step_seconds_get

#endif /* SKRED_SEQ_H */
