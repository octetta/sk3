#include "api.h"
#include "skode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <math.h>

int main(void) {
    printf("Starting Binary Opcode Protocol (BOP) Test Suite...\n");

    /* -------------------------------------------------------------
     * TEST 1: STRUCT MEMORY LAYOUT & CACHE-LINE ALIGNMENT
     * ------------------------------------------------------------- */
    printf("  [1] Verifying 40-byte cache-line layout and byte offsets...\n");
    assert(sizeof(skred_opcode_msg_t) == 40);
    assert(offsetof(skred_opcode_msg_t, opcode) == 0);
    assert(offsetof(skred_opcode_msg_t, voice) == 2);
    assert(offsetof(skred_opcode_msg_t, argc) == 4);
    assert(offsetof(skred_opcode_msg_t, flags) == 5);
    assert(offsetof(skred_opcode_msg_t, reserved) == 6);
    assert(offsetof(skred_opcode_msg_t, args) == 8);
    printf("      sizeof(skred_opcode_msg_t) = %zu (exact 40 bytes, fits L1 cache line)\n", sizeof(skred_opcode_msg_t));

    /* -------------------------------------------------------------
     * TEST 2: ENGINE INITIALIZATION
     * ------------------------------------------------------------- */
    printf("  [2] Starting headless audio engine (16 voices, 48kHz)...\n");
    int r = skred_start_headless(16, 48000);
    assert(r == 0);

    /* -------------------------------------------------------------
     * TEST 3: DIRECT OPCODE DISPATCH (skred_dispatch_opcode)
     * ------------------------------------------------------------- */
    printf("  [3] Testing direct opcode dispatch (skred_dispatch_opcode)...\n");
    float f440 = 440.0f;
    r = skred_dispatch_opcode(0, SKODE_OP_FREQ, &f440, 1);
    assert(r == 0);

    float a08 = 0.8f;
    r = skred_dispatch_opcode(0, SKODE_OP_AMP, &a08, 1);
    assert(r == 0);

    float k1500 = 1500.0f;
    r = skred_dispatch_opcode(0, SKODE_OP_FILTER_FREQ, &k1500, 1);
    assert(r == 0);

    float gate1 = 1.0f;
    r = skred_dispatch_opcode(0, SKODE_OP_TRIGGER, &gate1, 1);
    assert(r == 0);

    /* Test error conditions */
    r = skred_dispatch_opcode(0, 9999, &f440, 1); /* Invalid opcode */
    assert(r < 0);

    r = skred_dispatch_opcode(99, SKODE_OP_FREQ, &f440, 1); /* Invalid voice */
    assert(r < 0);

    float too_many_args[10] = {0};
    r = skred_dispatch_opcode(0, SKODE_OP_FREQ, too_many_args, 9); /* Exceeds max arity */
    assert(r < 0);
    printf("      Direct opcode dispatch validated with parameter verification.\n");

    /* -------------------------------------------------------------
     * TEST 4: WORD NAME DISPATCH (skred_dispatch_word)
     * ------------------------------------------------------------- */
    printf("  [4] Testing word name dispatch (skred_dispatch_word)...\n");
    float f880 = 880.0f;
    r = skred_dispatch_word(1, "f", &f880, 1);
    assert(r == 0);

    r = skred_dispatch_word(1, "freq", &f880, 1);
    assert(r == 0);

    float a05 = 0.5f;
    r = skred_dispatch_word(1, "a", &a05, 1);
    assert(r == 0);

    r = skred_dispatch_word(1, "l", &gate1, 1);
    assert(r == 0);

    /* Test nonexistent word */
    r = skred_dispatch_word(1, "non_existent_xyz", &gate1, 1);
    assert(r < 0);
    printf("      Word and alias dispatching succeeded.\n");

    /* -------------------------------------------------------------
     * TEST 5: MESSAGE STRUCT DISPATCH (skred_dispatch_msg)
     * ------------------------------------------------------------- */
    printf("  [5] Testing single message dispatch (skred_dispatch_msg)...\n");
    skred_opcode_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.opcode = SKODE_OP_FREQ;
    msg.voice = 2;
    msg.argc = 1;
    msg.args[0] = 660.0f;
    r = skred_dispatch_msg(&msg);
    assert(r == 0);
    printf("      Single message dispatch OK.\n");

    /* -------------------------------------------------------------
     * TEST 6: BATCH DISPATCH - PACKED VARIABLE LENGTH WIRE FRAMING
     * ------------------------------------------------------------- */
    printf("  [6] Testing batch dispatch with variable-length wire framing (8 + 4*N bytes)...\n");
    /* Create a buffer with 4 variable-length records back-to-back */
    uint8_t wire_buf[128];
    uint8_t *wptr = wire_buf;

    /* Record 1: Voice 3, SKODE_OP_FREQ, argc 1 (12 bytes) */
    skred_opcode_msg_t m1;
    memset(&m1, 0, sizeof(m1));
    m1.opcode = SKODE_OP_FREQ;
    m1.voice = 3;
    m1.argc = 1;
    m1.args[0] = 523.25f;
    memcpy(wptr, &m1, 8);
    memcpy(wptr + 8, m1.args, 4);
    wptr += 12;

    /* Record 2: Voice 3, SKODE_OP_AMP, argc 1 (12 bytes) */
    skred_opcode_msg_t m2;
    memset(&m2, 0, sizeof(m2));
    m2.opcode = SKODE_OP_AMP;
    m2.voice = 3;
    m2.argc = 1;
    m2.args[0] = 0.95f;
    memcpy(wptr, &m2, 8);
    memcpy(wptr + 8, m2.args, 4);
    wptr += 12;

    /* Record 3: Voice 3, SKODE_OP_FILTER_FREQ, argc 1 (12 bytes) */
    skred_opcode_msg_t m3;
    memset(&m3, 0, sizeof(m3));
    m3.opcode = SKODE_OP_FILTER_FREQ;
    m3.voice = 3;
    m3.argc = 1;
    m3.args[0] = 3000.0f;
    memcpy(wptr, &m3, 8);
    memcpy(wptr + 8, m3.args, 4);
    wptr += 12;

    /* Record 4: Voice 3, SKODE_OP_TRIGGER, argc 1 (12 bytes) */
    skred_opcode_msg_t m4;
    memset(&m4, 0, sizeof(m4));
    m4.opcode = SKODE_OP_TRIGGER;
    m4.voice = 3;
    m4.argc = 1;
    m4.args[0] = 1.0f;
    memcpy(wptr, &m4, 8);
    memcpy(wptr + 8, m4.args, 4);
    wptr += 12;

    size_t total_wire_bytes = (size_t)(wptr - wire_buf);
    assert(total_wire_bytes == 48);

    int processed = 0;
    r = skred_dispatch_batch(wire_buf, total_wire_bytes, &processed);
    assert(r == 0);
    assert(processed == 4);
    printf("      Variable-length batch dispatch processed %d opcodes successfully (48 wire bytes).\n", processed);

    /* -------------------------------------------------------------
     * TEST 7: BATCH DISPATCH - FIXED 40-BYTE STRIDE FORMAT
     * ------------------------------------------------------------- */
    printf("  [7] Testing batch dispatch with fixed 40-byte stride (SKRED_BOP_FLAG_FIXED)...\n");
    skred_opcode_msg_t fixed_batch[3];
    memset(fixed_batch, 0, sizeof(fixed_batch));

    fixed_batch[0].opcode = SKODE_OP_FREQ;
    fixed_batch[0].voice = 4;
    fixed_batch[0].argc = 1;
    fixed_batch[0].flags = SKRED_BOP_FLAG_FIXED;
    fixed_batch[0].args[0] = 330.0f;

    fixed_batch[1].opcode = SKODE_OP_AMP;
    fixed_batch[1].voice = 4;
    fixed_batch[1].argc = 1;
    fixed_batch[1].flags = SKRED_BOP_FLAG_FIXED;
    fixed_batch[1].args[0] = 0.7f;

    fixed_batch[2].opcode = SKODE_OP_TRIGGER;
    fixed_batch[2].voice = 4;
    fixed_batch[2].argc = 1;
    fixed_batch[2].flags = SKRED_BOP_FLAG_FIXED;
    fixed_batch[2].args[0] = 1.0f;

    processed = 0;
    r = skred_dispatch_batch(fixed_batch, sizeof(fixed_batch), &processed);
    assert(r == 0);
    assert(processed == 3);
    printf("      Fixed 40-byte batch dispatch processed %d opcodes successfully.\n", processed);

    /* -------------------------------------------------------------
     * TEST 8: BROADCAST DISPATCH (voice = -1)
     * ------------------------------------------------------------- */
    printf("  [8] Testing voice broadcast (voice = -1)...\n");
    float master_cutoff = 2200.0f;
    r = skred_dispatch_opcode(-1, SKODE_OP_FILTER_FREQ, &master_cutoff, 1);
    assert(r == 0);
    printf("      Broadcast to all 16 voices executed without error.\n");

    /* -------------------------------------------------------------
     * TEST 9: UP TO 8 FLOAT ARGUMENTS
     * ------------------------------------------------------------- */
    printf("  [9] Testing opcode with multiple float arguments (up to 8)...\n");
    float multi_args[7] = {1.0f, 0.5f, 0.3f, 0.2f, 0.1f, 0.05f, 0.02f};
    r = skred_dispatch_opcode(0, SKODE_OP_DELAY_PARAMS, multi_args, 7);
    assert(r == 0);
    printf("      Multi-argument opcode (7 floats) dispatched cleanly.\n");

    /* -------------------------------------------------------------
     * TEST 10: AUDIO RENDERING VERIFICATION
     * ------------------------------------------------------------- */
    printf("  [10] Rendering audio buffer from BOP-triggered voices...\n");
    float out_left[256];
    float out_right[256];
    memset(out_left, 0, sizeof(out_left));
    memset(out_right, 0, sizeof(out_right));

    skred_process_stereo(out_left, out_right, 256);

    /* Check that audio was generated (non-silent buffer) */
    double energy = 0.0;
    for (int i = 0; i < 256; i++) {
        energy += out_left[i] * out_left[i] + out_right[i] * out_right[i];
    }
    assert(energy > 0.0);
    printf("       Generated audio energy = %g (non-silent rendered audio verified!)\n", energy);

    /* -------------------------------------------------------------
     * CLEANUP
     * ------------------------------------------------------------- */
    skred_stop();
    printf("All Binary Opcode Protocol (BOP) tests passed successfully!\n");
    return 0;
}
