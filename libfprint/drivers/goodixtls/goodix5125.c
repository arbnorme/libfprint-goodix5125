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
#include "goodix_engine.h"

#define G5125_MAX_SOFT_ERRORS 3
#define G5125_CHIP_ID         0x2504
#define G5125_PSK_FLAGS       0xbb020003
#define G5125_ENGINE_DEFAULT_PATH "/usr/lib64/libfprint-2/goodix5125/AlgoChicago.dll"

typedef enum {
  G5125_ACTION_NONE,
  G5125_ACTION_ENROLL,
  G5125_ACTION_VERIFY,
  G5125_ACTION_IDENTIFY,
} G5125Action;

struct _FpiDeviceGoodixTls5125
{
  FpiDeviceGoodixTls parent;

  G5125Calib         calib;
  guint8             base[G5125_FDT_BASE_LEN];
  guint8             up_base[G5125_FDT_BASE_LEN];
  FpiSsm            *act_ssm;
  FpiSsm            *scan_ssm;
  gboolean           deactivating;   /* an action is being cancelled or suspended */
  gboolean           cancelling;
  gboolean           suspending;
  guint              soft_errors;

  G5125Action        action;
  void              *enroll_ctx;
  gint               enroll_stage;
  gint               enroll_progress;
  gboolean           frame_rejected;
  FpiMatchResult     result;
  FpPrint           *identified;
  GError            *result_error;
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
  ACT_FDT,
  ACT_NUM,
};

static void measure_fdt (FpDevice     *dev,
                         FpiSsm       *ssm,
                         const guint8 *base);
static gboolean scan_stop_if_deactivating (FpiDeviceGoodixTls5125 *self,
                                           FpiSsm                 *ssm,
                                           GError                **error);

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

  if (scan_stop_if_deactivating (self, ssm, &error))
    return;
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

    case ACT_FDT:
      self->soft_errors = 0;
      measure_fdt (dev, ssm, g5125_fdt_start_base);
      break;
    }
}

static void start_scan (FpDevice *dev);
static void action_stopped (FpDevice *dev);
static void action_finish (FpDevice *dev, GError *error);

static void
activate_complete (FpiSsm *ssm, FpDevice *dev, GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  self->act_ssm = NULL;
  if (self->deactivating)
    {
      g_clear_error (&error);
      action_stopped (dev);
      return;
    }
  if (error)
    {
      action_finish (dev, error);
      return;
    }
  fp_dbg ("Activation complete");
  start_scan (dev);
}

/* ---- scanning ------------------------------------------------------------ */

enum scan_states {
  SCAN_MEASURE,
  SCAN_ARM_DOWN,
  SCAN_GET_IMAGE,
  SCAN_ARM_UP,
  SCAN_TLS_RECONNECT,
  SCAN_NUM,
};

static gboolean
soft_error (FpiDeviceGoodixTls5125 *self, FpiSsm *ssm, const char *what)
{
  if (++self->soft_errors > G5125_MAX_SOFT_ERRORS)
    {
      fpi_ssm_mark_failed (ssm, fpi_device_error_new_msg (
                             FP_DEVICE_ERROR_PROTO, "%s (%u times in a row)",
                             what, self->soft_errors - 1));
      return FALSE;
    }
  fp_warn ("%s, retrying", what);
  return TRUE;
}

/* Cancellation and suspend can arrive while a scan step is in flight. Every
 * scan callback and state checks this and ends the scan machine instead of
 * continuing. */
static gboolean
scan_stop_if_deactivating (FpiDeviceGoodixTls5125 *self, FpiSsm *ssm, GError **error)
{
  if (!self->deactivating)
    return FALSE;
  if (error)
    g_clear_error (error);
  if (ssm == self->scan_ssm)
    fpi_ssm_mark_completed (ssm);
  return TRUE;
}

