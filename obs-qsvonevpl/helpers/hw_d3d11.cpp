#include "hw_d3d11.hpp"

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//
hw_handle::hw_handle()
    : session(nullptr), tex_counter(), tex_pool(), handled_tex_pool() {}

hw_handle::~hw_handle() { release_device(); }

IDXGIAdapter *hw_handle::GetIntelDeviceAdapterHandle() {
  mfxU32 adapterNum = 0;
  mfxIMPL impl;

  MFXQueryIMPL(session, &impl);

  mfxIMPL baseImpl =
      MFX_IMPL_BASETYPE(impl); // Extract Media SDK base implementation type

  // get corresponding adapter number
  for (mfxU8 i = 0; i < sizeof(implTypes) / sizeof(implTypes[0]); i++) {
    if (implTypes[i].impl == baseImpl) {
      adapterNum = implTypes[i].adapterID;
      break;
    }
  }

  HRESULT hres = CreateDXGIFactory1(__uuidof(IDXGIFactory2),
                                    reinterpret_cast<void **>(&hw_factory));
  if (FAILED(hres)) {
    return nullptr;
  }

  IDXGIAdapter *adapter;
  hres = hw_factory->EnumAdapters(adapterNum, &adapter);
  if (FAILED(hres)) {
    return nullptr;
  }

  return adapter;
}

// Create HW device context
mfxStatus hw_handle::create_device(mfxSession input_session, int deviceNum) {
  session = input_session;
  if (encoder_counter == 0) {

    HRESULT hr = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hw_adapter = GetIntelDeviceAdapterHandle();
    if (nullptr == hw_adapter) {
      return MFX_ERR_DEVICE_FAILED;
    }

    UINT dxFlags = 0;
    //UINT dxFlags = D3D11_CREATE_DEVICE_DEBUG;

    hr = D3D11CreateDevice(
        hw_adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, dxFlags, FeatureLevels,
        (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])), D3D11_SDK_VERSION,
        &hw_device, &pFeatureLevelsOut, &hw_context);
    if (FAILED(hr)) {
      // error_hr("D3D11CreateDevice error");
      return MFX_ERR_DEVICE_FAILED;
    }

    // turn on multithreading for the DX11 context
    CComQIPtr<ID3D10Multithread> p_mt(hw_context);
    if (p_mt) {
      p_mt->SetMultithreadProtected(true);
    } else {
      return MFX_ERR_DEVICE_FAILED;
    }

    device_handle = static_cast<mfxHDL>(hw_device);
  }
  return MFX_ERR_NONE;
}

// Free HW device context
void hw_handle::release_device() {
  free_tex();
  free_handled_tex();

  if (encoder_counter <= 0) {
    if (hw_adapter) {
      hw_adapter->Release();
      hw_adapter = nullptr;
    }
    if (hw_device) {
      hw_device->Release();
      hw_device = nullptr;
    }
    if (hw_context) {
      hw_context->Release();
      hw_context = nullptr;
    }
    if (hw_factory) {
      hw_factory->Release();
      hw_factory = nullptr;
    }

    device_handle = nullptr;
  }
}

