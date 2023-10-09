#pragma once

#ifndef __QSV_VPL_D3D_COMMON_H__
#define __QSV_VPL_D3D_COMMON_H__
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define D3D11_IGNORE_SDK_LAYERS

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "common_utils.hpp"
#endif
#ifndef __d3d10_1_h__
#include <d3d10_1.h>
#endif
#ifndef __d3d10_h__
#include <d3d10.h>
#endif
#ifndef __d3d11_h__
#include <d3d11.h>
#endif
#ifndef __d3dcommon_h__
#include <d3dcommon.h>
#endif
#ifndef __dxgi1_6_h__
#include <dxgi1_6.h>
#endif
#ifndef __dxgi1_2_h__
#include <dxgi1_2.h>
#endif
#ifndef __dxgiformat_h__
#include <dxgiformat.h>
#endif
#ifndef __dxgitype_h__
#include <dxgitype.h>
#endif
#ifndef _INC_VADEFS
#include <vadefs.h>
#endif
#ifndef _WINERROR_
#include <winerror.h>
#endif
#ifndef _MINWINDEF_
#include <minwindef.h>
#endif
#ifndef _WINNT_
#include <winnt.h>
#endif
#ifndef __ATLCOMCLI_H__
#include <atlcomcli.h>
#endif
#ifndef __ATLBASE_H__
#include <atlbase.h>
#endif
#ifndef _STRING_VIEW_
#include <string_view>
#endif
#ifndef _STRING_
#include <string>
#endif
#ifndef _NEW_
#include <new>
#endif
#ifndef _MAP_
#include <map>
#endif
#ifndef __MFX_H__
#include <vpl/mfx.h>
#endif

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
  QSV_VPL_D3D11();
  ~QSV_VPL_D3D11();
  void SetSession(mfxSession mfxsession);
  mfxStatus CreateHWDevice(mfxHDL *deviceHandle, mfxU32 deviceNum);
  void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx);
  void CleanupHWDevice();
  CComPtr<ID3D11DeviceContext> GetHWDeviceContext() const;

  std::wstring GetHWDeviceName() const;
  LUID GetHWDeviceLUID() const;

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
  CComPtr<IDXGISwapChain> g_pSwapChain;

  std::map<mfxMemId *, mfxHDL> allocResponses;
  std::map<mfxHDL, mfxFrameAllocResponse> allocDecodeResponses;
  std::map<mfxHDL, int> allocDecodeRefCount;

private:
  mfxSession session;
};
