#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/api.h"
#include "../src/host_commands.h"
#include "../src/exp-vfs/skred_vfs.h"

static char g_last_output[4096];
static int g_restart_called = 0;
static int g_stop_called = 0;
static int g_start_called = 0;
static int g_clear_called = 0;
static int g_quit_called = 0;

static void test_print(void *userdata, const char *fmt, ...) {
    (void)userdata;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_output, sizeof(g_last_output), fmt, ap);
    va_end(ap);
}

static void test_restart(void *userdata, int voices, int bufsize) {
    (void)userdata;
    (void)voices;
    (void)bufsize;
    g_restart_called++;
}

static void test_stop(void *userdata) {
    (void)userdata;
    g_stop_called++;
}

static void test_start(void *userdata) {
    (void)userdata;
    g_start_called++;
}

static void test_clear(void *userdata) {
    (void)userdata;
    g_clear_called++;
}

static void test_quit(void *userdata) {
    (void)userdata;
    g_quit_called++;
}

static void reset_test_state(void) {
    g_last_output[0] = '\0';
    g_restart_called = 0;
    g_stop_called = 0;
    g_start_called = 0;
    g_clear_called = 0;
    g_quit_called = 0;
}

int main(void) {
    printf("Starting host_command_tests...\n");

    skred_host_env_t env = {
        .print = test_print,
        .restart = test_restart,
        .stop = test_stop,
        .start = test_start,
        .clear = test_clear,
        .quit = test_quit,
        .userdata = NULL
    };

    /* 1. Baseline Rule: Non-host commands return 0 */
    assert(skred_host_execute("v0n60l1", &env) == 0);
    assert(skred_host_execute("10 20 +", &env) == 0);
    assert(skred_host_execute("/w 0", &env) == 0);

    /* 2. Leading . and - are caught locally */
    reset_test_state();
    assert(skred_host_execute("-stop", &env) == 1);
    assert(g_stop_called == 1);

    reset_test_state();
    assert(skred_host_execute(".start", &env) == 1);
    assert(g_start_called == 1);

    reset_test_state();
    assert(skred_host_execute("-restart 16 256", &env) == 1);
    assert(g_restart_called == 1);

    reset_test_state();
    assert(skred_host_execute("-clear", &env) == 1);
    assert(g_clear_called == 1);

    reset_test_state();
    assert(skred_host_execute("-quit", &env) == 1);
    assert(g_quit_called == 1);

    reset_test_state();
    assert(skred_host_execute("exit", &env) == 1);
    assert(g_quit_called == 1);

    /* 3. Unrecognized . and - host commands return 1 and print error */
    reset_test_state();
    assert(skred_host_execute("-invalid_command_xyz", &env) == 1);
    assert(strstr(g_last_output, "Unknown host command") != NULL);

    reset_test_state();
    assert(skred_host_execute(".unknown_foo", &env) == 1);
    assert(strstr(g_last_output, "Unknown host command") != NULL);

    /* 4. Audio device management commands */
    reset_test_state();
    assert(skred_host_execute("-als", &env) == 1);
    assert(skred_host_execute("/als", &env) == 1);
    assert(skred_host_execute("/a?", &env) == 1);

    /* 5. MIDI device management commands */
    reset_test_state();
    assert(skred_host_execute("-mls", &env) == 1);
    assert(skred_host_execute("/mls", &env) == 1);
    assert(skred_host_execute("-mstatus", &env) == 1);

    /* 6. Staging: -add, VFS lookup, and -del */
    skred_host_staging_init();
    const char *stage_dir = skred_vfs_get_staging_dir();
    assert(stage_dir != NULL && strlen(stage_dir) > 0);

    // Create a temporary source file
    char tmp_src[256];
    snprintf(tmp_src, sizeof(tmp_src), "/tmp/test_host_add_%d.txt", (int)getpid());
    FILE *fsrc = fopen(tmp_src, "w");
    assert(fsrc != NULL);
    fprintf(fsrc, "Hello Staged World 42!\n");
    fclose(fsrc);

    // Execute -add
    char add_cmd[512];
    snprintf(add_cmd, sizeof(add_cmd), "-add %s", tmp_src);
    reset_test_state();
    assert(skred_host_execute(add_cmd, &env) == 1);
    assert(strstr(g_last_output, "Attached") != NULL);

    // Read via skred_vfs_read_file using just basename
    char bname[256];
    snprintf(bname, sizeof(bname), "test_host_add_%d.txt", (int)getpid());
    void *read_data = NULL;
    size_t read_sz = 0;
    bool read_ok = skred_vfs_read_file(bname, &read_data, &read_sz);
    assert(read_ok == true);
    assert(read_data != NULL);
    assert(read_sz > 0);
    assert(strstr((char *)read_data, "Hello Staged World 42!") != NULL);
    free(read_data);

    // Test -del
    char del_cmd[512];
    snprintf(del_cmd, sizeof(del_cmd), "-del %s", bname);
    reset_test_state();
    assert(skred_host_execute(del_cmd, &env) == 1);
    assert(strstr(g_last_output, "Removed") != NULL);

    // Verify it is no longer found
    read_data = NULL;
    read_sz = 0;
    read_ok = skred_vfs_read_file(bname, &read_data, &read_sz);
    assert(read_ok == false);

    // Cleanup temp source
    unlink(tmp_src);

    // Cleanup staging
    skred_host_staging_cleanup();
    assert(skred_vfs_get_staging_dir() == NULL || strlen(skred_vfs_get_staging_dir()) == 0);

    printf("All host_command_tests passed successfully!\n");
    return 0;
}