static void
on_finger_down (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
                GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  guint16 raw[G5125_CHANNELS];
  guint16 irq;

  if (scan_stop_if_deactivating (self, ssm, &error))
    return;
  if (g_error_matches (error, GOODIX_ERROR, GOODIX_ERROR_TLS_RECONNECT))
    {
      g_error_free (error);
      if (soft_error (self, ssm, "TLS reconnect requested"))
        fpi_ssm_jump_to_state (ssm, SCAN_TLS_RECONNECT);
      return;
    }
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }

  irq = g5125_irq_status (data, length);
  if (irq == G5125_IRQ_REVERSE || irq == G5125_IRQ_REVERSE2)
    {
      /* a lift without a touch, common while the finger settles: re-arm */
      fp_dbg ("FDT reverse event, re-arming");
      fpi_ssm_jump_to_state (ssm, SCAN_MEASURE);
      return;
    }
  if (irq != G5125_IRQ_FINGER_DOWN)
    {
      if (soft_error (self, ssm, "Unexpected FDT event"))
        fpi_ssm_jump_to_state (ssm, SCAN_MEASURE);
      return;
    }
  /* Up base from the finger-down readings (Windows: "get fdt-up base"). */
  if (!g5125_fdt_channels (data, length, raw) || !g5125_fdt_base (raw, self->up_base))
    memcpy (self->up_base, self->base, sizeof self->up_base);
  self->soft_errors = 0;
  fpi_ssm_next_state (ssm);
}

/* Returns TRUE and the engine score if the frame matches the stored print. */
static gboolean
match_print (FpPrint *print, const guint8 *prep, gint *score, GError **error)
{
  g_autoptr(GVariant) data = NULL;
  const guint8 *blob;
  gsize len;

  g_object_get (print, "fpi-data", &data, NULL);
  if (!g5125_template_from_variant (data, &blob, &len))
    {
      g_set_error (error, FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID,
                   "Stored print is not a goodixtls5125 engine template");
      return FALSE;
    }
  *score = 0;
  return goodix_engine_verify_image (prep, G5125_WIDTH, G5125_HEIGHT, blob, len, score) == 1 &&
         *score >= G5125_MATCH_THRESHOLD;
}

static void
handle_frame (FpDevice *dev, const guint16 pix[G5125_PIXELS])
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  guint8 prep[G5125_PIXELS];

  g5125_engine_prep (pix, prep);

  switch (self->action)
    {
    case G5125_ACTION_ENROLL:
      {
        gint enrolled = 0, progress = 0;
        gint r = goodix_engine_enroll_add_image (self->enroll_ctx, prep, G5125_WIDTH,
                                                 G5125_HEIGHT, &enrolled, &progress);

        self->frame_rejected = (r != 0);
        if (!self->frame_rejected)
          {
            self->enroll_stage++;
            self->enroll_progress = progress;
          }
        fp_dbg ("Enroll frame: engine result %d, %d images, %d%%", r, enrolled, progress);
        break;
      }

    case G5125_ACTION_VERIFY:
      {
        FpPrint *print = NULL;
        gint score = 0;

        fpi_device_get_verify_data (dev, &print);
        self->result = match_print (print, prep, &score, &self->result_error) ?
                       FPI_MATCH_SUCCESS : FPI_MATCH_FAIL;
        fp_dbg ("Verify: score %d (threshold %d)", score, G5125_MATCH_THRESHOLD);
        break;
      }

    case G5125_ACTION_IDENTIFY:
      {
        GPtrArray *prints = NULL;
        gint best = -1;

        fpi_device_get_identify_data (dev, &prints);
        self->identified = NULL;
        for (guint i = 0; prints && i < prints->len; i++)
          {
            g_autoptr(GError) err = NULL;
            gint score = 0;

            if (match_print (g_ptr_array_index (prints, i), prep, &score, &err) && score > best)
              {
                best = score;
                self->identified = g_ptr_array_index (prints, i);
              }
          }
        fp_dbg ("Identify: best score %d", best);
        break;
      }

    case G5125_ACTION_NONE:
    default:
      break;
    }
}

static void
on_image (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
          GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  guint16 pix[G5125_PIXELS];

  if (scan_stop_if_deactivating (self, ssm, &error))
    return;
  if (error)
    {
      g_error_free (error);
      if (soft_error (self, ssm, "No image after get-image"))
        fpi_ssm_jump_to_state (ssm, SCAN_MEASURE);
      return;
    }
  if (!g5125_decode_image (data, length, pix))
    {
      if (soft_error (self, ssm, "Image has unexpected length"))
        fpi_ssm_jump_to_state (ssm, SCAN_MEASURE);
      return;
    }

  handle_frame (dev, pix);
  fpi_ssm_next_state (ssm);
}

