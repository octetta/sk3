#include "synth.h"
#include "synth-internal.h"
#include "util.h"
#include "sk_dsp.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "control-events.h"

// Process a single sample through the filter - VERY FAST
// Only multiplication and addition, no transcendental functions
float mmf_process(skred_engine_t *engine, int n, float input) {
    // Calculate output using Direct Form II - only 5 multiplies, 4 adds
    float output = sv.filter[n].b0 * input +
                  sv.filter[n].b1 * sv.filter[n].x1 +
                  sv.filter[n].b2 * sv.filter[n].x2 -
                  sv.filter[n].a1 * sv.filter[n].y1 -
                  sv.filter[n].a2 * sv.filter[n].y2;
    
    // Update delay lines
    sv.filter[n].x2 = sv.filter[n].x1;
    sv.filter[n].x1 = input;
    sv.filter[n].y2 = sv.filter[n].y1;
    sv.filter[n].y1 = output;
    if (sv.filter[n].drive > 0.0f) {
      // Standard cubic soft-clip from sk_dsp: transparent near zero, compresses toward
      // the boundary, C1-continuous at the clamp so no audible kink.
      output = sk_softclip_cubic(output, sv.filter[n].drive);
    }
    
    return output;
}

