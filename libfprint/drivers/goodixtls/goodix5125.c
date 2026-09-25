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

struct _FpiDeviceGoodixTls5125
{
  FpiDeviceGoodixTls parent;
};

G_DEFINE_TYPE (FpiDeviceGoodixTls5125, fpi_device_goodixtls5125,
               FPI_TYPE_DEVICE_GOODIXTLS)

static const FpIdEntry id_table[] = {
  { .vid = 0x27c6, .pid = 0x5125 },
  { .vid = 0, .pid = 0, .driver_data = 0 },
};

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
  fpi_image_device_activate_complete (
    img_dev, fpi_device_error_new_msg (FP_DEVICE_ERROR_NOT_SUPPORTED,
                                       "goodixtls5125: activation not implemented yet"));
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
  img_class->img_width = 64;
  img_class->img_height = 80;
  img_class->bz3_threshold = 24;

  fpi_device_class_auto_initialize_features (dev_class);
}
