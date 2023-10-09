#pragma once
#ifndef __QSV_VPL_COMMON_UTILS_H__
#define __QSV_VPL_COMMON_UTILS_H__
#endif

#ifndef __INTRIN_H_
#include <intrin.h>
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
// =================================================================
// OS-specific definitions of types, macro, etc...
// The following should be defined:
//  - mfxTime
//  - MSDK_FOPEN
//  - MSDK_SLEEP
#if defined(_WIN32) || defined(_WIN64)
#include "../bits/windows_defs.h"
#elif defined(__linux__)
#include "../bits/linux_defs.h"
#endif

#define INFINITE 0xFFFFFFFF // Infinite timeout

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
