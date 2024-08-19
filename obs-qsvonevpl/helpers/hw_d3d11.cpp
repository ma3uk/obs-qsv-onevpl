#include "hw_d3d11.hpp"

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//
HWManager::HWManager()
    : HWTextureCounter(), HWTexturePool(), HWHandledTexturePool() {}

HWManager::~HWManager() {
  ReleaseTexturePool();
  ReleaseDevice();
}

IDXGIAdapter *HWManager::GetIntelDeviceAdapterHandle(mfxIMPL &Impl) {

  HRESULT HR = S_OK;
  mfxU32 AdapterNum = 0;

  const std::array<mfxIMPL, 4> Impls{MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE2,
                                     MFX_IMPL_HARDWARE3, MFX_IMPL_HARDWARE4};

  mfxIMPL BaseImpl =
      MFX_IMPL_BASETYPE(Impl); // Extract Media SDK base implementation type

  // get corresponding adapter number
  for (mfxU32 i = 0; i < sizeof(Impls) / sizeof(Impls[0]); i++) {
    if (Impls[i] == BaseImpl) {
      AdapterNum = i;
      break;
    }
  }

  HR = CreateDXGIFactory1(__uuidof(IDXGIFactory2),
                          reinterpret_cast<void **>(&HWFactory));
  if (FAILED(HR)) {
    throw HR;
  }

  IDXGIAdapter *Adapter;
  HR = HWFactory->EnumAdapters(AdapterNum, &Adapter);
  if (FAILED(HR)) {
    throw HR;
  }

  return Adapter;
}

// Create HW device context
mfxStatus HWManager::CreateDevice(mfxIMPL &Impl) {
  try {

    if (HWEncoderCounter <= 0) {

      HRESULT HR = S_OK;

      static D3D_FEATURE_LEVEL FeatureLevels[] = {
          D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
      D3D_FEATURE_LEVEL FeatureLevelsOut;

      HWAdapter = GetIntelDeviceAdapterHandle(Impl);
      if (nullptr == HWAdapter) {
        throw std::runtime_error("Adapter is nullptr");
      }

      // UINT dxFlags = 0;
      UINT D3DFlags =
          /*D3D11_CREATE_DEVICE_DEBUG |*/ D3D11_CREATE_DEVICE_BGRA_SUPPORT |
          D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT |
          D3D11_CREATE_DEVICE_VIDEO_SUPPORT |
          D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;

      HR = D3D11CreateDevice(
          HWAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3DFlags, FeatureLevels,
          (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])), D3D11_SDK_VERSION,
          &HWDevice, &FeatureLevelsOut, &HWContext);
      if (FAILED(HR)) {
        throw std::runtime_error("D3D11CreateDevice error");
      }

      ID3D10Multithread *D3D10Multithread = nullptr;
      HR = HWContext->QueryInterface(
          IID_ID3D10Multithread, reinterpret_cast<void **>(&D3D10Multithread));
      if (FAILED(HR)) {
        throw std::runtime_error(
            "SetMultitHReadProtected error");
      }
      D3D10Multithread->SetMultithreadProtected(true);
      D3D10Multithread->Release();

      HWDeviceHandle = static_cast<mfxHDL>(HWDevice);
    }
    return MFX_ERR_NONE;
  } catch (const std::exception &e) {
    warn("CreateDevice(): Error %s", e.what());
    throw;
  }
}

// Free HW device context
void HWManager::ReleaseDevice() {
  if (HWEncoderCounter <= 0) {
    if (HWAdapter) {
      HWAdapter->Release();
      HWAdapter = nullptr;
    }
    if (HWDevice) {
      HWDevice->Release();
      HWDevice = nullptr;
    }
    if (HWContext) {
      HWContext->Release();
      HWContext = nullptr;
    }
    if (HWFactory) {
      HWFactory->Release();
      HWFactory = nullptr;
    }

    HWDeviceHandle = nullptr;
  }
}

mfxStatus HWManager::AllocateTexturePool(mfxFrameAllocRequest *Request) {
  mfxStatus Status = MFX_ERR_NONE;
  HRESULT HR = S_OK;
  // warn("Res: %d x %d", Request->Info.Width, Request->Info.Height);
  //  Determine texture Format
  DXGI_FORMAT Format;
  if (MFX_FOURCC_NV12 == Request->Info.FourCC) {
    Format = DXGI_FORMAT_NV12;
  } else if (MFX_FOURCC_RGB4 == Request->Info.FourCC) {
    Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  } else if (MFX_FOURCC_YUY2 == Request->Info.FourCC) {
    Format = DXGI_FORMAT_YUY2;
  } else if (MFX_FOURCC_P8 ==
             Request->Info
                 .FourCC) //|| MFX_FOURCC_P8_TEXTURE == Request->Info.FourCC
  {
    Format = DXGI_FORMAT_P8;
  } else if (MFX_FOURCC_P010 == Request->Info.FourCC) {
    Format = DXGI_FORMAT_P010;
  } else {
    throw std::runtime_error("AllocateHWTexturePool(): Unsupported Format");
  }

  D3D11_TEXTURE2D_DESC Desc = {0};

  Desc.Width = Request->Info.Width;
  Desc.Height = Request->Info.Height;
  Desc.MipLevels = 1;
  Desc.ArraySize = 1;
  Desc.Format = Format;
  Desc.SampleDesc.Count = 1;
  Desc.SampleDesc.Quality = 0;
  Desc.Usage = D3D11_USAGE_DEFAULT;
  Desc.BindFlags = (D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER |
                    D3D11_BIND_SHADER_RESOURCE);
  Desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  
  ID3D11Texture2D *Texture2D = nullptr;
  // Create textures
  HWTexturePool.reserve(static_cast<size_t>(Request->NumFrameSuggested + 100));

  for (size_t i = 0; i < static_cast<size_t>(Request->NumFrameSuggested +
                                             static_cast<mfxU16>(100));
       i++) {
    HR = HWDevice->CreateTexture2D(&Desc, nullptr, &Texture2D);

    if (FAILED(HR)) {
      throw std::runtime_error(
          "AllocateHWTexturePool(): CreateTexture2D error");
    }
    Texture2D->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    HWTexturePool.push_back(std::move(Texture2D));
  }

  return Status;
}

