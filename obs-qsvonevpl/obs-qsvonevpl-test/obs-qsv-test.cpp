#define MFX_DEPRECATED_OFF
#include "../libvpl/api/mfx.h"
#include "../helpers/common_utils.h"
#include <intrin.h>
#include <util/windows/ComPtr.hpp>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>
#include <string>
#include <map>

#ifdef _MSC_VER
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

#define INTEL_VENDOR_ID 0x8086

enum qsv_cpu_platform {
	QSV_CPU_PLATFORM_UNKNOWN,
	QSV_CPU_PLATFORM_BNL,
	QSV_CPU_PLATFORM_SNB,
	QSV_CPU_PLATFORM_IVB,
	QSV_CPU_PLATFORM_SLM,
	QSV_CPU_PLATFORM_CHT,
	QSV_CPU_PLATFORM_HSW,
	QSV_CPU_PLATFORM_BDW,
	QSV_CPU_PLATFORM_SKL,
	QSV_CPU_PLATFORM_APL,
	QSV_CPU_PLATFORM_KBL,
	QSV_CPU_PLATFORM_GLK,
	QSV_CPU_PLATFORM_CNL,
	QSV_CPU_PLATFORM_ICL,
	QSV_CPU_PLATFORM_TGL,
	QSV_CPU_PLATFORM_RKL,
	QSV_CPU_PLATFORM_ADL,
	QSV_CPU_PLATFORM_RPL,
	QSV_CPU_PLATFORM_INTEL
};

enum qsv_cpu_platform qsv_get_cpu_platform()
{
	using std::string;

	int cpuInfo[4];
	__cpuid(cpuInfo, 0);

	string vendor;
	vendor += string((char *)&cpuInfo[1], 4);
	vendor += string((char *)&cpuInfo[3], 4);
	vendor += string((char *)&cpuInfo[2], 4);

	if (vendor != "GenuineIntel")
		return QSV_CPU_PLATFORM_UNKNOWN;

	__cpuid(cpuInfo, 1);
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
using std::string;
struct adapter_caps {
	bool is_intel = false;
	bool is_dgpu = false;
	bool supports_av1 = false;
	bool supports_hevc = false;
	bool supports_vp9 = false;
	int video_memory = 0;
	unsigned int deviceID = 0;
	unsigned int deviceIDl = 0;
	string GPUArch = "";
};

static std::vector<uint64_t> luid_order;
static std::map<uint32_t, adapter_caps> adapter_info;

static inline uint32_t get_adapter_idx(uint32_t adapter_idx, LUID luid)
{
	for (size_t i = 0; i < luid_order.size(); i++) {
		if (luid_order[i] == *(uint64_t *)&luid) {
			return (uint32_t)i;
		}
	}

	return adapter_idx;
}

static bool get_adapter_caps(IDXGIFactory *factory, uint32_t adapter_idx)
{
	mfxIMPL impls[4] = {MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE2,
			    MFX_IMPL_HARDWARE3, MFX_IMPL_HARDWARE4};
	HRESULT hr;

	ComPtr<IDXGIAdapter> adapter;
	hr = factory->EnumAdapters(adapter_idx, &adapter);
	if (FAILED(hr))
		return false;

	DXGI_ADAPTER_DESC desc;
	adapter->GetDesc(&desc);

	uint32_t luid_idx = get_adapter_idx(adapter_idx, desc.AdapterLuid);
	adapter_caps &caps = adapter_info[luid_idx];
	if (desc.VendorId != INTEL_VENDOR_ID)
		return true;
	caps.deviceID = desc.DeviceId & 0xFF00;
	caps.deviceIDl = desc.DeviceId & 0x00FF;

	size_t video_memory = (desc.DedicatedVideoMemory > 0)
				      ? desc.DedicatedSystemMemory
				      : desc.DedicatedVideoMemory;

	mfxIMPL impl = impls[adapter_idx];

	caps.is_intel = (desc.VendorId == 0x8086);
	caps.is_dgpu = (desc.DedicatedVideoMemory >
			static_cast<unsigned long long>(512 * 1024 * 1024)) ||
		       (caps.deviceID >= 0x5600);
	enum qsv_cpu_platform qsv_platform = qsv_get_cpu_platform();
	caps.supports_av1 =
		(caps.is_intel && caps.is_dgpu) ||
		(caps.is_intel && (qsv_platform > QSV_CPU_PLATFORM_RPL));
	caps.supports_hevc =
		((qsv_platform > QSV_CPU_PLATFORM_KBL) || caps.is_dgpu);
	caps.supports_vp9 =
		((qsv_platform > QSV_CPU_PLATFORM_KBL) || caps.is_dgpu);

	return true;
}

#define CHECK_TIMEOUT_MS 10000

DWORD WINAPI TimeoutThread(LPVOID param)
{
	HANDLE hMainThread = (HANDLE)param;

	DWORD ret = WaitForSingleObject(hMainThread, CHECK_TIMEOUT_MS);
	if (ret == WAIT_TIMEOUT)
		TerminateProcess(GetCurrentProcess(), STATUS_TIMEOUT);

	CloseHandle(hMainThread);

	return 0;
}

int main(int argc, char *argv[])
try {
	ComPtr<IDXGIFactory> factory;
	HRESULT hr;

	HANDLE hMainThread;
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
			GetCurrentProcess(), &hMainThread, 0, FALSE,
			DUPLICATE_SAME_ACCESS);
	DWORD threadId;
	HANDLE hThread;
	hThread =
		CreateThread(NULL, 0, TimeoutThread, hMainThread, 0, &threadId);
	if (hThread != 0) {
		CloseHandle(hThread);
	}
	/* --------------------------------------------------------- */
	/* parse expected LUID order                                 */

	for (int i = 1; i < argc; i++) {
		luid_order.push_back(strtoull(argv[i], NULL, 16));
	}

	/* --------------------------------------------------------- */
	/* query qsv support                                         */

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void **)&factory);
	if (FAILED(hr))
		throw "CreateDXGIFactory1 failed";

	uint32_t idx = 0;

	while (get_adapter_caps(factory, idx++) && idx < 4)
		;

	for (auto &[idx, caps] : adapter_info) {
		printf("[%u]\n", idx);
		printf("is_intel=%s\n", caps.is_intel ? "true" : "false");
		printf("is_dgpu=%s\n", caps.is_dgpu ? "true" : "false");
		printf("supports_av1=%s\n",
		       caps.supports_av1 ? "true" : "false");
		printf("supports_hevc=%s\n",
		       caps.supports_hevc ? "true" : "false");
		printf("supports_vp9=%s\n",
		       caps.supports_vp9 ? "true" : "false");
	}

	return 0;
} catch (const char *text) {
	printf("[error]\nstring=%s\n", text);
	return 0;
}
