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

#define FP_COMPONENT "goodixtls5125"

#include "drivers_api.h"
#include "goodix_proto.h"
#include "goodix.h"
#include "goodix5125.h"
#include "goodix5125_calib.h"

#define G5125_MAX_SOFT_ERRORS 3
#define G5125_CHIP_ID         0x2504
#define G5125_PSK_FLAGS       0xbb020003

struct _FpiDeviceGoodixTls5125
{
  FpiDeviceGoodixTls parent;

  G5125Calib         calib;
  guint8             base[G5125_FDT_BASE_LEN];
  guint8             up_base[G5125_FDT_BASE_LEN];
  guint16            clear_img[G5125_PIXELS];
  gboolean           have_clear_img;
  FpiSsm            *scan_ssm;
  gboolean           deactivating;
  guint              soft_errors;
};

G_DEFINE_TYPE (FpiDeviceGoodixTls5125, fpi_device_goodixtls5125,
               FPI_TYPE_DEVICE_GOODIXTLS)

/* 51x0 default MCU config (goodix-fp-dump, MIT; identical to the goodixtls
 * fork's goodix_511_config, LGPL). Patched per sensor from the OTP. */
static const guint8 default_config[256] = {
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

G_STATIC_ASSERT (sizeof (default_config) == 256);

static const FpIdEntry id_table[] = {
  { .vid = 0x27c6, .pid = 0x5125 },
  { .vid = 0, .pid = 0, .driver_data = 0 },
};

/* ---- activation ---------------------------------------------------------- */

enum activate_states {
  ACT_NOP,
  ACT_ENABLE_CHIP,
  ACT_NOP2,
  ACT_FIRMWARE,
  ACT_PSK,
  ACT_RESET,
  ACT_CHIP_ID,
  ACT_OTP,
  ACT_IDLE,
  ACT_DAC0,
  ACT_DAC1,
  ACT_DAC2,
  ACT_DAC3,
  ACT_CONFIG,
  ACT_SCAN_FREQ,
  ACT_TLS,
  ACT_CLEAR_FDT,
  ACT_CLEAR_IMAGE,
  ACT_NUM,
};

static void measure_fdt (FpDevice     *dev,
                         FpiSsm       *ssm,
                         const guint8 *base);

static void
on_none (FpDevice *dev, gpointer ssm, GError *error)
{
  if (error)
    fpi_ssm_mark_failed (ssm, error);
  else
    fpi_ssm_next_state (ssm);
}

static void
on_firmware (FpDevice *dev, gchar *firmware, gpointer ssm, GError *error)
{
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  fp_info ("Sensor firmware: %s", firmware);
  fpi_ssm_next_state (ssm);
}

static void
on_psk (FpDevice *dev, gboolean success, guint32 flags, guint8 *psk,
        guint16 length, gpointer ssm, GError *error)
{
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  if (!success)
    {
      fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (FP_DEVICE_ERROR_PROTO,
                                                          "Failed to read the sensor PSK status"));
      return;
    }
  /* The PSK hash format differs between firmwares, so it is not compared
   * here; a PSK that is not the all-zero key makes the TLS handshake fail. */
  fp_dbg ("PSK flags 0x%08x, length %u", flags, length);
  fpi_ssm_next_state (ssm);
}

static void
on_reset (FpDevice *dev, gboolean success, guint16 number, gpointer ssm,
          GError *error)
{
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  fp_dbg ("Reset done, number %u", number);
  fpi_ssm_next_state (ssm);
}

static void
on_chip_id (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
            GError *error)
{
  guint16 chip;

  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  chip = length >= 3 ? (guint16) (data[2] << 8 | data[1]) : 0;
  if (chip != G5125_CHIP_ID)
    {
      fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (
                             FP_DEVICE_ERROR_NOT_SUPPORTED,
                             "Unsupported Goodix chip ID 0x%04x (expected 0x%04x)",
                             chip, G5125_CHIP_ID));
      return;
    }
  fpi_ssm_next_state (ssm);
}

