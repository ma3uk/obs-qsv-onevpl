#include "common_utils.hpp"
#include "../obs-qsv-onevpl-encoder.hpp"
#include <string>
#include <cstdint>

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//

void PrintErrString(int err, const char *filestr, int line)
{
	switch (err) {
	case 0:
		printf("\n No error.\n");
		break;
	case -1:
		printf("\n Unknown error: %s %d\n", filestr, line);
		break;
	case -2:
		printf("\n Null pointer.  Check filename/path + permissions? %s %d\n",
		       filestr, line);
		break;
	case -3:
		printf("\n Unsupported feature/library load error. %s %d\n",
		       filestr, line);
		break;
	case -4:
		printf("\n Could not allocate memory. %s %d\n", filestr, line);
		break;
	case -5:
		printf("\n Insufficient IO buffers. %s %d\n", filestr, line);
		break;
	case -6:
		printf("\n Invalid handle. %s %d\n", filestr, line);
		break;
	case -7:
		printf("\n Memory lock failure. %s %d\n", filestr, line);
		break;
	case -8:
		printf("\n Function called before initialization. %s %d\n",
		       filestr, line);
		break;
	case -9:
		printf("\n Specified object not found. %s %d\n", filestr, line);
		break;
	case -10:
		printf("\n More input data expected. %s %d\n", filestr, line);
		break;
	case -11:
		printf("\n More output surfaces expected. %s %d\n", filestr,
		       line);
		break;
	case -12:
		printf("\n Operation aborted. %s %d\n", filestr, line);
		break;
	case -13:
		printf("\n HW device lost. %s %d\n", filestr, line);
		break;
	case -14:
		printf("\n Incompatible video parameters. %s %d\n", filestr,
		       line);
		break;
	case -15:
		printf("\n Invalid video parameters. %s %d\n", filestr, line);
		break;
	case -16:
		printf("\n Undefined behavior. %s %d\n", filestr, line);
		break;
	case -17:
		printf("\n Device operation failure. %s %d\n", filestr, line);
		break;
	case -18:
		printf("\n More bitstream data expected. %s %d\n", filestr,
		       line);
		break;
	case -19:
		printf("\n Incompatible audio parameters. %s %d\n", filestr,
		       line);
		break;
	case -20:
		printf("\n Invalid audio parameters. %s %d\n", filestr, line);
		break;
	default:
		printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
	}
}

enum qsv_cpu_platform qsv_get_cpu_platform()
{
	using std::string;

	int cpuInfo[4];
	util_cpuid(cpuInfo, 0);

	string vendor;
	vendor += string(reinterpret_cast<char *>(&cpuInfo[1]), 4);
	vendor += string(reinterpret_cast<char *>(&cpuInfo[3]), 4);
	vendor += string(reinterpret_cast<char *>(&cpuInfo[2]), 4);

	if (vendor != "GenuineIntel")
		return QSV_CPU_PLATFORM_UNKNOWN;

	util_cpuid(cpuInfo, 1);
	uint8_t model = ((cpuInfo[0] >> 4) & 0xF) + ((cpuInfo[0] >> 12) & 0xF0);
	uint8_t family =
		((cpuInfo[0] >> 8) & 0xF) + ((cpuInfo[0] >> 20) & 0xFF);

	// See Intel 64 and IA-32 Architectures Software Developer's Manual,
	// Vol 3C Table 35-1
	if (family != 6)
		return QSV_CPU_PLATFORM_UNKNOWN;

	switch (model) {
	case 0x1C:
	case 0x26:
	case 0x27:
	case 0x35:
	case 0x36:
		return QSV_CPU_PLATFORM_BNL;

	case 0x2a:
	case 0x2d:
		return QSV_CPU_PLATFORM_SNB;

	case 0x3a:
	case 0x3e:
		return QSV_CPU_PLATFORM_IVB;

	case 0x37:
	case 0x4A:
	case 0x4D:
	case 0x5A:
	case 0x5D:
		return QSV_CPU_PLATFORM_SLM;

	case 0x4C:
		return QSV_CPU_PLATFORM_CHT;

	case 0x3c:
	case 0x3f:
	case 0x45:
	case 0x46:
		return QSV_CPU_PLATFORM_HSW;
	case 0x3d:
	case 0x47:
	case 0x4f:
	case 0x56:
		return QSV_CPU_PLATFORM_BDW;

	case 0x4e:
	case 0x5e:
		return QSV_CPU_PLATFORM_SKL;
	case 0x5c:
		return QSV_CPU_PLATFORM_APL;
	case 0x8e:
	case 0x9e:
		return QSV_CPU_PLATFORM_KBL;
	case 0x7a:
		return QSV_CPU_PLATFORM_GLK;
	case 0x66:
		return QSV_CPU_PLATFORM_CNL;
	case 0x7d:
	case 0x7e:
		return QSV_CPU_PLATFORM_ICL;
	case 0x8D:
		return QSV_CPU_PLATFORM_TGL;
	case 0x8C:
		return QSV_CPU_PLATFORM_TGL;
	case 0xA7:
		return QSV_CPU_PLATFORM_RKL;
	case 0x9A:
		return QSV_CPU_PLATFORM_ADL;
	case 0x97:
		return QSV_CPU_PLATFORM_ADL;
	case 0xB7:
		return QSV_CPU_PLATFORM_RPL;
	}

	//assume newer revisions are at least as capable as Haswell
	return QSV_CPU_PLATFORM_INTEL;
}
