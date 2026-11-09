/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (flash) Tables of the main loop's kept packed in the image, each one LZ4 block (tools/lz4blk.py compress_small, the
 * generators check each by unpacking it): the bass archetypes' steps (gen_bass.h BA_STEP_Z), the drum maps' lanes
 * (gen_drum_maps.h GM_LANE_Z), FM6's factory patches (felucca_fm6.h FM6_FACTORY_Z). gz_get gives the table asked for,
 * unpacked by the boot's own boot_lz4 into one buffer of the pool that the last one asked for shares: the same bytes
 * the table had. Only the main loop reads them (none is a table an interrupt reaches: tools/size_fns.py isr_reach,
 * build.py's layout records them 'n'), and every reader takes what it needs before it asks for another */
#pragma once
#include <stdint.h>
#include "boot_lz4.h"
#define GZ_MAX 1046u                                    /* (the largest: BA_STEP_N) */
static uint8_t gz_buf[GZ_MAX] __attribute__((section(".pool")));
static const uint8_t *gz_cur __attribute__((section(".bss.gz")));   /* (the block unpacked there; out of the merged statics) */
static __attribute__((noinline)) const uint8_t *gz_get(const uint8_t *z, uint32_t n)
{
    if (gz_cur != z) boot_lz4(z, gz_buf, gz_buf + n);
    gz_cur = z;
    return gz_buf;
}
