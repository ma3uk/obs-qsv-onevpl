#include "hw_d3d11.hpp"

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//
hw_handle::hw_handle()
    : session(nullptr), tex_counter(), tex_pool(), handled_tex_pool() {}

hw_handle::~hw_handle() {
  release_tex();
  release_device();
}

IDXGIAdapter *hw_handle::GetIntelDeviceAdapterHandle() {
  mfxU32 adapterNum = 0;
  mfxIMPL impl;

  const std::array<mfxIMPL, 4> impls{MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE2,
                                     MFX_IMPL_HARDWARE3, MFX_IMPL_HARDWARE4};
  MFXQueryIMPL(session, &impl);

  mfxIMPL baseImpl =
      MFX_IMPL_BASETYPE(impl); // Extract Media SDK base implementation type

  // get corresponding adapter number
  for (mfxU32 i = 0; i < sizeof(impls) / sizeof(impls[0]); i++) {
    if (impls[i] == baseImpl) {
      adapterNum = i;
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
mfxStatus hw_handle::create_device(mfxSession input_session) {
  session = std::move(input_session);
  if (encoder_counter == 0) {

    HRESULT hr = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hw_adapter = GetIntelDeviceAdapterHandle();
    if (nullptr == hw_adapter) {
      throw std::runtime_error("create_device(): Adapter is nullptr");
    }

    //UINT dxFlags = 0;
    UINT dxFlags =
        /*D3D11_CREATE_DEVICE_DEBUG |*/ D3D11_CREATE_DEVICE_BGRA_SUPPORT |
        D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT |
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT |
        D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;

    hr = D3D11CreateDevice(
        hw_adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, dxFlags, FeatureLevels,
        (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])), D3D11_SDK_VERSION,
        &hw_device, &pFeatureLevelsOut, &hw_context);
    if (FAILED(hr)) {
      throw std::runtime_error("create_device(): D3D11CreateDevice error");
    }

    ID3D10Multithread *pD10Multithread = nullptr;
    hr = hw_context->QueryInterface(IID_ID3D10Multithread,
                               reinterpret_cast<void **>(&pD10Multithread));
    if (FAILED(hr)) {
      throw std::runtime_error(
          "create_device(): SetMultithreadProtected error");
    }
    pD10Multithread->SetMultithreadProtected(true);

    device_handle = static_cast<mfxHDL>(hw_device);
  }
  return MFX_ERR_NONE;
}

// Free HW device context
void hw_handle::release_device() {
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
    throw std::runtime_error("allocate_tex(): Unsupported format");
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
      throw std::runtime_error("allocate_tex(): CreateTexture2D error");
    }
    pTexture2D->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    tex_pool.push_back(std::move(pTexture2D));
  }

  return sts;
}

mfxStatus hw_handle::copy_tex(mfxSurfaceD3D11Tex2D &out_tex, void *tex_handle,
                              mfxU64 lock_key, mfxU64 *next_key) {

  IDXGIKeyedMutex *km = nullptr;
  ID3D11Texture2D *input_tex = nullptr;
  HRESULT hr = S_OK;

  struct encoder_texture *tex =
      std::move(static_cast<struct encoder_texture *>(tex_handle));

  for (size_t i = 0; i < handled_tex_pool.size(); i++) {
    struct handled_texture *ht = std::move(&handled_tex_pool[i]);
    if (ht->handle == tex->handle) {
      input_tex = ht->texture;
      km = ht->km;
      break;
    }
  }

  if (!input_tex) {

    hr = hw_device->OpenSharedResource(
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(tex->handle)),
        IID_ID3D11Texture2D, reinterpret_cast<void **>(&input_tex));
    if (FAILED(hr)) {
      throw std::runtime_error("copy_tex(): OpenSharedResource error");
    }

    hr = input_tex->QueryInterface(IID_IDXGIKeyedMutex,
                                   reinterpret_cast<void **>(&km));
    if (FAILED(hr)) {
      input_tex->Release();
      throw std::runtime_error("copy_tex(): QueryInterface error");
    }

    input_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    struct handled_texture new_ht = {std::move(tex->handle), input_tex, km};
    handled_tex_pool.push_back(std::move(new_ht));
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
    for (size_t i = 0; i < tex_pool.size(); i++) {
      if (tex_pool[i]) {
        tex_pool[i]->Release();
        tex_pool[i] = nullptr;
      }
    }

    tex_pool.clear();
    tex_pool.shrink_to_fit();
    tex_pool.~vector();
  }
  return MFX_ERR_NONE;
}

mfxStatus hw_handle::free_handled_tex() {
  if (!handled_tex_pool.empty()) {
    for (size_t i = 0; i < handled_tex_pool.size(); i++) {
      struct handled_texture *ht = std::move(&handled_tex_pool[i]);
      if (ht->texture) {
        ht->km->Release();
        ht->texture->Release();
        ht = nullptr;
      }
    }

    handled_tex_pool.clear();
    handled_tex_pool.shrink_to_fit();
    handled_tex_pool.~vector();
  }
  return MFX_ERR_NONE;
}

void hw_handle::release_tex() {
  free_tex();
  free_handled_tex();
}