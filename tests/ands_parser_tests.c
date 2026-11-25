#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ands.h"

#define MAX_EVENTS 64
#define MAX_ARGS 8

typedef struct {
  int info;
  char atom[5];
  int argc;
  double args[MAX_ARGS];
  int arg_var[MAX_ARGS];
  char text[256];
  char defer_mode;
  double defer_num;
  int defer_var;
} event_t;

typedef struct {
  event_t events[MAX_EVENTS];
  int len;
} recorder_t;

static int failures = 0;

static const char *event_name(int info) {
  switch (info) {
    case FUNCTION: return "FUNCTION";
    case DEFER: return "DEFER";
    case GOT_STRING: return "GOT_STRING";
    case GOT_ARRAY: return "GOT_ARRAY";
    case CHUNK_END: return "CHUNK_END";
    default: return "OTHER";
  }
}

static void fail(const char *test, const char *msg) {
  fprintf(stderr, "FAIL %s: %s\n", test, msg);
  failures++;
}

static int near_double(double a, double b) {
  if (isnan(a) && isnan(b)) return 1;
  return fabs(a - b) < 1e-9;
}

static int record_cb(ands_t *s, int info) {
  recorder_t *r = (recorder_t *)ands_user(s);
  if (info == MACRO_DEFINED || info == MACRO_REMOVING) return 0;
  if (r->len >= MAX_EVENTS) return 0;

  event_t *e = &r->events[r->len++];
  memset(e, 0, sizeof(*e));
  e->info = info;
  strncpy(e->atom, ands_atom_string(s), sizeof(e->atom) - 1);
  e->argc = ands_arg_len(s);
  if (e->argc > MAX_ARGS) e->argc = MAX_ARGS;
  for (int i = 0; i < e->argc; i++) {
    e->args[i] = ands_arg(s)[i];
    e->arg_var[i] = ands_arg_var(s, i);
  }

  if (info == GOT_STRING) {
    strncpy(e->text, ands_string(s), sizeof(e->text) - 1);
  } else if (info == GOT_ARRAY) {
    e->argc = ands_data_len(s);
    if (e->argc > MAX_ARGS) e->argc = MAX_ARGS;
    for (int i = 0; i < e->argc; i++) e->args[i] = ands_data(s)[i];
  } else if (info == DEFER) {
    e->defer_mode = ands_defer_mode(s);
    e->defer_num = ands_defer_num(s);
    e->defer_var = ands_defer_var(s);
    strncpy(e->text, ands_defer_string(s), sizeof(e->text) - 1);
  }

  return 0;
}

static void expect_len(const char *test, recorder_t *r, int n) {
  if (r->len != n) {
    char msg[128];
    snprintf(msg, sizeof(msg), "expected %d events, got %d", n, r->len);
    fail(test, msg);
    for (int i = 0; i < r->len; i++) {
      fprintf(stderr, "  %d: %s %s argc=%d text=[%s]\n",
              i, event_name(r->events[i].info), r->events[i].atom,
              r->events[i].argc, r->events[i].text);
    }
  }
}

static void expect_event(const char *test, recorder_t *r, int idx, int info, const char *atom) {
  if (idx >= r->len) {
    fail(test, "missing event");
    return;
  }
  event_t *e = &r->events[idx];
  if (e->info != info || strcmp(e->atom, atom) != 0) {
    char msg[160];
    snprintf(msg, sizeof(msg), "event %d expected %s %s, got %s %s",
             idx, event_name(info), atom, event_name(e->info), e->atom);
    fail(test, msg);
  }
}

static void expect_arg(const char *test, recorder_t *r, int idx, int argi, double value) {
  if (idx >= r->len || argi >= r->events[idx].argc) {
    fail(test, "missing arg");
    return;
  }
  if (!near_double(r->events[idx].args[argi], value)) {
    char msg[160];
    snprintf(msg, sizeof(msg), "event %d arg %d expected %.12g, got %.12g",
             idx, argi, value, r->events[idx].args[argi]);
    fail(test, msg);
  }
}

static void run_parse(const char *test, char *input, recorder_t *r) {
  memset(r, 0, sizeof(*r));
  ands_t *s = ands_new(record_cb, r);
  if (!s) {
    fail(test, "ands_new returned NULL");
    return;
  }
  ands_consume(s, input);
  ands_free(s);
}