static void
on_finger_up (FpDevice *dev, guint8 *data, guint16 length, gpointer ssm,
              GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (scan_stop_if_deactivating (self, ssm, &error))
    return;
  if (g_error_matches (error, GOODIX_ERROR, GOODIX_ERROR_TLS_RECONNECT))
    {
      /* The frame is already handled; losing the lift-off event is harmless. */
      g_error_free (error);
      fpi_ssm_mark_completed (ssm);
      return;
    }
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  fpi_ssm_mark_completed (ssm);
}

static void
on_tls_reinit (FpDevice *dev, gpointer ssm, GError *error)
{
  if (scan_stop_if_deactivating (FPI_DEVICE_GOODIXTLS5125 (dev), ssm, &error))
    return;
  if (error)
    {
      fpi_ssm_mark_failed (ssm, error);
      return;
    }
  fpi_ssm_jump_to_state (ssm, SCAN_MEASURE);
}

static void
scan_ssm_run (FpiSsm *ssm, FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (scan_stop_if_deactivating (self, ssm, NULL))
    return;

  switch (fpi_ssm_get_cur_state (ssm))
    {
    case SCAN_MEASURE:
      if (goodix_take_tls_reconnect_request (dev))
        {
          fpi_ssm_jump_to_state (ssm, SCAN_TLS_RECONNECT);
          return;
        }
      measure_fdt (dev, ssm, self->base);
      break;

    case SCAN_ARM_DOWN:
      {
        guint8 *p = g_malloc (2 + G5125_FDT_BASE_LEN + 2);
        gsize n = g5125_fdt_payload (G5125_FDT_OP_DOWN, self->base, TRUE,
                                     g5125_timestamp_now (), p);

        fpi_device_report_finger_status_changes (dev, FP_FINGER_STATUS_NEEDED,
                                                 FP_FINGER_STATUS_NONE);
        goodix_send_mcu_switch_to_fdt_down (dev, p, n, g_free, on_finger_down, ssm);
        break;
      }

    case SCAN_GET_IMAGE:
      fpi_device_report_finger_status_changes (dev, FP_FINGER_STATUS_PRESENT,
                                               FP_FINGER_STATUS_NEEDED);
      goodix_tls_read_image (dev, on_image, ssm);
      break;

    case SCAN_ARM_UP:
      {
        guint8 *p = g_malloc (2 + G5125_FDT_BASE_LEN);
        gsize n = g5125_fdt_payload (G5125_FDT_OP_UP, self->up_base, FALSE, 0, p);

        goodix_send_mcu_switch_to_fdt_up (dev, p, n, g_free, on_finger_up, ssm);
        break;
      }

    case SCAN_TLS_RECONNECT:
      {
        g_autoptr(GError) err = NULL;

        goodix_shutdown_tls (dev, &err);
        goodix_tls_init (dev, on_tls_reinit, ssm);
        break;
      }
    }
}

/* ---- actions ------------------------------------------------------------- */

static void
action_cleanup (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  g_autoptr(GError) err = NULL;

  goodix_shutdown_tls (dev, &err);
  g_clear_pointer (&self->enroll_ctx, goodix_engine_enroll_finish);
  g_clear_error (&self->result_error);
  self->identified = NULL;
  self->action = G5125_ACTION_NONE;
  self->deactivating = self->cancelling = self->suspending = FALSE;
  fpi_device_report_finger_status_changes (dev, FP_FINGER_STATUS_NONE,
                                           FP_FINGER_STATUS_NEEDED | FP_FINGER_STATUS_PRESENT);
}