static void
on_otp (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
        GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  GError *err = NULL;

  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  if (!g5125_otp_check (data, length, &err))
    {
      fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (FP_DEVICE_ERROR_PROTO,
                                                          "%s", err->message));
      g_error_free (err);
      return;
    }
  g5125_calib_from_otp (data, &self->calib);
  fp_dbg ("Calibration: dac %04x %02x %02x %02x tcode 0x%x delta 0x%x",
          self->calib.dac[0], self->calib.dac[1], self->calib.dac[2],
          self->calib.dac[3], self->calib.tcode, self->calib.delta_down);
  fpi_ssm_next_state (ssm);
}

static void
on_success_step (FpDevice *dev, gboolean success, gpointer ssm, GError *error)
{
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  if (!success)
    {
      fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (
                             FP_DEVICE_ERROR_PROTO,
                             "Sensor rejected command in state %d",
                             fpi_ssm_get_cur_state (ssm)));
      return;
    }
  fpi_ssm_next_state (ssm);
}

static void
on_fdt_measured (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
                 GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  guint16 raw[G5125_CHANNELS];

  if (self->deactivating)
    {
      g_clear_error (&error);
      return;
    }
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  if (!g5125_fdt_channels (data, length, raw) || !g5125_fdt_base (raw, self->base))
    {
      if (++self->soft_errors > G5125_MAX_SOFT_ERRORS)
        {
          fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (
                                 FP_DEVICE_ERROR_PROTO,
                                 "FDT readings invalid (saturated?) %u times",
                                 self->soft_errors - 1));
          return;
        }
      fp_warn ("Invalid FDT base, re-measuring with start payload");
      measure_fdt (dev, ssm, g5125_fdt_start_base);
      return;
    }
  fp_dbg ("FDT channels %03x %03x %03x %03x %03x %03x",
          raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
  self->soft_errors = 0;
  fpi_ssm_next_state (ssm);
}

/* One FDT-manual measurement; the resulting base is stored in self->base.
 * Shared by the activation and scan state machines. */
static void
measure_fdt (FpDevice *dev, FpiSsm *ssm, const guint8 *base)
{
  guint8 *payload = g_malloc (2 + G5125_FDT_BASE_LEN);
  gsize n = g5125_fdt_payload (G5125_FDT_OP_MANUAL, base, FALSE, 0, payload);

  goodix_send_mcu_switch_to_fdt_mode (dev, payload, n, g_free,
                                      on_fdt_measured, ssm);
}

static void
on_clear_image (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
                GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  self->have_clear_img = g5125_decode_image (data, length, self->clear_img);
  if (!self->have_clear_img)
    fp_warn ("Clear image has unexpected length %u; continuing without", length);
  fpi_ssm_next_state (ssm);
}

