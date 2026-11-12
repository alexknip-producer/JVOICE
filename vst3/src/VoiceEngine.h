/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
enum { JV_BLOCK = 64, JV_SAMPLE_RATE = 44100, JV_CONTROL_COUNT = 32 };
typedef struct jv_engine jv_engine;
jv_engine *jv_create(void);
void jv_destroy(jv_engine *);
void jv_reset(jv_engine *);
void jv_seed(jv_engine *, unsigned seed);
void jv_bend(jv_engine *, int channel, double bipolar);
void jv_mod_wheel(jv_engine *, int channel, double normalized);
void jv_sustain(jv_engine *, int channel, double normalized);
void jv_text(jv_engine *, const char *utf8, int language);
/* index: CONS rank, SHIFT, Q, BREATH, BUZZ, ARTIC; value normalized 0..1. */
void jv_control(jv_engine *, int index, double value);
void jv_note_on(jv_engine *, int channel, int note, int velocity, int chord, int note_id);
void jv_note_off(jv_engine *, int channel, int note, int note_id);
void jv_panic(jv_engine *);
void jv_render(jv_engine *, float *out64);
void jv_render_stereo(jv_engine *, float *left64, float *right64);
void jv_render_input(jv_engine *, const float *inputLeft64, const float *inputRight64, float *left64, float *right64);
int jv_active(jv_engine *);
unsigned jv_position(jv_engine *);
#ifdef __cplusplus
}
#endif