/* Ends the current action with an error (or cancellation). */
static void
action_finish (FpDevice *dev, GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  G5125Action action = self->action;

  action_cleanup (dev);
  switch (action)
    {
    case G5125_ACTION_ENROLL:
      fpi_device_enroll_complete (dev, NULL, error);
      break;

    case G5125_ACTION_VERIFY:
      fpi_device_verify_complete (dev, error);
      break;

    case G5125_ACTION_IDENTIFY:
      fpi_device_identify_complete (dev, error);
      break;

    case G5125_ACTION_NONE:
    default:
      g_clear_error (&error);
      break;
    }
}

static void
enroll_commit (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  FpPrint *print = NULL;
  uint8_t *blob = NULL;
  size_t len = 0;

  if (goodix_engine_enroll_commit (self->enroll_ctx, &blob, &len) != 0 || !blob || len == 0)
    {
      free (blob);
      action_finish (dev, fpi_device_error_new_msg (FP_DEVICE_ERROR_GENERAL,
                                                    "Goodix engine could not build the template"));
      return;
    }
  fpi_device_get_enroll_data (dev, &print);
  fpi_print_set_type (print, FPI_PRINT_RAW);
  g_object_set (print, "fpi-data", g5125_template_to_variant (blob, len), NULL);
  free (blob);
  fp_dbg ("Enrolled: template %zu bytes", len);
  action_cleanup (dev);
  fpi_device_enroll_complete (dev, g_object_ref (print), NULL);
}

static void
scan_complete (FpiSsm *ssm, FpDevice *dev, GError *error)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  gint stages = fp_device_get_nr_enroll_stages (dev);

  self->scan_ssm = NULL;
  if (self->deactivating)
    {
      g_clear_error (&error);
      action_stopped (dev);
      return;
    }
  if (error)
    {
      action_finish (dev, error);
      return;
    }

  switch (self->action)
    {
    case G5125_ACTION_ENROLL:
      if (self->frame_rejected)
        {
          fpi_device_enroll_progress (dev, self->enroll_stage, NULL,
                                      fpi_device_retry_new (FP_DEVICE_RETRY_GENERAL));
          start_scan (dev);
          return;
        }
      fpi_device_enroll_progress (dev, self->enroll_stage, NULL, NULL);
      if (self->enroll_stage >= stages)
        {
          enroll_commit (dev);
          return;
        }
      start_scan (dev);
      return;

    case G5125_ACTION_VERIFY:
      if (self->result_error)
        {
          GError *err = g_steal_pointer (&self->result_error);

          action_finish (dev, err);
          return;
        }
      fpi_device_verify_report (dev, self->result, NULL, NULL);
      action_cleanup (dev);
      fpi_device_verify_complete (dev, NULL);
      return;

    case G5125_ACTION_IDENTIFY:
      fpi_device_identify_report (dev, self->identified, NULL, NULL);
      action_cleanup (dev);
      fpi_device_identify_complete (dev, NULL);
      return;

    case G5125_ACTION_NONE:
    default:
      return;
    }
}

static void
start_scan (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  self->soft_errors = 0;
  self->frame_rejected = FALSE;
  self->scan_ssm = fpi_ssm_new (dev, scan_ssm_run, SCAN_NUM);
  fpi_ssm_start (self->scan_ssm, scan_complete);
}

static void
start_activation (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  self->soft_errors = 0;
  self->act_ssm = fpi_ssm_new (dev, activate_ssm_run, ACT_NUM);
  fpi_ssm_start (self->act_ssm, activate_complete);
}

static void
start_action (FpDevice *dev, G5125Action action)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  self->action = action;
  self->enroll_stage = 0;
  self->enroll_progress = 0;
  self->result = FPI_MATCH_FAIL;
  if (action == G5125_ACTION_ENROLL)
    {
      gint max_images = 0;

      self->enroll_ctx = goodix_engine_enroll_start (&max_images);
      if (!self->enroll_ctx)
        {
          action_finish (dev, fpi_device_error_new_msg (FP_DEVICE_ERROR_GENERAL,
                                                        "Goodix engine could not start enrolment"));
          return;
        }
    }
  start_activation (dev);
}

