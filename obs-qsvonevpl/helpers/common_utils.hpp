#pragma once
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>
#include "common_directx11.hpp"
#include <VersionHelpers.h>
#endif

#include <obs-module.h>
#include <memory>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <thread>

// =================================================================
// OS-specific definitions of types, macro, etc...
// The following should be defined:
//  - mfxTime
//  - MSDK_FOPEN
//  - MSDK_SLEEP
#if defined(_WIN32) || defined(_WIN64)
#include "../bits/windows_defs.hpp"
#elif defined(__linux__)
#include "../bits/linux_defs.hpp"
#endif

// =================================================================
// Helper macro definitions...
#define MSDK_PRINT_RET_MSG(ERR)                          \
	{                                                \
		PrintErrString(ERR, __FILE__, __LINE__); \
	}
#define MSDK_CHECK_RESULT(P, X, ERR)             \
	{                                        \
		if ((X) > (P)) {                 \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_POINTER(P, ERR)               \
	{                                        \
		if (!(P)) {                      \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_CHECK_ERROR(P, X, ERR)              \
	{                                        \
		if ((X) == (P)) {                \
			MSDK_PRINT_RET_MSG(ERR); \
			return ERR;              \
		}                                \
	}
#define MSDK_IGNORE_MFX_STS(P, X)         \
	{                                 \
		if ((X) == (P)) {         \
			P = MFX_ERR_NONE; \
		}                         \
	}
#define MSDK_BREAK_ON_ERROR(P)           \
	{                                \
		if (MFX_ERR_NONE != (P)) \
			break;           \
	}
#define MSDK_SAFE_DELETE_ARRAY(P)   \
	{                           \
		if (P) {            \
			delete[] P; \
			P = NULL;   \
		}                   \
	}
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_SAFE_RELEASE(X)          \
	{                             \
		if (X) {              \
			X->Release(); \
			X = NULL;     \
		}                     \
	}
#define MSDK_MAX(A, B) (((A) > (B)) ? (A) : (B))

#define INIT_MFX_EXT_BUFFER(x, id)               \
	{                                        \
		(x).Header.BufferId = (id);      \
		(x).Header.BufferSz = sizeof(x); \
	}

#define INFINITE 0xFFFFFFFF // Infinite timeout

void PrintErrString(int err, const char *filestr, int line);

void mfxGetTime(mfxTime *timestamp);


//void mfxInitTime();  might need this for Windows
double TimeDiffMsec(mfxTime tfinish, mfxTime tstart);
extern "C" void util_cpuid(int cpuinfo[4], int flags);
