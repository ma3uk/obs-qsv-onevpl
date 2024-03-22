#pragma once

#if defined(_WIN32) || defined(_WIN64)

#ifndef __QSV_VPL_HW_HANDLES_H__
#define __QSV_VPL_HW_HANDLES_H__
#endif

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "common_utils.hpp"
#endif

#include <obs.h>

class hw_handle {
public:
  hw_handle();
  ~hw_handle();

  void release_device();
  void release_tex();

  mfxStatus create_device(mfxSession session);
  mfxStatus copy_tex(mfxSurfaceD3D11Tex2D &out_tex, void *tex_handle,
                     mfxU64 lock_key, mfxU64 *next_key);

  mfxStatus allocate_tex(mfxFrameAllocRequest *request);

  static inline int encoder_counter = 0;
  static inline mfxHDL device_handle = nullptr;

private:
 
  struct handled_texture {
    uint32_t handle;
    ID3D11Texture2D *texture;
    IDXGIKeyedMutex *km;
  };

  mfxSession session;

  IDXGIAdapter *GetIntelDeviceAdapterHandle();
  mfxStatus free_tex();
  mfxStatus free_handled_tex();

  int tex_counter;

  std::vector<ID3D11Texture2D *> tex_pool;
  std::vector<handled_texture> handled_tex_pool;

  static inline ID3D11Device *hw_device;
  static inline ID3D11DeviceContext *hw_context;
  static inline IDXGIFactory2 *hw_factory;
  static inline IDXGIAdapter *hw_adapter;
};
#endif