static void test_compact_commands(void) {
  const char *test = "compact commands";
  recorder_t r;
  char input[] = "v0w1f440a-12";
  run_parse(test, input, &r);

  expect_len(test, &r, 5);
  expect_event(test, &r, 0, FUNCTION, "v---");
  expect_arg(test, &r, 0, 0, 0);
  expect_event(test, &r, 1, FUNCTION, "w---");
  expect_arg(test, &r, 1, 0, 1);
  expect_event(test, &r, 2, FUNCTION, "f---");
  expect_arg(test, &r, 2, 0, 440);
  expect_event(test, &r, 3, FUNCTION, "a---");
  expect_arg(test, &r, 3, 0, -12);
  expect_event(test, &r, 4, CHUNK_END, "----");
}

static void test_strings_and_arrays(void) {
  const char *test = "strings and arrays";
  recorder_t r;
  char input[] = "[kick one] vt (1, 2 -3.5 4e2) d*";
  run_parse(test, input, &r);

  expect_len(test, &r, 5);
  expect_event(test, &r, 0, GOT_STRING, "----");
  if (strcmp(r.events[0].text, "kick one") != 0) fail(test, "string text mismatch");
  expect_event(test, &r, 1, GOT_ARRAY, "vt--");
  expect_arg(test, &r, 1, 0, 1);
  expect_arg(test, &r, 1, 1, 2);
  expect_arg(test, &r, 1, 2, -3.5);
  expect_arg(test, &r, 1, 3, 400);
  expect_event(test, &r, 2, FUNCTION, "vt--");
  expect_event(test, &r, 3, FUNCTION, "d*--");
  expect_event(test, &r, 4, CHUNK_END, "----");
}

static void test_variables_and_nan_defaults(void) {
  const char *test = "variables and nan defaults";
  recorder_t r;
  memset(&r, 0, sizeof(r));
  ands_t *s = ands_new(record_cb, &r);
  ands_set_local(s, 0, 42);
  ands_set_local(s, 127, -7);
  char input[] = "$0 $127 . - f";
  ands_consume(s, input);
  ands_free(s);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "f---");
  expect_arg(test, &r, 0, 0, 42);
  expect_arg(test, &r, 0, 1, -7);
  expect_arg(test, &r, 0, 2, NAN);
  expect_arg(test, &r, 0, 3, NAN);
  expect_event(test, &r, 1, CHUNK_END, "----");
}

static void test_comments_and_chunks(void) {
  const char *test = "comments and chunks";
  recorder_t r;
  char input[] = "v0 # ignore this; f220";
  run_parse(test, input, &r);

  expect_len(test, &r, 4);
  expect_event(test, &r, 0, FUNCTION, "v---");
  expect_event(test, &r, 1, CHUNK_END, "----");
  expect_event(test, &r, 2, FUNCTION, "f---");
  expect_arg(test, &r, 2, 0, 220);
  expect_event(test, &r, 3, CHUNK_END, "----");
}

static void test_defer(void) {
  const char *test = "defer";
  recorder_t r;
  char input[] = "+100 v0 a10~50 T;";
  run_parse(test, input, &r);

  expect_len(test, &r, 4);
  expect_event(test, &r, 0, CHUNK_END, "----");
  expect_event(test, &r, 1, DEFER, "----");
  if (r.events[1].defer_mode != '~' || !near_double(r.events[1].defer_num, 100)) {
    fail(test, "first defer timing mismatch");
  }
  if (strcmp(r.events[1].text, " v0 a10") != 0) fail(test, "first defer text mismatch");
  expect_event(test, &r, 2, DEFER, "----");
  if (r.events[2].defer_mode != '~' || !near_double(r.events[2].defer_num, 50)) {
    fail(test, "second defer timing mismatch");
  }
  if (strcmp(r.events[2].text, " T") != 0) fail(test, "second defer text mismatch");
  expect_event(test, &r, 3, CHUNK_END, "----");
}

static void test_variable_provenance(void) {
  const char *test = "variable provenance";
  recorder_t r;
  char input[] = "v$3 n60,$4 +$5 l$6";
  run_parse(test, input, &r);

  expect_event(test, &r, 0, FUNCTION, "v---");
  if (r.events[0].arg_var[0] != 3)
    fail(test, "voice argument lost variable provenance");
  expect_event(test, &r, 1, FUNCTION, "n---");
  if (r.events[1].arg_var[0] != -1 || r.events[1].arg_var[1] != 4)
    fail(test, "mixed note arguments lost provenance");
  expect_event(test, &r, 3, DEFER, "----");
  if (r.events[3].defer_var != 5)
    fail(test, "defer argument lost variable provenance");
}

