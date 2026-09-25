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

#include <string.h>
#include <glib.h>

#include <pthread.h>
#include <openssl/ssl.h>

#include "drivers/goodixtls/goodix5125_calib.h"
#include "drivers/goodixtls/goodix.h"
#include "drivers/goodixtls/goodix_proto.h"
#include "drivers/goodixtls/goodixtls.h"

/* Synthetic OTP: invented bytes (NOT from any real sensor) with valid
 * 51x0-layout CRCs. otp[42] = 0x6e, DAC bytes b0 b3 b1 b2. */
static const guint8 otp[64] = {
  0x0b, 0x30, 0x55, 0x7a, 0x9f, 0xc4, 0xe9, 0x0e, 0x33, 0x58, 0x7d, 0xa2, 0xc7, 0xec, 0x11, 0x36,
  0x5b, 0x80, 0xa5, 0xca, 0xef, 0x14, 0x7d, 0x5e, 0x83, 0xa8, 0xcd, 0xf2, 0x17, 0x3c, 0x61, 0x86,
  0xab, 0xd0, 0xf5, 0x1a, 0x3f, 0x64, 0x89, 0xae, 0xd3, 0xf8, 0x6e, 0x91, 0x67, 0x8c, 0xb0, 0xb3,
  0xb1, 0xb2, 0xb0, 0xb3, 0xb1, 0xb2, 0xd9, 0xfe, 0x23, 0x48, 0x6d, 0x92, 0x08, 0x0c, 0x7d, 0x1c,
};

/* 51x0 default config (goodix-fp-dump, MIT; identical to the goodixtls fork's
 * goodix_511_config). Stored checksum 0x2e89 (bytes 89 2e). */
static const guint8 config[256] = {
  0x70, 0x11, 0x60, 0x71, 0x2c, 0x9d, 0x2c, 0xc9, 0x1c, 0xe5, 0x18, 0xfd, 0x00, 0xfd, 0x00, 0xfd,
  0x03, 0xba, 0x00, 0x01, 0x80, 0xca, 0x00, 0x04, 0x00, 0x84, 0x00, 0x15, 0xb3, 0x86, 0x00, 0x00,
  0xc4, 0x88, 0x00, 0x00, 0xba, 0x8a, 0x00, 0x00, 0xb2, 0x8c, 0x00, 0x00, 0xaa, 0x8e, 0x00, 0x00,
  0xc1, 0x90, 0x00, 0xbb, 0xbb, 0x92, 0x00, 0xb1, 0xb1, 0x94, 0x00, 0x00, 0xa8, 0x96, 0x00, 0x00,
  0xb6, 0x98, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00, 0xd2, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00,
  0x00, 0xd6, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x50, 0x00, 0x01, 0x05, 0xd0, 0x00, 0x00,
  0x00, 0x70, 0x00, 0x00, 0x00, 0x72, 0x00, 0x78, 0x56, 0x74, 0x00, 0x34, 0x12, 0x20, 0x00, 0x10,
  0x40, 0x2a, 0x01, 0x02, 0x04, 0x22, 0x00, 0x01, 0x20, 0x24, 0x00, 0x32, 0x00, 0x80, 0x00, 0x01,
  0x00, 0x5c, 0x00, 0x80, 0x00, 0x56, 0x00, 0x04, 0x20, 0x58, 0x00, 0x03, 0x02, 0x32, 0x00, 0x0c,
  0x02, 0x66, 0x00, 0x03, 0x00, 0x7c, 0x00, 0x00, 0x58, 0x82, 0x00, 0x80, 0x15, 0x2a, 0x01, 0x82,
  0x03, 0x22, 0x00, 0x01, 0x20, 0x24, 0x00, 0x14, 0x00, 0x80, 0x00, 0x01, 0x00, 0x5c, 0x00, 0x00,
  0x01, 0x56, 0x00, 0x04, 0x20, 0x58, 0x00, 0x03, 0x02, 0x32, 0x00, 0x0c, 0x02, 0x66, 0x00, 0x03,
  0x00, 0x7c, 0x00, 0x00, 0x58, 0x82, 0x00, 0x80, 0x1f, 0x2a, 0x01, 0x08, 0x00, 0x5c, 0x00, 0x80,
  0x00, 0x54, 0x00, 0x10, 0x01, 0x62, 0x00, 0x04, 0x03, 0x64, 0x00, 0x19, 0x00, 0x66, 0x00, 0x03,
  0x00, 0x7c, 0x00, 0x01, 0x58, 0x2a, 0x01, 0x08, 0x00, 0x5c, 0x00, 0x00, 0x01, 0x52, 0x00, 0x08,
  0x00, 0x54, 0x00, 0x00, 0x01, 0x66, 0x00, 0x03, 0x00, 0x7c, 0x00, 0x01, 0x58, 0x00, 0x89, 0x2e,
};

