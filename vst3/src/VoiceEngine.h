/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
enum { JV_BLOCK = 64 };
typedef struct jv_engine jv_engine;
jv_engine *jv_create(void);
void jv_destroy(jv_engine *);
void jv_reset(jv_engine *);
void jv_text(jv_engine *, const char *utf8, int language);
void jv_note_on(jv_engine *, int channel, int note, int velocity, int chord, int note_id);
void jv_note_off(jv_engine *, int channel, int note, int note_id);
void jv_panic(jv_engine *);
void jv_render(jv_engine *, float *out64);
int jv_active(jv_engine *);
#ifdef __cplusplus
}
#endif
