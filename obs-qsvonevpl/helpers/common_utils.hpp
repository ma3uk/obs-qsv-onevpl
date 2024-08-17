#pragma once
#pragma warning(disable : 4201)

#ifndef __QSV_VPL_COMMON_UTILS_H__
#define __QSV_VPL_COMMON_UTILS_H__
#endif

#ifndef __QSV_VPL_WINDOWS_DEFS_H__
#include "../bits/windows_defs.hpp"
#endif
#ifndef __QSV_VPL_LINUX_DEFS_H__
#include "../bits/linux_defs.hpp"
#endif

#ifndef _INTTYPES
#include <inttypes.h>
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
#ifndef _VECTOR_
#include <vector>
#endif
#ifndef _ARRAY_
#include <array>
#endif
#ifndef _STRING_
#include <string>
#endif
#ifndef _OPTIONAL_
#include <optional>
#endif
#ifndef _CINTTYPES_
#include <cinttypes>
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
#ifndef _NEW_
#include <new>
#endif
#ifndef _ALGORITHM_
#include <algorithm>
#endif
#ifndef _ITERATOR_
#include <iterator>
#endif
#ifndef _MUTEX_
#include <mutex>
#endif

#ifndef __MFX_H__
#include <vpl/mfx.h>
#endif
#ifndef __MFXVIDEOPLUSPLUS_H
#include <vpl/mfxvideo++.h>
#endif

extern "C" {
#include <media-io/video-io.h>
#include <obs-av1.h>
#include <obs-avc.h>
#include <obs-hevc.h>
#include <obs-module.h>
#include <obs.h>

#include <util/base.h>
#include <util/config-file.h>
#include <util/pipe.h>
#include <util/platform.h>
}

#ifndef __QSV_VPL_HWManager_H__
#include "hw_d3d11.hpp"
#endif

#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#include "qsv_params.hpp"
#endif

#ifndef do_log
#define do_log(level, format, ...)                                             \
  blog(level, "[QSV encoder: '%s'] " format, "libvpl", ##__VA_ARGS__);
#endif
#ifndef error
#define error(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)
#endif
#ifndef warn
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#endif
#ifndef info
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#endif
#ifndef debug
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)
#endif
#ifndef error_hr
#define error_hr(msg)                                                          \
  warn("%s: %s: 0x%08lX", __FUNCTION__, msg, static_cast<uint32_t>(hr));
#endif

constexpr int MAX_ADAPTERS = 10;

void GetAdaptersInfo(struct adapter_info *Adapters, size_t *AdaptersCount);

struct adapter_info {
  bool IsIntel;
  bool IsDGPU;
  bool SupportAV1;
  bool SupportHEVC;
};

extern struct adapter_info AdaptersInfo[MAX_ADAPTERS];
extern size_t AdaptersCount;

enum codec_enum { QSV_CODEC_AVC, QSV_CODEC_AV1, QSV_CODEC_HEVC };

void ReleaseSessionData(void *);

inline static void avx2_memcpy(uint8_t *Dst, const uint8_t *Src,
                               unsigned long long Size) {
  if (Size < 128) {
    for (int i = 0; i < Size; i++)
      Dst[i] = Src[i];
    return;
  }
  uint8_t *DstFin = Dst + Size;
  const uint8_t *DstAlignedFin = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<size_t>(DstFin + 31) & ~31) - 128);
  __m256i Y0, Y1, Y2, Y3;
  const int StartAlignDiff =
      static_cast<int>(reinterpret_cast<size_t>(Dst) & 31);
  if (StartAlignDiff) {
    Y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(Dst), Y0);
    Dst += 32 - StartAlignDiff;
    Src += 32 - StartAlignDiff;
  }
  for (; Dst < DstAlignedFin; Dst += 128, Src += 128) {
    Y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 0));
    Y1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 32));
    Y2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 64));
    Y3 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 96));
    _mm256_stream_si256(reinterpret_cast<__m256i *>(Dst + 0), Y0);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(Dst + 32), Y1);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(Dst + 64), Y2);
    _mm256_stream_si256(reinterpret_cast<__m256i *>(Dst + 96), Y3);
  }
  uint8_t *DstTmpl = DstFin - 128;
  Src -= (Dst - DstTmpl);
  Y0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 0));
  Y1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 32));
  Y2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 64));
  Y3 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(Src + 96));
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(DstTmpl + 0), Y0);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(DstTmpl + 32), Y1);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(DstTmpl + 64), Y2);
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(DstTmpl + 96), Y3);
  _mm256_zeroupper();
}