static void test_stream_defer(void) {
  const char *test = "stream defer";
  recorder_t r;
  char input[] = "+&3 v0; ~&5 v1;";
  run_parse(test, input, &r);

  expect_event(test, &r, 1, DEFER, "----");
  if (r.events[1].defer_var != (3 | ANDS_STREAM_FLAG))
    fail(test, "first defer lost stream provenance");
  expect_event(test, &r, 3, DEFER, "----");
  if (r.events[3].defer_var != (5 | ANDS_STREAM_FLAG))
    fail(test, "second defer lost stream provenance");
}

static double dummy_stream_pull(void *user, int n) {
  (void)user;
  if (n == 1) return 20.5;
  return 0.0;
}

static void test_arrays_with_variables_and_streams(void) {
  const char *test = "arrays with variables and streams";
  recorder_t r;
  memset(&r, 0, sizeof(r));
  ands_t *s = ands_new(record_cb, &r);
  ands_set_local(s, 0, 10.5);
  ands_set_stream_pull(s, dummy_stream_pull);
  char input[] = "($0 &1 42) vt; ($0 &0)";
  ands_consume(s, input);
  ands_free(s);

  expect_event(test, &r, 0, GOT_ARRAY, "----");
  expect_arg(test, &r, 0, 0, 10.5);
  expect_arg(test, &r, 0, 1, 20.5);
  expect_arg(test, &r, 0, 2, 42.0);

  expect_event(test, &r, 3, GOT_ARRAY, "----");
  expect_arg(test, &r, 3, 0, 10.5);
  expect_arg(test, &r, 3, 1, 0.0);
}

static void test_macros_basic(void) {
  const char *test = "macros basic";
  recorder_t r;
  char input[] = "[ar] : t $$0 0 $$1 0 ; ar 0.01 0.4";
  ands_macro_clear(NULL);
  run_parse(test, input, &r);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "t---");
  expect_arg(test, &r, 0, 0, 0.01);
  expect_arg(test, &r, 0, 1, 0);
  expect_arg(test, &r, 0, 2, 0.4);
  expect_arg(test, &r, 0, 3, 0);
  expect_event(test, &r, 1, CHUNK_END, "----");
}

static void test_return_references(void) {
  const char *test = "return references";
  recorder_t r;
  memset(&r, 0, sizeof(r));
  ands_t *s = ands_new(record_cb, &r);
  if (!s) {
    fail(test, "ands_new returned NULL");
    return;
  }
  ands_return_set(s, 0, 0.01);
  ands_return_set(s, 1, 0.4);
  char input[] = "@0 0 @1 0 t";
  ands_consume(s, input);
  ands_free(s);

  expect_len(test, &r, 4);
  expect_event(test, &r, 2, FUNCTION, "t---");
  expect_arg(test, &r, 2, 0, 0.01);
  expect_arg(test, &r, 2, 1, 0);
  expect_arg(test, &r, 2, 2, 0.4);
  expect_arg(test, &r, 2, 3, 0);
  expect_event(test, &r, 3, CHUNK_END, "----");
}

static void test_return_sigil_is_not_an_atom(void) {
  const char *test = "return sigil is not an atom";
  recorder_t r;
  char input[] = "@ f440";
  run_parse(test, input, &r);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "f---");
  expect_arg(test, &r, 0, 0, 440);
  expect_event(test, &r, 1, CHUNK_END, "----");
}

static void test_macro_names_truncate_to_atom_width(void) {
  const char *test = "macro names truncate to atom width";
  recorder_t r;
  /* Test 1: macro names up to 31 chars (e.g. 'attack') work without truncation */
  char input1[] = "[attack]: t $$0 0 $$1 0 ; attack 0.02 0.5";
  ands_macro_clear(NULL);
  run_parse(test, input1, &r);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "t---");
  expect_arg(test, &r, 0, 0, 0.02);
  expect_arg(test, &r, 0, 1, 0);
  expect_arg(test, &r, 0, 2, 0.5);
  expect_arg(test, &r, 0, 3, 0);
  expect_event(test, &r, 1, CHUNK_END, "----");

  /* Test 2: macro names > 31 chars truncate to 31 chars */
  char input2[] = "[abcdefghijklmnopqrstuvwxyzabcdefghij]: t $$0 0 $$1 0 ; abcdefghijklmnopqrstuvwxyzabcde 0.03 0.6";
  ands_macro_clear(NULL);
  run_parse(test, input2, &r);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "t---");
  expect_arg(test, &r, 0, 0, 0.03);
  expect_arg(test, &r, 0, 1, 0);
  expect_arg(test, &r, 0, 2, 0.6);
  expect_arg(test, &r, 0, 3, 0);
  expect_event(test, &r, 1, CHUNK_END, "----");
}

