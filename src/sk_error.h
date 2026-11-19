#ifndef SK_ERROR_H
#define SK_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SK_OK                     =   0,
    SK_ERR_GENERIC            =  -1,  /* Unspecified / general failure */
    SK_ERR_INVALID_VOICE      =  -2,  /* Voice index out of range [0, voice_max) */
    SK_ERR_INVALID_PARAM      =  -3,  /* NaN, Inf, or invalid parameter value */
    SK_ERR_OUT_OF_RANGE       =  -4,  /* Value outside legal boundaries */
    SK_ERR_INVALID_WAVE       =  -5,  /* Wavetable index unallocated or out of bounds */
    SK_ERR_INVALID_BUS        =  -6,  /* Delay or FX bus index out of range */
    SK_ERR_NOT_FOUND          =  -7,  /* Resource, file, sample, or word not found */
    SK_ERR_BUFFER_FULL        =  -8,  /* Audio queue, track, or buffer overflow */
    SK_ERR_UNSUPPORTED        =  -9,  /* Opcode, mode, or hardware feature unsupported */
    SK_ERR_IO                 = -10,  /* File reading, writing, or filesystem mount failure */
    SK_ERR_PARSE              = -11,  /* Syntax error or arity mismatch */
    SK_ERR_DIV_BY_ZERO        = -12   /* Division or modulo by zero */
} sk_err_t;

/* Backward compatibility alias for legacy voice check */
#ifndef SYNTH_INVALID_VOICE
#define SYNTH_INVALID_VOICE SK_ERR_INVALID_VOICE
#endif

static inline const char *sk_strerror(int err) {
    switch (err) {
        case SK_OK:                return "Success";
        case SK_ERR_GENERIC:       return "Generic failure";
        case SK_ERR_INVALID_VOICE: return "Invalid voice index";
        case SK_ERR_INVALID_PARAM: return "Invalid parameter value";
        case SK_ERR_OUT_OF_RANGE:  return "Parameter out of range";
        case SK_ERR_INVALID_WAVE:  return "Invalid or unallocated wavetable";
        case SK_ERR_INVALID_BUS:   return "Invalid bus index";
        case SK_ERR_NOT_FOUND:     return "Item or resource not found";
        case SK_ERR_BUFFER_FULL:   return "Buffer full or capacity exceeded";
        case SK_ERR_UNSUPPORTED:   return "Operation unsupported";
        case SK_ERR_IO:            return "Input/output failure";
        case SK_ERR_PARSE:         return "Parsing or syntax error";
        case SK_ERR_DIV_BY_ZERO:   return "Division by zero";
        default:                   return "Unknown error";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* SK_ERROR_H */
