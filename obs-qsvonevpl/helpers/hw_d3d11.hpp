#pragma once

#if defined(_WIN32) || defined(_WIN64)

#ifndef __QSV_VPL_HWManagerS_H__
#define __QSV_VPL_HWManagerS_H__
#endif

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "common_utils.hpp"
#endif
#ifndef __QSV_VPL_EXT_BUF_MANAGER_H__
#include "ext_buf_manager.hpp"
#endif
extern "C" {
#include <obs.h>
}
class HWManager {
public:
  HWManager();
  ~HWManager();

  void ReleaseDevice();
  void ReleaseTexturePool();

  mfxStatus CreateDevice(mfxIMPL &Impl);
  mfxStatus CopyTexture(mfxSurfaceD3D11Tex2D &OuterTexture, void *TextureHandle,
                        mfxU64 LockKey, mfxU64 *NextKey);

  mfxStatus AllocateTexturePool(MFXVideoParam &EncodeParams);

  static inline int HWEncoderCounter = 0;
  static inline mfxHDL HWDeviceHandle = nullptr;

private:
 
  struct handled_texture {
    uint32_t Handle;
    ID3D11Texture2D *Texture;
    IDXGIKeyedMutex *KeyedMutex;
  };

  IDXGIAdapter *GetIntelDeviceAdapterHandle(mfxIMPL &Impl);
  mfxStatus FreeTexturePool();
  mfxStatus FreeHandledTexturePool();

  int HWTextureCounter;

  std::vector<ID3D11Texture2D *> HWTexturePool;
  std::vector<handled_texture> HWHandledTexturePool;

  static inline ID3D11Device *HWDevice;
  static inline ID3D11DeviceContext *HWContext;
  static inline IDXGIFactory2 *HWFactory;
  static inline IDXGIAdapter *HWAdapter;
};
#endif