static void test_macros_nested_and_replace(void) {
  const char *test = "macros nested and replace";
  recorder_t r;
  char input[] = "[ar]: t $$0 0 $$1 0 ; [aa]: f $$0 ; [aa]: ar $$0 .4 ; aa .01";
  ands_macro_clear(NULL);
  run_parse(test, input, &r);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "t---");
  expect_arg(test, &r, 0, 0, 0.01);
  expect_arg(test, &r, 0, 1, 0);
  expect_arg(test, &r, 0, 2, 0.4);
  expect_arg(test, &r, 0, 3, 0);
  expect_event(test, &r, 1, CHUNK_END, "----");
}

static void test_macro_definitions_do_not_break_strings(void) {
  const char *test = "macro definitions do not break strings";
  recorder_t r;
  char input[] = "[kick one] vt [ar]: t $$0 0 ; ar 2";
  ands_macro_clear(NULL);
  run_parse(test, input, &r);

  expect_len(test, &r, 4);
  expect_event(test, &r, 0, GOT_STRING, "----");
  if (strcmp(r.events[0].text, "kick one") != 0) fail(test, "string text mismatch");
  expect_event(test, &r, 1, FUNCTION, "vt--");
  expect_event(test, &r, 2, FUNCTION, "t---");
  expect_arg(test, &r, 2, 0, 2);
  expect_arg(test, &r, 2, 1, 0);
  expect_event(test, &r, 3, CHUNK_END, "----");
}

static void test_macros_are_global(void) {
  const char *test = "macros are global";
  recorder_t r;
  ands_t *s = ands_new(record_cb, &r);
  char define[] = "[zz]: f $$0 ;";
  char use[] = "zz 220";
  char fresh_use[] = "zz 330";

  memset(&r, 0, sizeof(r));
  ands_macro_clear(NULL);
  if (!s) {
    fail(test, "ands_new returned NULL");
    return;
  }

  ands_consume(s, define);
  r.len = 0;
  ands_consume(s, use);
  ands_free(s);

  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "f---");
  expect_arg(test, &r, 0, 0, 220);
  expect_event(test, &r, 1, CHUNK_END, "----");

  run_parse(test, fresh_use, &r);
  expect_len(test, &r, 2);
  expect_event(test, &r, 0, FUNCTION, "f---");
  expect_arg(test, &r, 0, 0, 330);
  expect_event(test, &r, 1, CHUNK_END, "----");
  ands_macro_clear(NULL);
}

static void test_macro_management_api(void) {
  const char *test = "macro management api";
  recorder_t r;
  ands_t *s = ands_new(record_cb, &r);
  char define[] = "[ar]: t $$0 0 $$1 0 ; [zz]: f $$0 ;";
  char name[ANDS_MACRO_NAME_LEN];
  char body[ANDS_MACRO_BODY_LEN];
  int arg_count = -1;

  memset(&r, 0, sizeof(r));
  ands_macro_clear(NULL);
  if (!s) {
    fail(test, "ands_new returned NULL");
    return;
  }

  ands_consume(s, define);
  expect_len(test, &r, 1);
  if (ands_macro_count(s) != 2) fail(test, "expected two macros");
  if (!ands_macro_get(s, 0, name, sizeof(name), body, sizeof(body), &arg_count)) {
    fail(test, "missing macro 0");
  } else if (name[0] == '\0' || body[0] == '\0' || arg_count < 1) {
    fail(test, "macro 0 contents invalid");
  }

  if (!ands_macro_remove(s, "ar")) fail(test, "remove ar failed");
  if (ands_macro_count(s) != 1) fail(test, "expected one macro after remove");
  if (ands_macro_remove(s, "ar")) fail(test, "remove ar should not repeat");

  ands_macro_clear(s);
  if (ands_macro_count(s) != 0) fail(test, "clear failed");

  char long_define[] = "[abcdefghijklmnopqrstuvwxyzabcdefghij]: t $$0 0 ;";
  ands_consume(s, long_define);
  if (!ands_macro_get(s, 0, name, sizeof(name), body, sizeof(body), &arg_count)) {
    fail(test, "missing long-name macro");
  } else if (strcmp(name, "abcdefghijklmnopqrstuvwxyzabcde") != 0) {
    fail(test, "long-name macro did not truncate to 31 chars");
  }

  ands_free(s);
  ands_macro_clear(NULL);
}

