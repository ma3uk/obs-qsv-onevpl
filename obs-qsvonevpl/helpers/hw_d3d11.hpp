#pragma once
#define D3D11_IGNORE_SDK_LAYERS

#include "common_utils.hpp"

#include <Windows.h>
#include <atlbase.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3d11_1.h>
#include <d3d10_1.h>
#include <d3dcommon.h>
#include <dxgitype.h>
#include <vector>
#include <obs.h>
#include <obs-encoder.h>

#define DEVICE_MGR_TYPE MFX_HANDLE_D3D11_DEVICE

const struct {
  mfxIMPL impl;     // actual implementation
  mfxU32 adapterID; // device adapter number
} implTypes[] = {{MFX_IMPL_HARDWARE, 0},
                 {MFX_IMPL_HARDWARE2, 1},
                 {MFX_IMPL_HARDWARE3, 2},
                 {MFX_IMPL_HARDWARE4, 3}};

class hw_handle {
public:
  hw_handle();
  ~hw_handle();
  void release_device();

  mfxStatus create_device(mfxSession session);
  mfxStatus copy_tex(mfxSurfaceD3D11Tex2D &out_tex, void* tex_handle,
                     mfxU64 lock_key, mfxU64 *next_key);

  mfxStatus allocate_tex(mfxFrameAllocRequest *request);

  static inline int encoder_counter;
  static inline mfxHDL device_handle;

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