#include "common_utils.hpp"

#include "common_directx11.hpp"

#include <intrin.h>
#include <wrl/client.h>

/* =======================================================
 * Windows implementation of OS-specific utility functions
 */

void util_cpuid(int cpuinfo[4], int flags)
{
	return __cpuid(cpuinfo, flags);
}

void mfxGetTime(mfxTime *timestamp)
{
	QueryPerformanceCounter(timestamp);
}

double TimeDiffMsec(mfxTime tfinish, mfxTime tstart)
{
	static LARGE_INTEGER tFreq = {};

	if (!tFreq.QuadPart)
		QueryPerformanceFrequency(&tFreq);

	double freq = static_cast<double>(tFreq.QuadPart);
	return static_cast<double>(1000.0 * (static_cast<double>(tfinish.QuadPart) - static_cast<double>(tstart.QuadPart)) /
	       freq);
}