static void
test_otp_check_ok (void)
{
  g_autoptr(GError) error = NULL;

  g_assert_true (g5125_otp_check (otp, sizeof otp, &error));
  g_assert_no_error (error);
}

static void
test_otp_check_detects_corruption (void)
{
  g_autoptr(GError) error = NULL;
  guint8 bad[64];

  memcpy (bad, otp, sizeof bad);
  bad[46] ^= 0x01;            /* flips a DAC byte: DAC-MT CRC must fail */
  g_assert_false (g5125_otp_check (bad, sizeof bad, &error));
  g_assert_nonnull (error);
}

static void
test_otp_check_short (void)
{
  g_autoptr(GError) error = NULL;

  g_assert_false (g5125_otp_check (otp, 32, &error));
  g_assert_nonnull (error);
}

static void
test_calib_from_otp (void)
{
  G5125Calib c;

  g5125_calib_from_otp (otp, &c);
  g_assert_cmpuint (c.dac[0], ==, 0x0b08);
  g_assert_cmpuint (c.dac[1], ==, 0x00b3);
  g_assert_cmpuint (c.dac[2], ==, 0x00b1);
  g_assert_cmpuint (c.dac[3], ==, 0x00b2);
  g_assert_cmpuint (c.tcode, ==, 0xb0);
  g_assert_cmpuint (c.delta_down, ==, 0x30);
}

static void
test_config_checksum_original (void)
{
  g_assert_cmpuint (g5125_config_checksum (config, sizeof config), ==, 0x2e89);
}

static void
test_config_patch (void)
{
  G5125Calib c;
  guint8 cfg[256];

  memcpy (cfg, config, sizeof cfg);
  g5125_calib_from_otp (otp, &c);
  g_assert_cmpuint (g5125_config_patch (cfg, sizeof cfg, &c), ==, 3);
  /* section 2: 0x5c at offset 173, 0x82 at 197; section 4: 0x5c at 233 */
  g_assert_cmpuint (cfg[175] | cfg[176] << 8, ==, 0x00b0);
  g_assert_cmpuint (cfg[199] | cfg[200] << 8, ==, 0x3080);
  g_assert_cmpuint (cfg[235] | cfg[236] << 8, ==, 0x00b0);
  /* untouched half-tcode / other delta entries */
  g_assert_cmpuint (cfg[131] | cfg[132] << 8, ==, 0x0080);
  g_assert_cmpuint (cfg[155] | cfg[156] << 8, ==, 0x1580);
  /* checksum rewritten */
  g_assert_cmpuint (cfg[254], ==, 0x7a);
  g_assert_cmpuint (cfg[255], ==, 0xce);
  g_assert_cmpuint (g5125_config_checksum (cfg, sizeof cfg), ==, 0xce7a);
}

/* Synthetic FDT-mode reply payload (after the cmd/len header):
 * IrqStatus 0x0100, 0x0000, then 6 LE channels. */
static const guint8 fdt_reply[16] = {
  0x00, 0x01, 0x00, 0x00, 0x50, 0x01, 0x70, 0x01, 0x40, 0x01, 0x60, 0x01, 0x44, 0x01, 0x58, 0x01,
};

static void
test_fdt_channels_and_base (void)
{
  guint16 raw[6];
  guint8 base[12];
  static const guint8 want[12] = { 0x80, 0xa8, 0x80, 0xb8, 0x80, 0xa0, 0x80, 0xb0, 0x80, 0xa2, 0x80, 0xac };

  g_assert_true (g5125_fdt_channels (fdt_reply, sizeof fdt_reply, raw));
  g_assert_cmpuint (raw[0], ==, 0x150);
  g_assert_cmpuint (raw[5], ==, 0x158);
  g_assert_true (g5125_fdt_base (raw, base));
  g_assert_cmpmem (base, 12, want, 12);
}

static void
test_fdt_channels_short (void)
{
  guint16 raw[6];

  g_assert_false (g5125_fdt_channels (fdt_reply, 10, raw));
}

static void
test_fdt_base_rejects_saturated (void)
{
  guint16 raw[6] = { 0x150, 0x170, 0x1ff, 0x160, 0x144, 0x158 };
  guint8 base[12];

  g_assert_false (g5125_fdt_base (raw, base));
  raw[2] = 0x001;             /* raw >> 1 == 0x00 is invalid as well */
  g_assert_false (g5125_fdt_base (raw, base));
}

static void
test_irq_status (void)
{
  static const guint8 ev_down[4] = { 0x02, 0x00, 0x3f, 0x00 };
  static const guint8 ev_rev[4] = { 0x80, 0x00, 0x00, 0x00 };

  g_assert_cmpuint (g5125_irq_status (ev_down, 4), ==, G5125_IRQ_FINGER_DOWN);
  g_assert_cmpuint (g5125_irq_status (ev_rev, 4), ==, G5125_IRQ_REVERSE);
  g_assert_cmpuint (g5125_irq_status (ev_down, 1), ==, 0xffff);
}

