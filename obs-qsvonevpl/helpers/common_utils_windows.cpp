#include "common_utils.h"

#include "common_directx11.h"

#include <intrin.h>
#include <wrl/client.h>

/* =======================================================
 * Windows implementation of OS-specific utility functions
 */

void util_cpuid(int cpuinfo[4], int flags)
{
	return __cpuid(cpuinfo, flags);
}

void Release()
{
	CleanupHWDevice();
}

void mfxGetTime(mfxTime *timestamp)
{
	QueryPerformanceCounter(timestamp);
}

double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
	static LARGE_INTEGER tFreq = {0};

	if (!tFreq.QuadPart)
		QueryPerformanceFrequency(&tFreq);

	double freq = (double)tFreq.QuadPart;
	return 1000.0 * ((double)tfinish.QuadPart - (double)tstart.QuadPart) /
	       freq;
}
