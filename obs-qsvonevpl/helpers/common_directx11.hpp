#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "common_utils.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <atlbase.h>
#include <string_view>
#include <map>
#include <vpl/mfx.h>
#define DEVICE_MGR_TYPE MFX_HANDLE_D3D11_DEVICE
#define WILL_READ 0x1000
#define WILL_WRITE 0x2000

const struct {
	mfxIMPL impl;     // actual implementation
	mfxU32 adapterID; // device adapter number
} implTypes[] = {{MFX_IMPL_HARDWARE, 0},
		 {MFX_IMPL_HARDWARE2, 1},
		 {MFX_IMPL_HARDWARE3, 2},
		 {MFX_IMPL_HARDWARE4, 3}};

class QSV_VPL_D3D11 {
public:
	QSV_VPL_D3D11(mfxSession session);
	~QSV_VPL_D3D11();
	mfxStatus CreateHWDevice(mfxHDL *deviceHandle, uint32_t deviceNum);
	void CleanupHWDevice();

	mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
				 mfxU64 lock_key, mfxU64 *next_key);

protected:
	mfxStatus FillSCD(DXGI_SWAP_CHAIN_DESC &scd);
	IDXGIAdapter *GetIntelDeviceAdapterHandle();

	struct CustomMemId {
		mfxMemId memId;
		mfxMemId memIdStage;
		mfxU16 rw;
	};

	ID3D11Device *g_pD3D11Device;
	ID3D11DeviceContext *g_pD3D11Ctx;
	IDXGIFactory2 *g_pDXGIFactory;
	IDXGIAdapter *g_pAdapter;
	CComPtr<IDXGISwapChain1> g_pSwapChain;

private:
	mfxSession session;
};
