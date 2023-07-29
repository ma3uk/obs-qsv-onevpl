#pragma once

#include "common_utils.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <atlbase.h>
#include <string_view>
#define DEVICE_MGR_TYPE MFX_HANDLE_D3D11_DEVICE

// =================================================================
// DirectX functionality required to manage D3D surfaces
//

// Create DirectX 11 device context
// - Required when using D3D surfaces.
// - D3D Device created and handed to Intel Media SDK
// - Intel graphics device adapter will be determined automatically (does not have to be primary),
//   but with the following caveats:
//     - Device must be active (but monitor does NOT have to be attached)
//     - Device must be enabled in BIOS. Required for the case when used together with a discrete graphics card
//     - For switchable graphics solutions (mobile) make sure that Intel device is the active device
mfxStatus CreateHWDevice(mfxSession session, mfxHDL *deviceHandle, HWND hWindow);
mfxStatus CreateVideoProcessor(mfxFrameSurface1 *pSurface);
void CleanupHWDevice();
void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx);
CComPtr<ID3D11DeviceContext> GetHWDeviceContext();
void ClearYUVSurfaceD3D(mfxMemId memId);
void ClearRGBSurfaceD3D(mfxMemId memId);
LUID GetHWDeviceLUID();
CComPtr<ID3D11DeviceContext> GetHWDeviceContext();
std::wstring GetHWDeviceName();
mfxStatus FillSCD1(DXGI_SWAP_CHAIN_DESC1 &scd1);
mfxStatus Reset(mfxHDL *deviceHandle);
// =================================================================
// Usage of the following two macros are only required for certain Windows DirectX11 use cases
constexpr auto WILL_READ = 0x1000;
constexpr auto WILL_WRITE = 0x2000;
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
