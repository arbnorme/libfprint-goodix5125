/*
 * Extra kernel32/advapi32 shims needed by AlgoChicago.dll (goodixtls5125).
 * Copyright (C) 2026 The libfprint-goodix5125 contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Most of these only need to exist: the engine touches them for logging,
 * locale and file housekeeping, not for matching.
 */
#include <malloc.h>
static u16 g_cmdlinew[] = { 'x', 0 };
static char g_cmdlinea[] = "x";
static u16 g_envw[] = { 0, 0 };
static u32 MS sx_EventRegister (void *g, void *cb, void *ctx, u64 *h) { if (h) *h = 1; return 0; }
static u32 MS sx_EventSetInformation (u64 h, int c, void *i, u32 l) { return 0; }
static u32 MS sx_EventWriteTransfer (u64 h, void *d, void *a, void *r, u32 n, void *p) { return 0; }
static long MS sx_RegFail (void) { return 2; }
static int MS sx_CompareStringW (u32 loc, u32 fl, const u16 *a, int la, const u16 *b, int lb) {
  if (la < 0)
    {
      la = 0;
      while (a[la])
        la++;
    }
  if (lb < 0)
    {
      lb = 0;
      while (b[lb])
        lb++;
    }
  for (int i = 0; i < la && i < lb; i++) if (a[i] != b[i]) return a[i] < b[i] ? 1 : 3;
  return la == lb ? 2 : (la < lb ? 1 : 3);
}
static int MS sx_Fail0 (void) { g_lasterr = 2; return 0; }
static int MS sx_True (void) { return 1; }
static void MS sx_ExitProcess (u32 c) { abort (); }
static void *MS sx_FindFirstFileExW (void *a, int b, void *c, int d, void *e, u32 f) { g_lasterr = 2; return (void *) -1; }
static u32 MS sx_GetACP (void) { return 1252; }
static u32 MS sx_GetOEMCP (void) { return 437; }
static u32 MS sx_Zero (void) { return 0; }
static char *MS sx_GetCommandLineA (void) { return g_cmdlinea; }
static u16 *MS sx_GetCommandLineW (void) { return g_cmdlinew; }
static int MS sx_GetCPInfo (u32 cp, u8 *info) { memset (info, 0, 20); *(u32 *) info = 1; info[4] = '?'; return 1; }
static u16 *MS sx_GetEnvironmentStringsW (void) { return g_envw; }
static u32 MS sx_GetFileType (void *h) { return 2; }
static u32 MS sx_GetModuleFileNameW (void *h, u16 *buf, u32 n) {
  static const char p[] = "C:\\Windows\\System32\\AlgoChicago.dll"; u32 i = 0;
  for (; p[i] && i + 1 < n; i++) buf[i] = p[i];
  if (n)
    buf[i] = 0;
  return i;
}
static int MS sx_GetModuleHandleExW (u32 fl, void *name, void **out) { if (out) *out = g_image; return 1; }
static void *get_export (const char *want);
static void *MS sx_GetProcAddress (void *h, const char *name) {
  if ((uintptr_t) name < 0x10000) return NULL;
  void *f = find_shim (name); if (f) return f;
  return get_export (name);
}
static void *MS sx_GetStdHandle (u32 n) { return (void *) (uintptr_t) (0x10 + (n & 0xf)); }
static int MS sx_GetStringTypeW (u32 t, const u16 *s, int n, u16 *out) {
  if (n < 0)
    {
      n = 0;
      while (s[n])
        n++;
      n++;
    }
  for (int i = 0; i < n; i++) out[i] = (s[i] >= '0' && s[i] <= '9') ? 0x04 : ((s[i] | 32) >= 'a' && (s[i] | 32) <= 'z') ? 0x100 : 0;
  return 1;
}
static u32 MS sx_GetTimeZoneInformation (void *tzi) { memset (tzi, 0, 172); return 0; }
static void *MS sx_HeapReAlloc (void *h, u32 fl, void *p, u64 s) { return realloc (p, s ? s : 1); }
static u64 MS sx_HeapSize (void *h, u32 fl, void *p) { return malloc_usable_size (p); }
static int MS sx_LCMapStringW (u32 loc, u32 fl, const u16 *s, int n, u16 *d, int dn) {
  if (n < 0)
    {
      n = 0;
      while (s[n])
        n++;
      n++;
    }
  if (dn == 0) return n;
  for (int i = 0; i < n && i < dn; i++) d[i] = s[i];
  return n < dn ? n : dn;
}
static void *MS sx_LoadLibraryExW (void *n, void *f, u32 fl) { g_lasterr = 126; return NULL; }
static u32 MS sx_SetFilePointer (void *h, long d, long *hi, u32 m) { return 0xffffffff; }
static int MS sx_WriteConsoleW (void *h, const void *b, u32 n, u32 *w, void *r) { if (w) *w = n; return 1; }

