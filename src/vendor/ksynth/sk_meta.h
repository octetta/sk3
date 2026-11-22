#ifndef SK_META_H
#define SK_META_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define SK_MAX_CUES 64

typedef enum {
    SK_CUE_POINT = 0,
    SK_CUE_LOOP_SUSTAIN = 1,
    SK_CUE_LOOP_RELEASE = 2,
    SK_CUE_SLICE = 3,
    SK_CUE_WORD = 4
} sk_cue_type_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    uint16_t type;
    char label[32];
} sk_cue_t;

typedef enum {
    SK_WAVE_MODE_AUTO = 0,
    SK_WAVE_MODE_SAMPLE,
    SK_WAVE_MODE_OSC
} sk_wave_mode_t;

typedef struct {
    sk_wave_mode_t mode;
    float rate;
    int has_rate;
    int loop_start;
    int loop_end;
    int has_loop;
    int release_point;
    int has_release;
    int one_shot;
    int has_one_shot;
    float root_note;
    int has_root_note;
    int num_cues;
    sk_cue_t cues[SK_MAX_CUES];
} sk_meta_info_t;

static inline void sk_meta_init(sk_meta_info_t *m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->mode = SK_WAVE_MODE_AUTO;
}

static inline int sk_parse_metadata(const char *str, sk_meta_info_t *out) {
    if (!str || !out) return 0;
    sk_meta_init(out);

    int count = 0;
    const char *p = str;

    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ';')) p++;
        if (!*p) break;

        const char *key_start = p;
        while (*p && *p != ':' && *p != '=' && !isspace((unsigned char)*p)) p++;
        int key_len = (int)(p - key_start);
        if (*p != ':' && *p != '=') {
            while (*p && !isspace((unsigned char)*p)) p++;
            continue;
        }
        p++; // skip ':' or '='

        const char *val_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ';') p++;
        int val_len = (int)(p - val_start);

        char key[32];
        if (key_len >= (int)sizeof(key)) key_len = (int)sizeof(key) - 1;
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        for (int i = 0; i < key_len; i++) key[i] = (char)tolower((unsigned char)key[i]);

        char val[128];
        if (val_len >= (int)sizeof(val)) val_len = (int)sizeof(val) - 1;
        memcpy(val, val_start, val_len);
        val[val_len] = '\0';

        if (strcmp(key, "mode") == 0 || strcmp(key, "type") == 0) {
            char val_lower[32];
            int vl = val_len < 31 ? val_len : 31;
            for (int i = 0; i < vl; i++) val_lower[i] = (char)tolower((unsigned char)val[i]);
            val_lower[vl] = '\0';
            if (strcmp(val_lower, "sample") == 0) {
                out->mode = SK_WAVE_MODE_SAMPLE;
                count++;
            } else if (strcmp(val_lower, "osc") == 0 || strcmp(val_lower, "mip") == 0 || strcmp(val_lower, "cycle") == 0) {
                out->mode = SK_WAVE_MODE_OSC;
                count++;
            }
        } else if (strcmp(key, "rate") == 0 || strcmp(key, "sr") == 0) {
            float r = (float)atof(val);
            if (r > 0.0f) {
                out->rate = r;
                out->has_rate = 1;
                count++;
            }
        } else if (strcmp(key, "loop") == 0 || strcmp(key, "l") == 0) {
            char *sep = strchr(val, ',');
            if (!sep) sep = strstr(val, "..");
            if (sep) {
                *sep = '\0';
                char *end_str = (sep[0] == '.' && sep[1] == '.') ? sep + 2 : sep + 1;
                out->loop_start = atoi(val);
                out->loop_end = atoi(end_str);
                out->has_loop = 1;
                count++;
            }
        } else if (strcmp(key, "release") == 0 || strcmp(key, "rel") == 0) {
            out->release_point = atoi(val);
            out->has_release = 1;
            count++;
        } else if (strcmp(key, "one_shot") == 0 || strcmp(key, "oneshot") == 0 || strcmp(key, "os") == 0) {
            out->one_shot = atoi(val) != 0;
            out->has_one_shot = 1;
            count++;
        } else if (strcmp(key, "root") == 0 || strcmp(key, "note") == 0) {
            out->root_note = (float)atof(val);
            out->has_root_note = 1;
            count++;
        } else if (strcmp(key, "cue") == 0 || strcmp(key, "slice") == 0) {
            if (out->num_cues < SK_MAX_CUES) {
                sk_cue_t *c = &out->cues[out->num_cues];
                char *sep = strchr(val, ',');
                if (sep) {
                    *sep = '\0';
                    char *label = sep + 1;
                    char *range = strstr(val, "..");
                    if (range) {
                        *range = '\0';
                        c->start = (uint32_t)atoi(val);
                        c->end = (uint32_t)atoi(range + 2);
                    } else {
                        c->start = (uint32_t)atoi(val);
                        c->end = c->start;
                    }
                    c->type = (strcmp(key, "slice") == 0) ? SK_CUE_SLICE : SK_CUE_POINT;
                    strncpy(c->label, label, sizeof(c->label) - 1);
                    c->label[sizeof(c->label) - 1] = '\0';
                    out->num_cues++;
                    count++;
                } else {
                    c->start = (uint32_t)atoi(val);
                    c->end = c->start;
                    c->type = SK_CUE_POINT;
                    snprintf(c->label, sizeof(c->label), "cue%d", out->num_cues);
                    out->num_cues++;
                    count++;
                }
            }
        }
    }
    return count;
}

static inline int sk_format_metadata(const sk_meta_info_t *m, char *out, size_t out_sz) {
    if (!m || !out || out_sz == 0) return 0;
    out[0] = '\0';
    size_t pos = 0;

    if (m->mode == SK_WAVE_MODE_SAMPLE) {
        pos += snprintf(out + pos, out_sz - pos, "mode:sample ");
    } else if (m->mode == SK_WAVE_MODE_OSC) {
        pos += snprintf(out + pos, out_sz - pos, "mode:osc ");
    }

    if (m->has_rate && m->rate > 0) {
        pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "rate:%.0f ", m->rate);
    }

    if (m->has_one_shot) {
        pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "one_shot:%d ", m->one_shot);
    }

    if (m->has_root_note) {
        pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "root:%.0f ", m->root_note);
    }

    if (m->has_loop) {
        pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "loop:%d,%d ", m->loop_start, m->loop_end);
    }

    if (m->has_release) {
        pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "release:%d ", m->release_point);
    }

    for (int i = 0; i < m->num_cues && pos < out_sz; i++) {
        const sk_cue_t *c = &m->cues[i];
        if (c->start != c->end && c->end > 0) {
            pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "cue:%u..%u,%s ",
                            c->start, c->end, c->label[0] ? c->label : "cue");
        } else {
            pos += snprintf(out + pos, out_sz > pos ? out_sz - pos : 0, "cue:%u,%s ",
                            c->start, c->label[0] ? c->label : "cue");
        }
    }

    if (pos > 0 && pos < out_sz && out[pos - 1] == ' ') {
        out[pos - 1] = '\0';
        pos--;
    } else if (pos >= out_sz) {
        out[out_sz - 1] = '\0';
        pos = out_sz - 1;
    }
    return (int)pos;
}

#endif /* SK_META_H */
