#ifndef __QSV_VPL_LINUX_DEFS_H__
#define __QSV_VPL_LINUX_DEFS_H__
#endif
#if defined(__linux__)
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef __X86INTRIN_H
#include <x86intrin.h>
#endif
#include <obs-nix-platform.h>
#include <va/va_wayland.h>
#include <va/va_x11.h>
#include <va/va_drm.h>
#include <va/va_str.h>
#include <va/va_drmcommon.h>

#include <util/bmem.h>
#endif