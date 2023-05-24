#include "common_utils.h"


#include <intrin.h>
#include <wrl/client.h>

/* =======================================================
 * Windows implementation of OS-specific utility functions
 */

void util_cpuid(int cpuinfo[4], int flags)
{
	return __cpuid(cpuinfo, flags);
}
