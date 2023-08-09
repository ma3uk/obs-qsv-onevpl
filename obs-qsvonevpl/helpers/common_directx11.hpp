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
#include "mfxvideo++.h"
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
	mfxStatus CreateHWDevice(mfxHDL *deviceHandle);
	mfxStatus CreateVideoProcessor(mfxFrameSurface1 *pSurface);
	void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx);
	void CleanupHWDevice();
	mfxStatus Reset(mfxHDL *deviceHandle);
	CComPtr<ID3D11DeviceContext> GetHWDeviceContext();

	std::wstring GetHWDeviceName();
	LUID GetHWDeviceLUID();

	mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
			       mfxFrameAllocResponse *response);
	mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
	mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
	mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
	mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response);
	mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
				 mfxU64 lock_key, mfxU64 *next_key);

protected:
	mfxStatus FillSCD(DXGI_SWAP_CHAIN_DESC &scd);
	mfxStatus FillSCD1(DXGI_SWAP_CHAIN_DESC1 &scd1);
	IDXGIAdapter *GetIntelDeviceAdapterHandle();

	mfxStatus _simple_alloc(mfxFrameAllocRequest *request,
				mfxFrameAllocResponse *response);
	mfxStatus _simple_free(mfxFrameAllocResponse *response);

	struct CustomMemId {
		mfxMemId memId;
		mfxMemId memIdStage;
		mfxU16 rw;
	};

	ID3D11Device *g_pD3D11Device;
	ID3D11DeviceContext *g_pD3D11Ctx;
	IDXGIFactory2 *g_pDXGIFactory;
	IDXGIAdapter *g_pAdapter;
	std::wstring g_displayDeviceName;
	LUID g_devLUID;
	BOOL g_bIsA2rgb10 = false;
	mfxU16 n_Views = 0;
	CComPtr<IDXGISwapChain1> g_pSwapChain;

	std::map<mfxMemId *, mfxHDL> allocResponses;
	std::map<mfxHDL, mfxFrameAllocResponse> allocDecodeResponses;
	std::map<mfxHDL, int> allocDecodeRefCount;

private:
	mfxSession session;
};