static void test_tie_and_rest_sentinels(void) {
  const char *test = "tie and rest sentinels";
  recorder_t r;
  char input[] = "( 60 . _ - -52 ) d*";
  run_parse(test, input, &r);

  expect_len(test, &r, 3);
  expect_event(test, &r, 0, GOT_ARRAY, "----");
  if (!near_double(r.events[0].args[0], 60)) fail(test, "expected 60 at arg 0");
  if (!ands_is_tie(r.events[0].args[1])) fail(test, "expected tie at arg 1 (.)");
  if (!ands_is_tie(r.events[0].args[2])) fail(test, "expected tie at arg 2 (_)");
  if (!ands_is_rest(r.events[0].args[3])) fail(test, "expected rest at arg 3 (-)");
  if (!near_double(r.events[0].args[4], -52)) fail(test, "expected -52 at arg 4");
  expect_event(test, &r, 1, FUNCTION, "d*--");
  expect_event(test, &r, 2, CHUNK_END, "----");
}

static void test_defer_space(void) {
  const char *test = "defer space";
  recorder_t r;

  char a[] = "+ 4 v0;";                      /* guide: "+ 4 /SS ..." */
  run_parse(test, a, &r);
  expect_event(test, &r, 1, DEFER, "----");
  if (r.len > 1 && !(r.events[1].defer_mode == '+' && near_double(r.events[1].defer_num, 4)))
    fail(test, "\"+ 4 v0\" should defer 4 beats");

  char b[] = "~ 1.5 f 440;";                 /* guide: "~ 1.5 f 440" */
  run_parse(test, b, &r);
  expect_event(test, &r, 1, DEFER, "----");
  if (r.len > 1 && !(r.events[1].defer_mode == '~' && near_double(r.events[1].defer_num, 1.5)))
    fail(test, "\"~ 1.5 f 440\" should defer 1.5 seconds");
  if (r.len > 1 && strcmp(r.events[1].text, " f 440") != 0)
    fail(test, "\"~ 1.5 f 440\" body should be \" f 440\"");

  char c[] = "+ -3 n57;";                    /* negative = ticks */
  run_parse(test, c, &r);
  if (r.len > 1 && !near_double(r.events[1].defer_num, -3)) fail(test, "\"+ -3\" should be -3");

  char d[] = "+   $1 v0;";                   /* several blanks, then a variable */
  run_parse(test, d, &r);
  if (r.len > 1 && r.events[1].defer_var != 1) fail(test, "\"+ $1\" lost its variable");

  char e[] = "+ v0 a10;";                     /* unchanged: zero delay, body kept as-is */
  run_parse(test, e, &r);
  if (r.len > 1 && !(near_double(r.events[1].defer_num, 0) && strcmp(r.events[1].text, " v0 a10") == 0))
    fail(test, "\"+ v0 a10\" must stay a zero delay with body \" v0 a10\"");

  char f[] = "+;";                            /* sigil then chunk end: unchanged */
  run_parse(test, f, &r);
  if (r.len > 1 && !near_double(r.events[1].defer_num, 0)) fail(test, "\"+;\" should be a zero delay");

  char g[] = "+4 v0;";                        /* attached form still works */
  run_parse(test, g, &r);
  if (r.len > 1 && !near_double(r.events[1].defer_num, 4)) fail(test, "attached form broke");
}

static void test_defer_space_streams(void) {
  const char *test = "defer space streams";
  recorder_t r;
  char a[] = "+ &3 v0; ~   &5 v1;";
  run_parse(test, a, &r);
  expect_len(test, &r, 5);
  if (r.len > 1 && r.events[1].defer_var != (3 | ANDS_STREAM_FLAG)) fail(test, "+ &3 lost stream provenance");
  if (r.len > 3 && r.events[3].defer_var != (5 | ANDS_STREAM_FLAG)) fail(test, "~   &5 lost stream provenance");
  if (r.len > 1 && r.events[1].defer_mode != '+') fail(test, "mode lost");
  char b[] = "+ $2 v0; ~ $4 v1;";
  run_parse(test, b, &r);
  expect_len(test, &r, 5);
  if (r.len > 1 && r.events[1].defer_var != 2) fail(test, "+ $2 lost variable provenance");
  if (r.len > 3 && r.events[3].defer_var != 4) fail(test, "~ $4 lost variable provenance");
}

