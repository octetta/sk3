#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ands.h"
#include "skode.h"
#include "skode-internal.h"

/* Append forms ("((", "[[") and the swap / argument-list words, driven through
   the real Skode dispatcher (no audio device needed). */

static int failures = 0;
static char captured[4096];

static int capture_printf(skode_t *w, const char *fmt, ...) {
  va_list ap;
  size_t used = strlen(captured);
  (void)w;
  va_start(ap, fmt);
  vsnprintf(captured + used, sizeof(captured) - used, fmt, ap);
  va_end(ap);
  return 0;
}

static void fail(const char *test, const char *msg) {
  fprintf(stderr, "FAIL %s: %s\n", test, msg);
  failures++;
}

static void run(const char *test, skode_t *ctx, const char *line) {
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s", line);
  if (skode_consume(buf, ctx) != 0) {
    char msg[200];
    snprintf(msg, sizeof(msg), "skode_consume failed for [%s]", line);
    fail(test, msg);
  }
}

static void want_data(const char *test, skode_t *ctx, const double *w, int n) {
  ands_t *p = ctx->parse;
  char msg[200];
  if (ands_data_len(p) != n) {
    snprintf(msg, sizeof(msg), "data len expected %d, got %d", n, ands_data_len(p));
    fail(test, msg);
    return;
  }
  for (int i = 0; i < n; i++) {
    if (fabs(ands_data(p)[i] - w[i]) > 1e-9) {
      snprintf(msg, sizeof(msg), "data[%d] expected %g, got %g", i, w[i], ands_data(p)[i]);
      fail(test, msg);
      return;
    }
  }
}

static void want_str(const char *test, skode_t *ctx, const char *w) {
  if (strcmp(ands_string(ctx->parse), w) != 0) {
    char msg[300];
    snprintf(msg, sizeof(msg), "string expected [%s], got [%s]", w, ands_string(ctx->parse));
    fail(test, msg);
  }
}

static void test_append_forms_and_dlen(void) {
  const char *t = "append forms + dlen";
  skode_t ctx = SKODE_EMPTY();
  skode_init(&ctx);
  ctx.printf = capture_printf;

  run(t, &ctx, "((1 2 3)");
  run(t, &ctx, "((4 5)");
  { double w[] = {1, 2, 3, 4, 5}; want_data(t, &ctx, w, 5); }
  run(t, &ctx, "dlen");
  if (!strstr(captured, "# D len=5")) fail(t, "dlen did not report the length");

  run(t, &ctx, "((6 7 8.");                 /* truncated piece: nothing applied */
  { double w[] = {1, 2, 3, 4, 5}; want_data(t, &ctx, w, 5); }

  run(t, &ctx, "(10 20) (30)");
  run(t, &ctx, "d>>d");
  { double w[] = {10, 20, 30}; want_data(t, &ctx, w, 3); }
  run(t, &ctx, "d<>d");
  { double w[] = {30}; want_data(t, &ctx, w, 1); }
}

static void test_string_words(void) {
  const char *t = "string words";
  skode_t ctx = SKODE_EMPTY();
  skode_init(&ctx);

  run(t, &ctx, "[a] [b] s>>s");
  want_str(t, &ctx, "ab");
  run(t, &ctx, "[[cd]");
  want_str(t, &ctx, "abcd");
  run(t, &ctx, "[x] [y] s<>s");
  want_str(t, &ctx, "x");
  run(t, &ctx, "[so far] [ @0 and @1] 3 4 fmt>>s");
  want_str(t, &ctx, "so far 3 and 4");
}

static void test_array_argument_words(void) {
  const char *t = "array <-> argument words";
  skode_t ctx = SKODE_EMPTY();
  skode_init(&ctx);

  run(t, &ctx, "(1 2 3) d>a a>>d");         /* d>a keeps the list for the next word */
  { double w[] = {1, 2, 3, 1, 2, 3}; want_data(t, &ctx, w, 6); }

  run(t, &ctx, "(5 6 7 8) 1 2 ds>a a>>d");  /* slice: values 6 7 */
  { double w[] = {5, 6, 7, 8, 6, 7}; want_data(t, &ctx, w, 6); }

  run(t, &ctx, "9 8 a>d");                  /* args become the array */
  { double w[] = {9, 8}; want_data(t, &ctx, w, 2); }

  run(t, &ctx, "(1 2 3 4 5 6 7 8 9 10) d>a a>d");  /* more than the argument limit */
  if (ands_data_len(ctx.parse) > 8) fail(t, "argument list exceeded its limit");
}

static void test_limits(void) {
  const char *t = "limits";
  skode_t ctx = SKODE_EMPTY();
  skode_init(&ctx);
  run(t, &ctx, "()");                       /* the parser is created on first use */
  ands_set_limits(ctx.parse, 10, 0);
  run(t, &ctx, "(1 2 3)");
  run(t, &ctx, "((4 5 6 7 8 9 10 11 12 13 14 15)");   /* would pass the ceiling */
  { double w[] = {1, 2, 3}; want_data(t, &ctx, w, 3); }
  run(t, &ctx, "((4 5)");
  { double w[] = {1, 2, 3, 4, 5}; want_data(t, &ctx, w, 5); }
}

#ifdef KSYNTH
static void test_ksynth_append(void) {
  const char *t = "k>>d";
  skode_t ctx = SKODE_EMPTY();
  skode_init(&ctx);
  run(t, &ctx, "[1 2 3] k!");
  run(t, &ctx, "k>>d");
  run(t, &ctx, "k>>d");
  { double w[] = {1, 2, 3, 1, 2, 3}; want_data(t, &ctx, w, 6); }
  run(t, &ctx, "k>d");                      /* existing word still replaces */
  { double w[] = {1, 2, 3}; want_data(t, &ctx, w, 3); }
}
#endif

int main(void) {
  test_append_forms_and_dlen();
  test_string_words();
  test_array_argument_words();
  test_limits();
#ifdef KSYNTH
  test_ksynth_append();
#endif
  if (failures) {
    fprintf(stderr, "%d append/swap word test failure(s)\n", failures);
    return 1;
  }
  printf("append/swap word tests passed\n");
  return 0;
}