static void
test_fdt_payload (void)
{
  static const guint8 base[12] = { 0x80, 0xa8, 0x80, 0xb8, 0x80, 0xa0, 0x80, 0xb0, 0x80, 0xa2, 0x80, 0xac };
  /* op, 0x01, base, timestamp little-endian */
  static const guint8 want_down[16] = { 0x08, 0x01, 0x80, 0xa8, 0x80, 0xb8, 0x80, 0xa0,
                                        0x80, 0xb0, 0x80, 0xa2, 0x80, 0xac, 0x34, 0x12 };
  guint8 out[16];

  g_assert_cmpuint (g5125_fdt_payload (G5125_FDT_OP_DOWN, base, TRUE, 0x1234, out), ==, 16);
  g_assert_cmpmem (out, 16, want_down, 16);
  g_assert_cmpuint (g5125_fdt_payload (G5125_FDT_OP_UP, base, FALSE, 0, out), ==, 14);
  g_assert_cmpuint (out[0], ==, 0x0a);
  g_assert_cmpuint (out[1], ==, 0x01);
}

static void
test_timestamp_range (void)
{
  g_assert_cmpuint (g5125_timestamp_now (), <, 60000);
}

/* Inverse of the sensor's 12-bit packing: 4 values -> 6 bytes. */
static void
pack4 (const guint16 p[4], guint8 d[6])
{
  d[0] = ((p[0] >> 8) & 0x0f) | ((p[1] & 0x0f) << 4);
  d[1] = p[0] & 0xff;
  d[2] = p[2] & 0xff;
  d[3] = p[1] >> 4;
  d[4] = p[3] >> 4;
  d[5] = ((p[2] >> 8) & 0x0f) | ((p[3] & 0x0f) << 4);
}

static void
test_decode_image_bit_layout (void)
{
  g_autofree guint8 *plain = g_malloc0 (G5125_IMAGE_PLAIN_LEN);
  guint16 pix[G5125_PIXELS];
  static const guint8 chunk[6] = { 0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb };

  for (gsize i = 0; i < G5125_RAW_PIXELS / 4; i++)
    memcpy (plain + 8 + i * 6, chunk, 6);

  g_assert_true (g5125_decode_image (plain, G5125_IMAGE_PLAIN_LEN, pix));
  g_assert_cmpuint (pix[0], ==, 0x143);   /* ((0x21&0xf)<<8) + 0x43 */
  g_assert_cmpuint (pix[1], ==, 0x872);   /* (0x87<<4) + (0x21>>4) */
  g_assert_cmpuint (pix[2], ==, 0xb65);   /* ((0xcb&0xf)<<8) + 0x65 */
  g_assert_cmpuint (pix[3], ==, 0xa9c);   /* (0xa9<<4) + (0xcb>>4) */
}

static void
test_decode_image_crops_to_64 (void)
{
  g_autofree guint8 *plain = g_malloc0 (G5125_IMAGE_PLAIN_LEN);
  guint16 raw[G5125_RAW_PIXELS];
  guint16 pix[G5125_PIXELS];

  for (int y = 0; y < G5125_HEIGHT; y++)
    for (int x = 0; x < G5125_RAW_WIDTH; x++)
      raw[y * G5125_RAW_WIDTH + x] = x < G5125_WIDTH ? (guint16) (y * 40 + x) : 0xfff;
  for (int i = 0; i < G5125_RAW_PIXELS / 4; i++)
    pack4 (raw + 4 * i, plain + 8 + 6 * i);

  g_assert_true (g5125_decode_image (plain, G5125_IMAGE_PLAIN_LEN, pix));
  for (int y = 0; y < G5125_HEIGHT; y++)
    for (int x = 0; x < G5125_WIDTH; x++)
      g_assert_cmpuint (pix[y * G5125_WIDTH + x], ==, y * 40 + x);
}

static void
test_decode_image_wrong_length (void)
{
  guint8 plain[100] = { 0 };
  guint16 pix[G5125_PIXELS];

  g_assert_false (g5125_decode_image (plain, sizeof plain, pix));
}

static void
test_image_to_8bit (void)
{
  guint16 pix[G5125_PIXELS];
  guint8 out[G5125_PIXELS];

  /* ridges (low raw values) come out bright */
  for (int i = 0; i < G5125_PIXELS; i++)
    pix[i] = i % 2 ? 1000 : 1400;
  g5125_image_to_8bit (pix, out);
  g_assert_cmpuint (out[1], ==, 255);
  g_assert_cmpuint (out[0], ==, 0);

  for (int i = 0; i < G5125_PIXELS; i++)
    pix[i] = 1000;             /* flat image must not divide by zero */
  g5125_image_to_8bit (pix, out);
  g_assert_cmpuint (out[0], ==, 0);
}

