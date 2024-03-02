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
#include "../bits/windows_defs.h"
#include "hw_d3d11.hpp"
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

static void avx2_memcpy(uint8_t *dst, const uint8_t *src,
                        unsigned long long size) {
  if (size < 128) {
    for (int i = 0; i < size; i++)
      dst[i] = src[i];
    return;
  }
  uint8_t *dst_fin = dst + size;
  const uint8_t *dst_aligned_fin = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(dst_fin + 31) & ~31) - 128);
  __m256i y0, y1, y2, y3;
  const int start_align_diff =
      static_cast<int>(reinterpret_cast<size_t>(dst) & 31);
  if (start_align_diff) {
    y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), y0);
    dst += 32 - start_align_diff;
    src += 32 - start_align_diff;
  }
  for (; dst < dst_aligned_fin; dst += 128, src += 128) {
    y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 0));
    y1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 32));
    y2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 64));
    y3 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 96));
    _mm256_stream_si256(reinterpret_cast<__m256i *>(dst + 0), y0);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(dst + 32), y1);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(dst + 64), y2);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(dst + 96), y3);
  }
  uint8_t *dst_tmp = dst_fin - 128;
  src -= (dst - dst_tmp);
  y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 0));
  y1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 32));
  y2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 64));
  y3 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + 96));
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst_tmp + 0), y0);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst_tmp + 32), y1);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst_tmp + 64), y2);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst_tmp + 96), y3);
  _mm256_zeroupper();
}