static void test_stream_variable_sigil_edges(void) {
  const char *test = "bare $ / & sigils";
  recorder_t r;

  /* '&' needs digits (ands_var_parse only reads digits); a letter is not a stream */
  char a[] = "f &x;";
  run_parse(test, a, &r);
  expect_event(test, &r, 0, FUNCTION, "f---");
  if (r.len > 0 && r.events[0].argc != 0) fail(test, "\"f &x\" must not gain a phantom argument");
  expect_event(test, &r, 1, FUNCTION, "x---");

  /* a bare sigil in an array must not swallow the next character */
  char b[] = "( 1 &a 3 ) vt;";
  run_parse(test, b, &r);
  expect_event(test, &r, 0, GOT_ARRAY, "----");
  if (r.len > 0 && r.events[0].argc != 3) fail(test, "\"( 1 &a 3 )\" should hold 3 values");

  char c[] = "( 1 $xy 3 ) vt;";
  run_parse(test, c, &r);
  expect_event(test, &r, 0, GOT_ARRAY, "----");
  if (r.len > 0 && r.events[0].argc != 3) fail(test, "\"( 1 $xy 3 )\" should hold 3 values");

  /* a bare sigil in a defer must not swallow the first body character */
  char d[] = "+&x v0;";
  run_parse(test, d, &r);
  expect_event(test, &r, 1, DEFER, "----");
  if (r.len > 1 && strcmp(r.events[1].text, "x v0") != 0) fail(test, "\"+&x v0\" body should be \"x v0\"");

  char e[] = "+$v0 a10;";
  run_parse(test, e, &r);
  expect_event(test, &r, 1, DEFER, "----");
  if (r.len > 1 && strcmp(r.events[1].text, "v0 a10") != 0) fail(test, "\"+$v0 a10\" body should be \"v0 a10\"");
}


/* ------------------------------------------------------------------------ */
/* Append forms ("((" and "[[") and swap-position primitives                 */
/* ------------------------------------------------------------------------ */

static int null_cb(ands_t *s, int info) { (void)s; (void)info; return 0; }

static ands_t *new_plain(const char *test) {
  ands_t *s = ands_new(null_cb, NULL);
  if (!s) fail(test, "ands_new returned NULL");
  return s;
}

static void feed(ands_t *s, const char *text) {
  char buf[65536];
  snprintf(buf, sizeof(buf), "%s", text);
  ands_consume(s, buf);
}

static void expect_data(const char *test, ands_t *s, const double *want, int n) {
  if (ands_data_len(s) != n) {
    char msg[96];
    snprintf(msg, sizeof(msg), "data len expected %d, got %d", n, ands_data_len(s));
    fail(test, msg);
    return;
  }
  for (int i = 0; i < n; i++) {
    if (!near_double(ands_data(s)[i], want[i])) {
      char msg[96];
      snprintf(msg, sizeof(msg), "data[%d] expected %g, got %g", i, want[i], ands_data(s)[i]);
      fail(test, msg);
      return;
    }
  }
}

static void expect_str(const char *test, const char *got, const char *want) {
  if (strcmp(got, want) != 0) {
    char msg[200];
    snprintf(msg, sizeof(msg), "string expected [%s], got [%s]", want, got);
    fail(test, msg);
  }
}

static void test_array_append_form(void) {
  const char *test = "array append form";
  ands_t *s = new_plain(test);
  if (!s) return;

  feed(s, "((5 6)");                       /* append to empty */
  { double w[] = {5, 6}; expect_data(test, s, w, 2); }

  feed(s, "((7 8)");                       /* persists across consume calls */
  { double w[] = {5, 6, 7, 8}; expect_data(test, s, w, 4); }

  feed(s, "((9) ((10 11)");                /* several appends in one chunk */
  { double w[] = {5, 6, 7, 8, 9, 10, 11}; expect_data(test, s, w, 7); }

  ands_set_local(s, 1, 42);
  feed(s, "((1 $1 2)");                    /* variables work inside append */
  if (ands_data_len(s) != 10 || !near_double(ands_data(s)[8], 42)) fail(test, "variable in append array");

  feed(s, "(1 2)");                        /* plain form starts over ... */
  { double w[] = {1, 2}; expect_data(test, s, w, 2); }
  if (ands_data_prev_len(s) != 10) fail(test, "previous array not kept in swap position");

  feed(s, "()");                           /* ... and so does the empty form */
  if (ands_data_len(s) != 0 || ands_data_prev_len(s) != 2) fail(test, "empty array reset");
  ands_free(s);
}