static void register_chicago_shims (void) {
  reg_name ("EventRegister", sx_EventRegister);
  reg_name ("EventSetInformation", sx_EventSetInformation);
  reg_name ("EventWriteTransfer", sx_EventWriteTransfer);
  reg_name ("RegDeleteTreeW", sx_RegFail);
  reg_name ("RegEnumKeyExW", sx_RegFail);
  reg_name ("RegQueryInfoKeyW", sx_RegFail);
  reg_name ("CompareStringW", sx_CompareStringW);
  reg_name ("CreateDirectoryW", sx_Fail0);
  reg_name ("ExitProcess", sx_ExitProcess);
  reg_name ("FindClose", sx_True);
  reg_name ("FindFirstFileExW", sx_FindFirstFileExW);
  reg_name ("FindNextFileW", sx_Fail0);
  reg_name ("FlushFileBuffers", sx_True);
  reg_name ("FreeEnvironmentStringsW", sx_True);
  reg_name ("FreeLibrary", sx_True);
  reg_name ("GetACP", sx_GetACP);
  reg_name ("GetOEMCP", sx_GetOEMCP);
  reg_name ("GetCommandLineA", sx_GetCommandLineA);
  reg_name ("GetCommandLineW", sx_GetCommandLineW);
  reg_name ("GetConsoleCP", sx_Zero);
  reg_name ("GetConsoleMode", sx_Zero);
  reg_name ("GetCPInfo", sx_GetCPInfo);
  reg_name ("GetDateFormatW", sx_Zero);
  reg_name ("GetTimeFormatW", sx_Zero);
  reg_name ("GetEnvironmentStringsW", sx_GetEnvironmentStringsW);
  reg_name ("GetFileAttributesExW", sx_Fail0);
  reg_name ("GetFileSizeEx", sx_Fail0);
  reg_name ("GetFileType", sx_GetFileType);
  reg_name ("GetModuleFileNameW", sx_GetModuleFileNameW);
  reg_name ("GetModuleHandleExW", sx_GetModuleHandleExW);
  reg_name ("GetProcAddress", sx_GetProcAddress);
  reg_name ("GetStdHandle", sx_GetStdHandle);
  reg_name ("GetStringTypeW", sx_GetStringTypeW);
  reg_name ("GetTimeZoneInformation", sx_GetTimeZoneInformation);
  reg_name ("HeapReAlloc", sx_HeapReAlloc);
  reg_name ("HeapSize", sx_HeapSize);
  reg_name ("InitializeCriticalSectionAndSpinCount", sx_True);
  reg_name ("IsValidCodePage", sx_True);
  reg_name ("LCMapStringW", sx_LCMapStringW);
  reg_name ("LoadLibraryExW", sx_LoadLibraryExW);
  reg_name ("ReadConsoleW", sx_Fail0);
  reg_name ("ReadFile", sx_Fail0);
  reg_name ("SetEndOfFile", sx_Fail0);
  reg_name ("SetEnvironmentVariableW", sx_True);
  reg_name ("SetFilePointer", sx_SetFilePointer);
  reg_name ("SetFilePointerEx", sx_Fail0);
  reg_name ("SetStdHandle", sx_True);
  reg_name ("WriteConsoleW", sx_WriteConsoleW);
}
