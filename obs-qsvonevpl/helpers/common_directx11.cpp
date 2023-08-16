#include "common_directx11.hpp"

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//

QSV_VPL_D3D11::QSV_VPL_D3D11(mfxSession session)
	: g_pAdapter(),
	  g_pD3D11Ctx(),
	  g_pD3D11Device(),
	  g_pDXGIFactory()
{
	this->session = session;
}

QSV_VPL_D3D11::~QSV_VPL_D3D11()
{
	CleanupHWDevice();
}

mfxStatus QSV_VPL_D3D11::FillSCD(DXGI_SWAP_CHAIN_DESC &scd)
{
	scd.BufferDesc.Width = 0; // Use automatic sizing.
	scd.BufferDesc.Height = 0;
	scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.SampleDesc.Count = 1; // Don't use multi-sampling.
	scd.SampleDesc.Quality = 0;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2; // Use double buffering to minimize latency.
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	return MFX_ERR_NONE;
}

IDXGIAdapter *QSV_VPL_D3D11::GetIntelDeviceAdapterHandle()
{
	mfxU32 adapterNum = 0;
	mfxIMPL impl;

	MFXQueryIMPL(session, &impl);

	mfxIMPL baseImpl = MFX_IMPL_BASETYPE(
		impl); // Extract Media SDK base implementation type

	// get corresponding adapter number
	for (mfxU8 i = 0; i < sizeof(implTypes) / sizeof(implTypes[0]); i++) {
		if (implTypes[i].impl == baseImpl) {
			adapterNum = implTypes[i].adapterID;
			break;
		}
	}

	HRESULT hres = CreateDXGIFactory1(__uuidof(IDXGIFactory2),
					  (void **)(&g_pDXGIFactory));
	if (FAILED(hres))
		return NULL;

	IDXGIAdapter *adapter;
	hres = g_pDXGIFactory->EnumAdapters(adapterNum, &adapter);
	if (FAILED(hres))
		return NULL;

	return adapter;
}

// Create HW device context
mfxStatus QSV_VPL_D3D11::CreateHWDevice(mfxHDL *deviceHandle, mfxU32 deviceNum)
{
	HRESULT hres = S_OK;

	static D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1,
						    D3D_FEATURE_LEVEL_11_0,
						    D3D_FEATURE_LEVEL_10_1,
						    D3D_FEATURE_LEVEL_10_0};
	D3D_FEATURE_LEVEL pFeatureLevelsOut;

	g_pAdapter = GetIntelDeviceAdapterHandle();
	if (NULL == g_pAdapter) {
		return MFX_ERR_DEVICE_FAILED;
	}

	UINT dxFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT |
		       D3D11_CREATE_DEVICE_BGRA_SUPPORT |
		       D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {0};
	FillSCD(swapChainDesc);
	hres = D3D11CreateDeviceAndSwapChain(
		g_pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, dxFlags,
		FeatureLevels, _countof(FeatureLevels), D3D11_SDK_VERSION,
		&swapChainDesc, NULL, &g_pD3D11Device, &pFeatureLevelsOut,
		&g_pD3D11Ctx);
	if (FAILED(hres)) {
		return MFX_ERR_DEVICE_FAILED;
	}

	DXGI_ADAPTER_DESC desc;
	g_pAdapter->GetDesc(&desc);

	// turn on multithreading for the DX11 context
	CComQIPtr<ID3D10Multithread> p_mt(g_pD3D11Ctx);
	if (p_mt) {
		p_mt->SetMultithreadProtected(true);
	} else {
		return MFX_ERR_DEVICE_FAILED;
	}

	*deviceHandle = static_cast<mfxHDL>(g_pD3D11Device);

	return MFX_ERR_NONE;
}

// Free HW device context
void QSV_VPL_D3D11::CleanupHWDevice()
{
	if (g_pAdapter) {
		g_pAdapter->Release();
		g_pAdapter = nullptr;
	}
	if (g_pD3D11Device) {
		g_pD3D11Device->Release();
		g_pD3D11Device = nullptr;
	}
	if (g_pD3D11Ctx) {
		g_pD3D11Ctx->Release();
		g_pD3D11Ctx = nullptr;
	}
	if (g_pDXGIFactory) {
		g_pDXGIFactory->Release();
		g_pDXGIFactory = nullptr;
	}
}

mfxStatus QSV_VPL_D3D11::simple_copytex(mfxHDL pthis, mfxMemId mid,
					mfxU32 tex_handle, mfxU64 lock_key,
					mfxU64 *next_key)
{
	pthis; // To suppress warning for this unused parameter

	CustomMemId *memId = static_cast<CustomMemId *>(mid);
	ID3D11Texture2D *pSurface =
		static_cast<ID3D11Texture2D *>(memId->memId);

	IDXGIKeyedMutex *km = 0;
	ID3D11Texture2D *input_tex = 0;
	HRESULT hr;

	hr = g_pD3D11Device->OpenSharedResource(
		reinterpret_cast<HANDLE>(static_cast<uintptr_t>(tex_handle)),
		IID_ID3D11Texture2D, (void **)&input_tex);
	if (FAILED(hr)) {
		return MFX_ERR_INVALID_HANDLE;
	}

	hr = input_tex->QueryInterface(IID_IDXGIKeyedMutex, (void **)&km);
	if (FAILED(hr)) {
		input_tex->Release();
		return MFX_ERR_INVALID_HANDLE;
	}

	input_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

	km->AcquireSync(lock_key, INFINITE);

	D3D11_TEXTURE2D_DESC desc = {0};
	input_tex->GetDesc(&desc);
	D3D11_BOX SrcBox = {0, 0, 0, desc.Width, desc.Height, 1};
	g_pD3D11Ctx->CopySubresourceRegion(pSurface, 0, 0, 0, 0, input_tex, 0,
					   &SrcBox);

	km->ReleaseSync(*next_key);

	km->Release();
	input_tex->Release();

	return MFX_ERR_NONE;
}