#pragma once
#define MFX_DEPRECATED_OFF
#include <stdio.h>

#include "mfxvideo++.h"


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

// Usage of the following two macros are only required for certain Windows DirectX11 use cases
constexpr auto WILL_READ = 0x1000;
constexpr auto WILL_WRITE = 0x2000;

// =================================================================
// Intel Media SDK memory allocator entrypoints....
// Implementation of this functions is OS/Memory type specific.
mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
		       mfxFrameAllocResponse *response);
mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response);
mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
			 mfxU64 lock_key, mfxU64 *next_key);



void PrintErrString(int err, const char *filestr, int line);

typedef struct {
	mfxBitstream mfxBS;
	mfxSyncPoint syncp;
} Task;

void Release();

void mfxGetTime(mfxTime *timestamp);

//void mfxInitTime();  might need this for Windows
double TimeDiffMsec(mfxTime tfinish, mfxTime tstart);
extern "C" void util_cpuid(int cpuinfo[4], int flags);