static void
activate_ssm_run (FpiSsm *ssm, FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  switch (fpi_ssm_get_cur_state (ssm))
    {
    case ACT_NOP:
      goodix_start_read_loop (dev);
      goodix_send_nop (dev, on_none, ssm);
      break;

    case ACT_ENABLE_CHIP:
      goodix_send_enable_chip (dev, TRUE, on_none, ssm);
      break;

    case ACT_NOP2:
      goodix_send_nop (dev, on_none, ssm);
      break;

    case ACT_FIRMWARE:
      goodix_send_query_firmware_version (dev, on_firmware, ssm);
      break;

    case ACT_PSK:
      goodix_send_preset_psk_read (dev, G5125_PSK_FLAGS, 0, on_psk, ssm);
      break;

    case ACT_RESET:
      goodix_send_reset (dev, TRUE, 20, on_reset, ssm);
      break;

    case ACT_CHIP_ID:
      goodix_send_read_sensor_register (dev, 0x0000, 4, on_chip_id, ssm);
      break;

    case ACT_OTP:
      goodix_send_read_otp (dev, on_otp, ssm);
      break;

    case ACT_IDLE:
      goodix_send_mcu_switch_to_idle_mode (dev, 20, on_none, ssm);
      break;

    case ACT_DAC0:
    case ACT_DAC1:
    case ACT_DAC2:
    case ACT_DAC3:
      {
        int i = fpi_ssm_get_cur_state (ssm) - ACT_DAC0;

        goodix_send_write_sensor_register (dev, g5125_dac_regs[i],
                                           self->calib.dac[i], on_none, ssm);
        break;
      }

    case ACT_CONFIG:
      {
        guint8 *cfg = g_memdup2 (default_config, sizeof default_config);
        guint n = g5125_config_patch (cfg, sizeof default_config, &self->calib);

        fp_dbg ("Patched %u config entries", n);
        goodix_send_upload_config_mcu (dev, cfg, sizeof default_config, g_free,
                                       on_success_step, ssm);
        break;
      }

    case ACT_SCAN_FREQ:
      goodix_send_set_powerdown_scan_frequency (dev, 100, on_success_step, ssm);
      break;

    case ACT_TLS:
      goodix_tls_init (dev, on_none, ssm);
      break;

    case ACT_CLEAR_FDT:
      self->soft_errors = 0;
      measure_fdt (dev, ssm, g5125_fdt_start_base);
      break;

    case ACT_CLEAR_IMAGE:
      goodix_tls_read_image (dev, on_clear_image, ssm);
      break;
    }
}

static void
activate_complete (FpiSsm *ssm, FpDevice *dev, GError *error)
{
  if (!error)
    fp_dbg ("Activation complete");
  fpi_image_device_activate_complete (FP_IMAGE_DEVICE (dev), error);
}

/* ---- device ops ---------------------------------------------------------- */

static void
dev_init (FpImageDevice *img_dev)
{
  GError *error = NULL;

  goodix_dev_init (FP_DEVICE (img_dev), &error);
  fpi_image_device_open_complete (img_dev, error);
}

static void
dev_deinit (FpImageDevice *img_dev)
{
  GError *error = NULL;

  goodix_dev_deinit (FP_DEVICE (img_dev), &error);
  fpi_image_device_close_complete (img_dev, error);
}

static void
dev_activate (FpImageDevice *img_dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (img_dev);

  self->deactivating = FALSE;
  self->soft_errors = 0;
  self->have_clear_img = FALSE;
  fpi_ssm_start (fpi_ssm_new (FP_DEVICE (img_dev), activate_ssm_run, ACT_NUM),
                 activate_complete);
}

static void
dev_deactivate (FpImageDevice *img_dev)
{
  fpi_image_device_deactivate_complete (img_dev, NULL);
}

static void
fpi_device_goodixtls5125_init (FpiDeviceGoodixTls5125 *self)
{
}

static void
fpi_device_goodixtls5125_class_init (FpiDeviceGoodixTls5125Class *class)
{
  FpiDeviceGoodixTlsClass *gx_class = FPI_DEVICE_GOODIXTLS_CLASS (class);
  FpDeviceClass *dev_class = FP_DEVICE_CLASS (class);
  FpImageDeviceClass *img_class = FP_IMAGE_DEVICE_CLASS (class);

  gx_class->interface = 0;
  gx_class->ep_in = 0x81;
  gx_class->ep_out = 0x01;
  gx_class->get_image_cmd = 0x22;

  dev_class->id = "goodixtls5125";
  dev_class->full_name = "Goodix TLS Fingerprint Sensor 27c6:5125";
  dev_class->type = FP_DEVICE_TYPE_USB;
  dev_class->id_table = id_table;
  dev_class->scan_type = FP_SCAN_TYPE_PRESS;
  dev_class->nr_enroll_stages = 20;

  img_class->img_open = dev_init;
  img_class->img_close = dev_deinit;
  img_class->activate = dev_activate;
  img_class->deactivate = dev_deactivate;
  img_class->img_width = G5125_WIDTH;
  img_class->img_height = G5125_HEIGHT;
  img_class->bz3_threshold = 24;

  fpi_device_class_auto_initialize_features (dev_class);
}