static void
test_image_to_8bit_ignores_outliers (void)
{
  guint16 pix[G5125_PIXELS];
  guint8 out[G5125_PIXELS];

  for (int i = 0; i < G5125_PIXELS; i++)
    pix[i] = i % 2 ? 1000 : 1400;
  pix[2] = 4095;               /* one hot pixel must not squash the contrast */
  pix[4] = 0;                  /* nor one dead pixel */
  g5125_image_to_8bit (pix, out);
  g_assert_cmpuint (out[1], ==, 255);
  g_assert_cmpuint (out[0], ==, 0);
  g_assert_cmpuint (out[2], ==, 0);
  g_assert_cmpuint (out[4], ==, 255);
}

static void
test_tls_records_complete (void)
{
  /* ChangeCipherSpec record, then a Finished header announcing 0x28 bytes */
  static const guint8 ccs[6] = { 0x14, 0x03, 0x03, 0x00, 0x01, 0x01 };
  guint8 buf[6 + 5 + 0x28];

  g_assert_true (goodix_tls_records_complete (ccs, sizeof ccs));
  g_assert_false (goodix_tls_records_complete (ccs, 3));
  g_assert_false (goodix_tls_records_complete (ccs, 0));

  memcpy (buf, ccs, 6);
  buf[6] = 0x16; buf[7] = 0x03; buf[8] = 0x03; buf[9] = 0x00; buf[10] = 0x28;
  memset (buf + 11, 0xaa, 0x28);
  g_assert_false (goodix_tls_records_complete (buf, 6 + 5 + 10));   /* partial Finished */
  g_assert_true (goodix_tls_records_complete (buf, sizeof buf));
}

static void
test_goodix_error_domain (void)
{
  g_autoptr(GError) e = g_error_new (GOODIX_ERROR, GOODIX_ERROR_TLS_RECONNECT, "x");

  g_assert_true (g_error_matches (e, GOODIX_ERROR, GOODIX_ERROR_TLS_RECONNECT));
}

static void
test_pack_total_len (void)
{
  /* ACK pack (4-byte header + 6 payload bytes) directly followed by the
   * start of an FDT reply pack, as they can arrive in one USB transfer. */
  static const guint8 two[] = {
    0xa0, 0x06, 0x00, 0xa6, 0xb0, 0x03, 0x00, 0x36, 0x01, 0xe0,
    0xa0, 0x14, 0x00, 0xb4, 0x36, 0x11, 0x00,
  };

  g_assert_cmpuint (goodix_pack_total_len (two, sizeof two), ==, 10);
  g_assert_cmpuint (goodix_pack_total_len (two + 10, sizeof two - 10), ==, 0);  /* incomplete */
  g_assert_cmpuint (goodix_pack_total_len (two, 3), ==, 0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/goodix5125/otp/check-ok", test_otp_check_ok);
  g_test_add_func ("/goodix5125/otp/check-corrupt", test_otp_check_detects_corruption);
  g_test_add_func ("/goodix5125/otp/check-short", test_otp_check_short);
  g_test_add_func ("/goodix5125/otp/calib", test_calib_from_otp);
  g_test_add_func ("/goodix5125/config/checksum", test_config_checksum_original);
  g_test_add_func ("/goodix5125/config/patch", test_config_patch);
  g_test_add_func ("/goodix5125/fdt/channels-base", test_fdt_channels_and_base);
  g_test_add_func ("/goodix5125/fdt/channels-short", test_fdt_channels_short);
  g_test_add_func ("/goodix5125/fdt/base-rejects-saturated", test_fdt_base_rejects_saturated);
  g_test_add_func ("/goodix5125/fdt/irq-status", test_irq_status);
  g_test_add_func ("/goodix5125/fdt/payload", test_fdt_payload);
  g_test_add_func ("/goodix5125/fdt/timestamp", test_timestamp_range);
  g_test_add_func ("/goodix5125/image/decode-bit-layout", test_decode_image_bit_layout);
  g_test_add_func ("/goodix5125/image/decode-crops-to-64", test_decode_image_crops_to_64);
  g_test_add_func ("/goodix5125/image/decode-wrong-length", test_decode_image_wrong_length);
  g_test_add_func ("/goodix5125/image/to-8bit", test_image_to_8bit);
  g_test_add_func ("/goodix5125/image/to-8bit-outliers", test_image_to_8bit_ignores_outliers);
  g_test_add_func ("/goodix5125/tls/records-complete", test_tls_records_complete);
  g_test_add_func ("/goodix5125/transport/error-domain", test_goodix_error_domain);
  g_test_add_func ("/goodix5125/transport/pack-total-len", test_pack_total_len);
  return g_test_run ();
}