static void test_array_append_events(void) {
  const char *test = "array append events";
  recorder_t r;
  char input[] = "((1 2) foo";
  run_parse(test, input, &r);
  expect_len(test, &r, 3);
  expect_event(test, &r, 0, GOT_ARRAY, "----");
  expect_event(test, &r, 1, FUNCTION, "foo-");
  expect_event(test, &r, 2, CHUNK_END, "----");
}

static void test_array_growth_and_ceiling(void) {
  const char *test = "array growth and ceiling";
  ands_t *s = new_plain(test);
  char *big = (char *)malloc(60000);
  if (!s || !big) { fail(test, "alloc"); return; }

  strcpy(big, "(");
  for (int i = 0; i < 3000; i++) { char n[16]; snprintf(n, sizeof(n), "%d ", i); strcat(big, n); }
  strcat(big, ")");
  feed(s, big);                            /* plain literal now grows past 1024 */
  if (ands_data_len(s) != 3000 || ands_data(s)[2999] != 2999) fail(test, "plain growth");

  strcpy(big, "(("); for (int i = 0; i < 3000; i++) { char n[16]; snprintf(n, sizeof(n), "%d ", i); strcat(big, n); }
  strcat(big, ")");
  feed(s, big);
  if (ands_data_len(s) != 6000) fail(test, "append growth");

  ands_set_limits(s, 7000, 0);             /* ceiling: whole piece or nothing */
  feed(s, big);
  if (ands_data_len(s) != 6000) fail(test, "over-ceiling append must be rolled back");
  if (!ands_overflow(s)) fail(test, "overflow flag not set");
  feed(s, "((1 2)");
  if (ands_data_len(s) != 6002 || ands_overflow(s)) fail(test, "append after overflow / flag not cleared");
  free(big);
  ands_free(s);
}

static void test_ceiling_applies_within_capacity(void) {
  const char *test = "ceiling within capacity";
  ands_t *s = new_plain(test);
  if (!s) return;
  ands_set_limits(s, 10, 0);
  feed(s, "(1 2 3)");
  feed(s, "((4 5 6 7 8 9 10 11 12 13 14 15)");   /* whole piece refused */
  if (ands_data_len(s) != 3 || !ands_overflow(s)) fail(test, "append not refused at ceiling");
  feed(s, "((4 5)");
  if (ands_data_len(s) != 5) fail(test, "append under ceiling");
  ands_free(s);
}

static void test_truncated_append_is_rolled_back(void) {
  const char *test = "truncated append";
  ands_t *s = new_plain(test);
  if (!s) return;
  feed(s, "(1 2)");
  feed(s, "((3 4 5.");                     /* closer lost, e.g. truncated datagram */
  { double w[] = {1, 2}; expect_data(test, s, w, 2); }
  feed(s, "((3)");                         /* parser is clean for the next message */
  { double w[] = {1, 2, 3}; expect_data(test, s, w, 3); }

  feed(s, "[abc]");
  feed(s, "[[def");
  expect_str(test, ands_string(s), "abc");
  feed(s, "[[def]");
  expect_str(test, ands_string(s), "abcdef");
  ands_free(s);
}

static void test_string_append_form(void) {
  const char *test = "string append form";
  ands_t *s = new_plain(test);
  if (!s) return;

  feed(s, "[[hello]");                     /* append with nothing before */
  expect_str(test, ands_string(s), "hello");
  feed(s, "[[ world]");
  expect_str(test, ands_string(s), "hello world");
  if (!ands_string_fresh(s)) fail(test, "append should mark the string fresh");

  feed(s, "[x]");                          /* plain: old string moves to swap position */
  expect_str(test, ands_string(s), "x");
  expect_str(test, ands_string_prev(s), "hello world");

  feed(s, "[a] [b] [[c]");                 /* [[ leaves the swap position alone */
  expect_str(test, ands_string(s), "bc");
  expect_str(test, ands_string_prev(s), "a");

  feed(s, "[]");
  expect_str(test, ands_string(s), "");
  ands_free(s);
}

static void test_string_append_events_and_macros(void) {
  const char *test = "string append events";
  recorder_t r;
  char input[] = "[[ab] foo";
  int macros_before, macros_after;
  ands_t *s;

  run_parse(test, input, &r);
  expect_len(test, &r, 3);
  expect_event(test, &r, 0, GOT_STRING, "----");
  expect_event(test, &r, 1, FUNCTION, "foo-");
  expect_str(test, r.events[0].text, "ab");

  s = new_plain(test);
  macros_before = ands_macro_count(s);
  feed(s, "[[ab]: 1");                     /* not a macro definition */
  macros_after = ands_macro_count(s);
  if (macros_before != macros_after) fail(test, "[[x]: was treated as a macro definition");
  ands_free(s);
}