mfxStatus hw_handle::allocate_tex(mfxFrameAllocRequest *request) {
  mfxStatus sts = MFX_ERR_NONE;
  HRESULT hr;
  // warn("Res: %d x %d", request->Info.Width, request->Info.Height);
  //  Determine texture format
  DXGI_FORMAT format;
  if (MFX_FOURCC_NV12 == request->Info.FourCC) {
    format = DXGI_FORMAT_NV12;
  } else if (MFX_FOURCC_RGB4 == request->Info.FourCC) {
    format = DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (MFX_FOURCC_YUY2 == request->Info.FourCC) {
    format = DXGI_FORMAT_YUY2;
  } else if (MFX_FOURCC_P8 ==
             request->Info
                 .FourCC) //|| MFX_FOURCC_P8_TEXTURE == request->Info.FourCC
  {
    format = DXGI_FORMAT_P8;
  } else if (MFX_FOURCC_P010 == request->Info.FourCC) {
    format = DXGI_FORMAT_P010;
  } else {
    return MFX_ERR_UNSUPPORTED;
  }

  D3D11_TEXTURE2D_DESC desc = {0};

  desc.Width = request->Info.Width;
  desc.Height = request->Info.Height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = (D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER |
                    D3D11_BIND_SHADER_RESOURCE);
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  
  ID3D11Texture2D *pTexture2D = nullptr;
  // Create textures
  tex_pool.reserve(request->NumFrameSuggested);

  for (size_t i = 0; i < request->NumFrameSuggested; i++) {
    hr = hw_device->CreateTexture2D(&desc, nullptr, &pTexture2D);

    if (FAILED(hr)) {
      // error_hr("CreateTexture2D error");
      return MFX_ERR_MEMORY_ALLOC;
    }
    pTexture2D->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    tex_pool.push_back(pTexture2D);
  }

  return sts;
}

mfxStatus hw_handle::copy_tex(mfxSurfaceD3D11Tex2D &out_tex, mfxU32 tex_handle,
                              mfxU64 lock_key, mfxU64 *next_key) {

  IDXGIKeyedMutex *km = nullptr;
  ID3D11Texture2D *input_tex = nullptr;
  HRESULT hr = S_OK;

  for (size_t i = 0; i < handled_tex_pool.size(); i++) {
    struct handled_texture *ht = &handled_tex_pool[i];
    if (ht->handle == tex_handle) {
      input_tex = ht->texture;
      km = ht->km;
      break;
    }
  }

  if (!input_tex || input_tex == nullptr) {

    hr = hw_device->OpenSharedResource(
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(tex_handle)),
        IID_ID3D11Texture2D, reinterpret_cast<void **>(&input_tex));
    if (FAILED(hr)) {
      // error_hr("OpenSharedResource error");
      return MFX_ERR_INVALID_HANDLE;
    }

    hr = input_tex->QueryInterface(IID_IDXGIKeyedMutex,
                                   reinterpret_cast<void **>(&km));
    if (FAILED(hr)) {
      input_tex->Release();
      // error_hr("QueryInterface error");
      return MFX_ERR_INVALID_HANDLE;
    }

    input_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    struct handled_texture new_ht = {tex_handle, input_tex, km};
    handled_tex_pool.push_back(new_ht);
  }
  // warn("Handled size: %d, Tex size: %d", handled_tex_pool.size(),
  //      tex_pool.size());
  // info("-------------------");
  km->AcquireSync(lock_key, INFINITE);

  D3D11_TEXTURE2D_DESC desc = {0};
  input_tex->GetDesc(&desc);
  D3D11_BOX SrcBox = {0, 0, 0, desc.Width, desc.Height, 1};
  hw_context->CopySubresourceRegion(tex_pool[tex_counter], 0, 0, 0, 0,
                                    input_tex, 0, &SrcBox);
  km->ReleaseSync(*next_key);

  out_tex.texture2D = tex_pool[tex_counter];

  if (++tex_counter == tex_pool.size()) {
    tex_counter = 0;
  }

  return MFX_ERR_NONE;
}

mfxStatus hw_handle::free_tex() {
  if (!tex_pool.empty()) {
    for (mfxU32 i = 0; i < tex_pool.size(); i++) {
      if (tex_pool[i] && tex_pool[i] != nullptr) {
        tex_pool[i]->Release();
      }
    }

    tex_pool.clear();
    tex_pool.shrink_to_fit();
  }
  return MFX_ERR_NONE;
}

mfxStatus hw_handle::free_handled_tex() {
  if (!handled_tex_pool.empty()) {
    for (mfxU32 i = 0; i < handled_tex_pool.size(); i++) {
      struct handled_texture *ht = &handled_tex_pool[i];
      if (ht->texture && ht->texture != nullptr) {
        ht->km->Release();
        ht->texture->Release();
      }
    }

    handled_tex_pool.clear();
    handled_tex_pool.shrink_to_fit();
  }
  return MFX_ERR_NONE;
}