/* The stopped action's machines have ended; finish the cancel or suspend. */
static void
action_stopped (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (self->suspending)
    {
      self->deactivating = FALSE;
      fpi_device_suspend_complete (dev, NULL);
      return;
    }
  action_finish (dev, g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation was cancelled"));
}

/* Runs from the main loop after the triggering callback has returned: end
 * whichever machine is still waiting for a (now dropped) device reply. */
static void
stop_deferred (FpDevice *dev, gpointer user_data)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (!self->deactivating)
    return;
  if (self->act_ssm)
    fpi_ssm_mark_completed (self->act_ssm);
  else if (self->scan_ssm)
    fpi_ssm_mark_completed (self->scan_ssm);
  else
    action_stopped (dev);
}

static void
stop_action (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);
  g_autoptr(GError) err = NULL;

  self->deactivating = TRUE;
  goodix_reset_state (dev);   /* drop any pending device command */
  goodix_shutdown_tls (dev, &err);
  fpi_device_add_timeout (dev, 0, stop_deferred, NULL, NULL);
}

/* ---- device ops ---------------------------------------------------------- */

static void
dev_open (FpDevice *dev)
{
  const char *dll = g_getenv ("GOODIX5125_ENGINE_DLL");
  GError *error = NULL;

  if (!dll || !*dll)
    dll = G5125_ENGINE_DEFAULT_PATH;
  if (!goodix_engine_init (dll, &error))
    {
      fpi_device_open_complete (dev, fpi_device_error_new_msg (FP_DEVICE_ERROR_GENERAL,
                                                               "%s", error->message));
      g_error_free (error);
      return;
    }
  fp_info ("Goodix engine %s loaded", goodix_engine_get_version ());
  goodix_dev_init (dev, &error);
  fpi_device_open_complete (dev, error);
}

static void
dev_close (FpDevice *dev)
{
  GError *error = NULL;

  goodix_dev_deinit (dev, &error);
  fpi_device_close_complete (dev, error);
}

static void
dev_enroll (FpDevice *dev)
{
  start_action (dev, G5125_ACTION_ENROLL);
}

static void
dev_verify (FpDevice *dev)
{
  start_action (dev, G5125_ACTION_VERIFY);
}

static void
dev_identify (FpDevice *dev)
{
  start_action (dev, G5125_ACTION_IDENTIFY);
}

static void
dev_cancel (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (self->action == G5125_ACTION_NONE || self->deactivating)
    return;
  self->cancelling = TRUE;
  stop_action (dev);
}

static void
dev_suspend (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  if (self->action == G5125_ACTION_NONE)
    {
      fpi_device_suspend_complete (dev, NULL);
      return;
    }
  self->suspending = TRUE;
  stop_action (dev);
}

static void
dev_resume (FpDevice *dev)
{
  FpiDeviceGoodixTls5125 *self = FPI_DEVICE_GOODIXTLS5125 (dev);

  fpi_device_resume_complete (dev, NULL);
  if (self->action != G5125_ACTION_NONE)
    {
      /* the sensor lost its state while suspended: activate from scratch */
      self->suspending = FALSE;
      start_activation (dev);
    }
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

  gx_class->interface = 0;
  gx_class->ep_in = 0x81;
  gx_class->ep_out = 0x01;
  gx_class->get_image_cmd = 0x22;

  dev_class->id = "goodixtls5125";
  dev_class->full_name = "Goodix TLS Fingerprint Sensor 27c6:5125";
  dev_class->type = FP_DEVICE_TYPE_USB;
  dev_class->id_table = id_table;
  dev_class->scan_type = FP_SCAN_TYPE_PRESS;
  dev_class->nr_enroll_stages = 12;   /* the engine keeps at most 12 images */

  /* Matching is done by the Goodix engine, not libfprint's image pipeline */
  dev_class->open = dev_open;
  dev_class->close = dev_close;
  dev_class->enroll = dev_enroll;
  dev_class->verify = dev_verify;
  dev_class->identify = dev_identify;
  dev_class->cancel = dev_cancel;
  dev_class->suspend = dev_suspend;
  dev_class->resume = dev_resume;

  /* drop what the image-device parent advertises (capture, print update) */
  dev_class->capture = NULL;
  dev_class->features = FP_DEVICE_FEATURE_NONE;
  fpi_device_class_auto_initialize_features (dev_class);
}
