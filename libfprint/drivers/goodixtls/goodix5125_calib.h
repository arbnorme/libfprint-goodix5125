/*
 * Goodix 27c6:5125 driver for libfprint
 * Copyright (C) 2026 The libfprint-goodix5125 contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <glib.h>

#define G5125_OTP_LEN         64
#define G5125_CHANNELS        6
#define G5125_FDT_BASE_LEN    12
#define G5125_WIDTH           64
#define G5125_HEIGHT          80
#define G5125_PIXELS          (G5125_WIDTH * G5125_HEIGHT)
#define G5125_RAW_WIDTH       88      /* the MCU sends 88 columns, 64..87 are empty */
#define G5125_RAW_PIXELS      (G5125_RAW_WIDTH * G5125_HEIGHT)
#define G5125_IMAGE_PLAIN_LEN (8 + G5125_RAW_PIXELS / 4 * 6 + 5)
#define G5125_FDT_OP_MANUAL   0x09
#define G5125_FDT_OP_DOWN     0x08
#define G5125_FDT_OP_UP       0x0a
#define G5125_IRQ_FINGER_DOWN 0x0002
#define G5125_IRQ_REVERSE     0x0080
#define G5125_IRQ_REVERSE2    0x0082
#define G5125_ENGINE_GAIN     1.5f  /* high-pass gain the Goodix engine matches best with */
#define G5125_MATCH_THRESHOLD 30    /* minimum engine score for a match */
#define G5125_TEMPLATE_VERSION 1

typedef struct
{
  guint16 dac[4];      /* values for regs 0x0220, 0x0236, 0x0238, 0x023a */
  guint16 tcode;
  guint8  delta_down;
} G5125Calib;

extern const guint16 g5125_dac_regs[4];
extern const guint8  g5125_fdt_start_base[G5125_FDT_BASE_LEN];

gboolean g5125_otp_check (const guint8 *otp,
                          gsize         len,
                          GError      **error);
void     g5125_calib_from_otp (const guint8 *otp,
                               G5125Calib   *out);
guint16  g5125_config_checksum (const guint8 *cfg,
                                gsize         len);
guint    g5125_config_patch (guint8           *cfg,
                             gsize             len,
                             const G5125Calib *calib);
gboolean g5125_fdt_channels (const guint8 *payload,
                             gsize         len,
                             guint16       raw[G5125_CHANNELS]);
gboolean g5125_fdt_base (const guint16 raw[G5125_CHANNELS],
                         guint8        base[G5125_FDT_BASE_LEN]);
guint16  g5125_irq_status (const guint8 *payload,
                           gsize         len);
gsize    g5125_fdt_payload (guint8        op,
                            const guint8  base[G5125_FDT_BASE_LEN],
                            gboolean      with_timestamp,
                            guint16       timestamp,
                            guint8       *out);
guint16  g5125_timestamp_now (void);
gboolean g5125_decode_image (const guint8 *plain,
                             gsize         len,
                             guint16       pix[G5125_PIXELS]);
void     g5125_image_to_8bit (const guint16 pix[G5125_PIXELS],
                              guint8        out[G5125_PIXELS]);
void     g5125_engine_prep (const guint16 pix[G5125_PIXELS],
                            guint8        out[G5125_PIXELS]);
GVariant *g5125_template_to_variant (const guint8 *blob,
                                     gsize         len);
gboolean g5125_template_from_variant (GVariant      *v,
                                      const guint8 **blob,
                                      gsize         *len);
