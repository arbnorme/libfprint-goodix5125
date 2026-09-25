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

/* Loads the Goodix engine from $GOODIX5125_ENGINE_DLL; skips (77) when unset,
 * because the DLL is proprietary and never part of this repository. */

#include <glib.h>
#include <stdlib.h>

#include "drivers/goodixtls/goodix_engine.h"

int
main (int argc, char *argv[])
{
  const char *dll = g_getenv ("GOODIX5125_ENGINE_DLL");
  g_autoptr(GError) error = NULL;
  int max_images = 0;
  void *ctx;

  if (!dll || !*dll)
    {
      g_print ("GOODIX5125_ENGINE_DLL not set, skipping\n");
      return 77;
    }

  if (!goodix_engine_init (dll, &error))
    {
      g_printerr ("engine init failed: %s\n", error->message);
      return 1;
    }
  if (!g_str_has_prefix (goodix_engine_get_version (), "Milan_v_"))
    {
      g_printerr ("unexpected engine version '%s'\n", goodix_engine_get_version ());
      return 1;
    }
  ctx = goodix_engine_enroll_start (&max_images);
  if (!ctx || max_images != 16)
    {
      g_printerr ("enroll_start: ctx=%p max_images=%d\n", ctx, max_images);
      return 1;
    }
  goodix_engine_enroll_finish (ctx);
  g_print ("engine %s ok\n", goodix_engine_get_version ());
  return 0;
}