mfxStatus HWManager::CopyTexture(mfxSurfaceD3D11Tex2D &OuterTexture,
                                 void *TextureHandle, mfxU64 LockKey,
                                 mfxU64 *NextKey) {

  IDXGIKeyedMutex *KeyedMutex = nullptr;
  ID3D11Texture2D *InputTexture = nullptr;
  HRESULT HR = S_OK;

  struct encoder_texture *Texture =
      std::move(static_cast<struct encoder_texture *>(TextureHandle));

  if (!HWHandledTexturePool.empty()) {
    for (size_t i = 0; i < HWHandledTexturePool.size(); i++) {
      struct handled_texture *HandledTexture =
          std::move(&HWHandledTexturePool[i]);
      if (HandledTexture->Handle == Texture->handle) {
        InputTexture = HandledTexture->Texture;
        KeyedMutex = HandledTexture->KeyedMutex;
        break;
      }
    }
  }

  if (HWHandledTexturePool.empty() || !InputTexture) {
    HR = HWDevice->OpenSharedResource(
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Texture->handle)),
        IID_ID3D11Texture2D, reinterpret_cast<void **>(&InputTexture));
    if (FAILED(HR)) {
      throw std::runtime_error("CopyTexture(): OpenSharedResource error");
    }

    HR = InputTexture->QueryInterface(IID_IDXGIKeyedMutex,
                                      reinterpret_cast<void **>(&KeyedMutex));
    if (FAILED(HR)) {
      InputTexture->Release();
      throw std::runtime_error("CopyTexture(): QueryInterface error");
    }

    InputTexture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

    struct handled_texture NewHandledTexture = {std::move(Texture->handle),
                                                InputTexture, KeyedMutex};

    if (HWHandledTexturePool.empty()) {
      HWHandledTexturePool.reserve(240);
    }

    HWHandledTexturePool.push_back(std::move(NewHandledTexture));
  }
  // warn("Handled size: %d, Tex size: %d", HWHandledTexturePool.size(),
  //      HWTexturePool.size());
  // info("-------------------");
  KeyedMutex->AcquireSync(LockKey, INFINITE);

  D3D11_TEXTURE2D_DESC Desc = {0};
  InputTexture->GetDesc(&Desc);
  D3D11_BOX SrcBox = {0, 0, 0, Desc.Width, Desc.Height, 1};
  HWContext->CopySubresourceRegion(HWTexturePool[HWTextureCounter], 0, 0, 0, 0,
                                   InputTexture, 0, &SrcBox);
  KeyedMutex->ReleaseSync(*NextKey);

  OuterTexture.texture2D = HWTexturePool[HWTextureCounter];

  if (++HWTextureCounter == HWTexturePool.size()) {
    HWTextureCounter = 0;
  }

  return MFX_ERR_NONE;
}

mfxStatus HWManager::FreeTexturePool() {
  if (!HWTexturePool.empty()) {
    for (size_t i = 0; i < HWTexturePool.size(); i++) {
      if (HWTexturePool[i]) {
        HWTexturePool[i]->Release();
        HWTexturePool[i] = nullptr;
      }
    }

    HWTexturePool.clear();
    HWTexturePool.shrink_to_fit();
    HWTexturePool.~vector();
  }
  return MFX_ERR_NONE;
}

mfxStatus HWManager::FreeHandledTexturePool() {
  if (!HWHandledTexturePool.empty()) {
    for (size_t i = 0; i < HWHandledTexturePool.size(); i++) {
      struct handled_texture *HandledTexture =
          std::move(&HWHandledTexturePool[i]);
      if (HandledTexture->Texture) {
        HandledTexture->KeyedMutex->Release();
        HandledTexture->Texture->Release();
        HandledTexture = nullptr;
      }
    }

    HWHandledTexturePool.clear();
    HWHandledTexturePool.shrink_to_fit();
    HWHandledTexturePool.~vector();
  }
  return MFX_ERR_NONE;
}

void HWManager::ReleaseTexturePool() {
  FreeTexturePool();
  FreeHandledTexturePool();
}