static void test_string_growth(void) {
  const char *test = "string growth";
  ands_t *s = new_plain(test);
  char *chunk = (char *)malloc(40010);
  if (!s || !chunk) { fail(test, "alloc"); return; }
  chunk[0] = '['; chunk[1] = '[';
  memset(chunk + 2, 'z', 40000);
  chunk[40002] = ']'; chunk[40003] = '\0';
  feed(s, "[]");
  feed(s, chunk);                          /* 40000 */
  feed(s, chunk);                          /* 80000 > old fixed 65535 cap */
  if (ands_string_len(s) != 80000) fail(test, "append past old fixed buffer size");

  ands_set_limits(s, 0, 100000);
  feed(s, chunk);                          /* would be 120000 > ceiling */
  if (ands_string_len(s) != 80000 || !ands_overflow(s)) fail(test, "over-ceiling string append");
  ands_free(s);
  free(chunk);
}

static void test_swap_primitives(void) {
  const char *test = "swap primitives";
  ands_t *s = new_plain(test);
  double more[] = {3, 4};
  if (!s) return;

  feed(s, "[a] [b]");
  if (ands_string_concat(s) != 2) fail(test, "string concat length");
  expect_str(test, ands_string(s), "ab");
  ands_string_swap(s);                     /* swap exposes the other buffer ... */
  expect_str(test, ands_string(s), "b");
  ands_string_swap(s);                     /* ... and swapping back restores it */
  expect_str(test, ands_string(s), "ab");
  if (ands_string_appendf(s, " %d/%g", 7, 0.5) <= 0) fail(test, "appendf");
  expect_str(test, ands_string(s), "ab 7/0.5");

  feed(s, "(1 2) (3 4)");                  /* prev = 1 2, cur = 3 4 */
  if (ands_data_concat(s) != 4) fail(test, "data concat length");
  { double w[] = {1, 2, 3, 4}; expect_data(test, s, w, 4); }
  if (ands_data_append(s, more, 2) != 2) fail(test, "data append");
  { double w[] = {1, 2, 3, 4, 3, 4}; expect_data(test, s, w, 6); }
  if (!ands_data_swap(s) || ands_data_len(s) != 2) fail(test, "data swap");
  { double w[] = {3, 4}; expect_data(test, s, w, 2); }

  char ext[] = "external";
  ands_string_from_external(s, ext, 8);
  expect_str(test, ands_string(s), "external");
  ands_free(s);
}

static void test_no_leak_under_repeated_append(void) {
  const char *test = "repeated append";
  ands_t *s = new_plain(test);
  if (!s) return;
  for (int i = 0; i < 20000; i++) {
    feed(s, "((1 2 3 4 5 6 7 8)");
    feed(s, "[[abcdefgh]");
    if (i % 5000 == 4999) { feed(s, "(1)"); feed(s, "[x]"); }
  }
  /* run under ASan/LSan: any leaked growth block is reported at exit */
  ands_free(s);
}

int main(void) {
  test_compact_commands();
  test_strings_and_arrays();
  test_variables_and_nan_defaults();
  test_tie_and_rest_sentinels();
  test_comments_and_chunks();
  test_defer();
  test_defer_space();
  test_defer_space_streams();
  test_stream_variable_sigil_edges();
  test_variable_provenance();
  test_stream_defer();
  test_arrays_with_variables_and_streams();
  test_macros_basic();
  test_return_references();
  test_return_sigil_is_not_an_atom();
  test_macro_names_truncate_to_atom_width();
  test_macros_nested_and_replace();
  test_macro_definitions_do_not_break_strings();
  test_macros_are_global();
  test_macro_management_api();
  test_array_append_form();
  test_array_append_events();
  test_array_growth_and_ceiling();
  test_ceiling_applies_within_capacity();
  test_truncated_append_is_rolled_back();
  test_string_append_form();
  test_string_append_events_and_macros();
  test_string_growth();
  test_swap_primitives();
  test_no_leak_under_repeated_append();

  if (failures) {
    fprintf(stderr, "%d ANDS parser test failure(s)\n", failures);
    return 1;
  }
  printf("ANDS parser tests passed\n");
  return 0;
}
