#pragma once
#ifndef __QSV_VPL_COMMON_UTILS_H__
#define __QSV_VPL_COMMON_UTILS_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef __INTRIN_H_
#include <intrin.h>
#endif
#elif defined(__linux__)
#ifndef __X86INTRIN_H
#include <x86intrin.h>
#endif
#endif
#ifndef _INC_STDIO
#include <stdio.h>
#endif
#ifndef _CSTDIO_
#include <cstdio>
#endif
#ifndef _CONDITION_VARIABLE_
#include <condition_variable>
#endif
#ifndef _CSTDLIB_
#include <cstdlib>
#endif
#ifndef _INC_STDLIB
#include <stdlib.h>
#endif
#ifndef _MEMORY_
#include <memory>
#endif
#ifndef _THREAD_
#include <thread>
#endif
#ifndef _VECTOR_
#include <vector>
#endif
#ifndef _BIT_
#include <bit>
#endif
#ifndef _CSTDDEF_
#include <cstddef>
#endif
#ifndef _CSTDINT_
#include <cstdint>
#endif
#ifndef _ATOMIC_
#include <atomic>
#endif
#ifndef _STRING_
#include <string>
#endif
#ifndef _CINTTYPES_
#include <cinttypes>
#endif
#ifndef _NEW_
#include <new>
#endif
#ifndef __MFX_H__
#include <vpl/mfx.h>
#endif
#ifndef __MFXVIDEOPLUSPLUS_H
#include <vpl/mfxvideo++.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "hw_d3d11.hpp"
#include "../bits/windows_defs.h"
#elif defined(__linux__)
#include "../bits/linux_defs.h"
#endif

#define do_log(level, format, ...)                                             \
  blog(level, "[QSV encoder: '%s'] " format, "libvpl", ##__VA_ARGS__);

#define error(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#define error_hr(msg) warn("%s: %s: 0x%08lX", __FUNCTION__, msg, (uint32_t)hr);

#define INFINITE 0xFFFFFFFF // Infinite timeout

#define CHECK_STS(X, ERR_TEXT)                                                 \
  {                                                                            \
    if ((X) < MFX_ERR_NONE) {                                                  \
      warn(TEXT);                                                              \
      return X;                                                                \
    }                                                                          \
  }

extern "C" void util_cpuid(int cpuinfo[4], int flags);

void check_adapters(struct adapter_info *adapters, size_t *adapter_count);

struct adapter_info {
  bool is_intel;
  bool is_dgpu;
  bool supports_av1;
  bool supports_hevc;
  bool supports_vp9;
};

#define MAX_ADAPTERS 10
extern struct adapter_info adapters[MAX_ADAPTERS];
extern size_t adapter_count;

enum qsv_codec { QSV_CODEC_AVC, QSV_CODEC_AV1, QSV_CODEC_HEVC, QSV_CODEC_VP9 };

// Usage of the following two macros are only required for certain Windows
// DirectX11 use cases
#define WILL_READ 0x1000
#define WILL_WRITE 0x2000

void Release();
void ReleaseSessionData(void *);
