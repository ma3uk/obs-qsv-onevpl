#include "common_directx11.h"

#include <map>

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

typedef struct {
	mfxMemId memId;
	mfxMemId memIdStage;
	mfxU16 rw;
} CustomMemId;

const struct {
	mfxIMPL impl;     // actual implementation
	mfxU32 adapterID; // device adapter number
} implTypes[] = {{MFX_IMPL_HARDWARE, 0},
		 {MFX_IMPL_HARDWARE2, 1},
		 {MFX_IMPL_HARDWARE3, 2},
		 {MFX_IMPL_HARDWARE4, 3}};

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//

mfxStatus FillSCD1(DXGI_SWAP_CHAIN_DESC1 &scd1)
{
	scd1.Width = 0; // Use automatic sizing.
	scd1.Height = 0;
	scd1.Format = (g_bIsA2rgb10) ? DXGI_FORMAT_R10G10B10A2_UNORM
				     : DXGI_FORMAT_B8G8R8A8_UNORM;
	scd1.Stereo = /*n_Views == 2 ? TRUE : */ FALSE;
	scd1.SampleDesc.Count = 1; // Don't use multi-sampling.
	scd1.SampleDesc.Quality = 0;
	scd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd1.BufferCount = 2; // Use double buffering to minimize latency.
	scd1.Scaling = DXGI_SCALING_STRETCH;
	scd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	return MFX_ERR_NONE;
}

IDXGIAdapter *GetIntelDeviceAdapterHandle(mfxSession session)
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
mfxStatus CreateHWDevice(mfxSession session, mfxHDL *deviceHandle, HWND hWindow)
{
	HRESULT hres = S_OK;

	static D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1,
						    D3D_FEATURE_LEVEL_11_0,
						    D3D_FEATURE_LEVEL_10_1,
						    D3D_FEATURE_LEVEL_10_0};
	D3D_FEATURE_LEVEL pFeatureLevelsOut;

	g_pAdapter = GetIntelDeviceAdapterHandle(session);
	if (NULL == g_pAdapter) {
		return MFX_ERR_DEVICE_FAILED;
	}

	UINT dxFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT |
		       D3D11_CREATE_DEVICE_BGRA_SUPPORT |
		       D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;
	//UINT dxFlags = D3D11_CREATE_DEVICE_DEBUG;

	hres = D3D11CreateDevice(g_pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
				 dxFlags, FeatureLevels,
				 _countof(FeatureLevels), D3D11_SDK_VERSION,
				 &g_pD3D11Device, &pFeatureLevelsOut,
				 &g_pD3D11Ctx);
	if (FAILED(hres)) {
		return MFX_ERR_DEVICE_FAILED;
	}

	DXGI_ADAPTER_DESC desc;
	g_pAdapter->GetDesc(&desc);
	g_displayDeviceName = desc.Description;
	g_devLUID = desc.AdapterLuid;

	//g_pD3D11Ctx->Release();
	//g_pD3D11Device->GetImmediateContext(&g_pD3D11Ctx);
	// turn on multithreading for the DX11 context
	CComQIPtr<ID3D10Multithread> p_mt(g_pD3D11Ctx);
	if (p_mt) {
		p_mt->SetMultithreadProtected(true);
	} else {
		return MFX_ERR_DEVICE_FAILED;
	}

	if (hWindow) {
		MSDK_CHECK_POINTER(g_pDXGIFactory, MFX_ERR_NULL_PTR);
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		FillSCD1(swapChainDesc);

		hres = g_pDXGIFactory->CreateSwapChainForHwnd(
			g_pD3D11Device, reinterpret_cast<HWND>(hWindow),
			&swapChainDesc, NULL, NULL,
			reinterpret_cast<IDXGISwapChain1 **>(&g_pSwapChain));
		if (FAILED(hres))
			return MFX_ERR_DEVICE_FAILED;
	}

	*deviceHandle = static_cast<mfxHDL>(g_pD3D11Device);

	return MFX_ERR_NONE;
}

mfxStatus Reset(mfxHDL *deviceHandle)
{

	MSDK_CHECK_POINTER(g_pDXGIFactory, MFX_ERR_NULL_PTR);
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

	FillSCD1(swapChainDesc);

	HRESULT hres = S_OK;
	hres = g_pDXGIFactory->CreateSwapChainForHwnd(
		g_pD3D11Device, reinterpret_cast<HWND>(deviceHandle),
		&swapChainDesc, NULL, NULL,
		reinterpret_cast<IDXGISwapChain1 **>(&g_pSwapChain));

	if (FAILED(hres)) {
		return MFX_ERR_DEVICE_FAILED;
	}
	return MFX_ERR_NONE;
}

mfxStatus CreateVideoProcessor(mfxFrameSurface1 *pSurface)
{
	CComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnum;
	if (m_VideoProcessorEnum.p || pSurface == nullptr) {
		return MFX_ERR_NONE;
	}

	D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
	memset(&(ContentDesc), 0, sizeof(D3D11_VIDEO_PROCESSOR_CONTENT_DESC));
	ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	ContentDesc.InputFrameRate.Numerator = 30000;
	ContentDesc.InputFrameRate.Denominator = 1000;
	ContentDesc.InputWidth = pSurface->Info.CropW;
	ContentDesc.InputHeight = pSurface->Info.CropH;
	ContentDesc.OutputFrameRate.Numerator = 30000;
	ContentDesc.OutputFrameRate.Denominator = 1000;
	ContentDesc.OutputWidth = pSurface->Info.CropW;
	ContentDesc.OutputHeight = pSurface->Info.CropH;
	ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	HRESULT hr = 0;
	CComQIPtr<ID3D11VideoDevice> m_pDX11VideoDevice;
	CComPtr<ID3D11VideoProcessor> m_pVideoProcessor;
	if (FAILED(hr = m_pDX11VideoDevice->CreateVideoProcessorEnumerator(
			   &ContentDesc, &m_VideoProcessorEnum))) {

		return MFX_ERR_DEVICE_FAILED;
	} else if (FAILED(hr = m_pDX11VideoDevice->CreateVideoProcessor(
				  m_VideoProcessorEnum, 0,
				  &m_pVideoProcessor))) {
		return MFX_ERR_DEVICE_FAILED;
	}

	return MFX_ERR_NONE;
}

void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx)
{
	g_pD3D11Ctx = devCtx;
	devCtx->GetDevice(&g_pD3D11Device);
}

// Free HW device context
void CleanupHWDevice()
{
	if (g_pAdapter) {
		g_pAdapter->Release();
		g_pAdapter = NULL;
	}
	if (g_pD3D11Device) {
		g_pD3D11Device->Release();
		g_pD3D11Device = NULL;
	}
	if (g_pD3D11Ctx) {
		g_pD3D11Ctx->Release();
		g_pD3D11Ctx = NULL;
	}
	if (g_pDXGIFactory) {
		g_pDXGIFactory->Release();
		g_pDXGIFactory = NULL;
	}
}

CComPtr<ID3D11DeviceContext> GetHWDeviceContext()
{
	return g_pD3D11Ctx;
}

LUID GetHWDeviceLUID()
{
	return g_devLUID;
}

std::wstring GetHWDeviceName()
{
	return g_displayDeviceName;
}

/* (Hugh) Functions currently unused */
#if 0
void ClearYUVSurfaceD3D(mfxMemId memId)
{
    // TBD
}

void ClearRGBSurfaceD3D(mfxMemId memId)
{
    // TBD
}
#endif

//
// Intel Media SDK memory allocator entrypoints....
//
mfxStatus _simple_alloc(mfxFrameAllocRequest *request,
			mfxFrameAllocResponse *response)
{
	HRESULT hRes;

	// Determine surface format
	DXGI_FORMAT format;
	switch (request->Info.FourCC) {
	case MFX_FOURCC_NV12:
		format = DXGI_FORMAT_NV12;
		break;
	case MFX_FOURCC_RGB4:
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case MFX_FOURCC_BGR4:
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case MFX_FOURCC_YUY2:
		format = DXGI_FORMAT_YUY2;
		break;
	case MFX_FOURCC_P8:
	case MFX_FOURCC_P8_TEXTURE:
		format = DXGI_FORMAT_P8;
		break;
	case MFX_FOURCC_P010:
		format = DXGI_FORMAT_P010;
		break;
	case MFX_FOURCC_A2RGB10:
		format = DXGI_FORMAT_R10G10B10A2_UNORM;
		break;
	case MFX_FOURCC_AYUV:
		format = DXGI_FORMAT_AYUV;
		break;
	case DXGI_FORMAT_AYUV:
		format = DXGI_FORMAT_AYUV;
		break;
	case MFX_FOURCC_Y210:
		format = DXGI_FORMAT_Y210;
		break;
	case MFX_FOURCC_Y410:
		format = DXGI_FORMAT_Y410;
		break;
	case MFX_FOURCC_P016:
		format = DXGI_FORMAT_P016;
		break;
	case MFX_FOURCC_Y216:
		format = DXGI_FORMAT_Y216;
		break;
	case MFX_FOURCC_Y416:
		format = DXGI_FORMAT_Y416;
		break;
	default:
	case DXGI_FORMAT_UNKNOWN:
		/*format = DXGI_FORMAT_UNKNOWN;*/
		return MFX_ERR_UNSUPPORTED;
		break;
	}

	// Allocate custom container to keep texture and stage buffers for each surface
	// Container also stores the intended read and/or write operation.
	CustomMemId **mids = static_cast<CustomMemId **>(
		calloc(static_cast<size_t>(request->NumFrameSuggested),
		       sizeof(CustomMemId *)));
	if (!mids)
		return MFX_ERR_MEMORY_ALLOC;

	for (int i = 0; i < request->NumFrameSuggested; i++) {
		mids[i] = static_cast<CustomMemId *>(
			calloc(1, sizeof(CustomMemId)));
		if (!mids[i]) {
			return MFX_ERR_MEMORY_ALLOC;
		}
		mids[i]->rw = static_cast<mfxU16>(request->Type &
			      0xF000); // Set intended read/write operation
	}

	request->Type = request->Type & 0x0FFF;

	// because P8 data (bitstream) for h264 encoder should be allocated by CreateBuffer()
	// but P8 data (MBData) for MPEG2 encoder should be allocated by CreateTexture2D()
	if (request->Info.FourCC == MFX_FOURCC_P8) {
		D3D11_BUFFER_DESC desc = {0};

		if (!request->NumFrameSuggested)
			return MFX_ERR_MEMORY_ALLOC;

		desc.ByteWidth = request->Info.Width * request->Info.Height;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		desc.StructureByteStride = 0;

		ID3D11Buffer *buffer = 0;
		hRes = g_pD3D11Device->CreateBuffer(&desc, 0, &buffer);
		if (FAILED(hRes))
			return MFX_ERR_MEMORY_ALLOC;

		mids[0]->memId = reinterpret_cast<ID3D11Texture2D *>(buffer);
	} else {
		D3D11_TEXTURE2D_DESC desc = {0};

		desc.Width = request->Info.Width;
		desc.Height = request->Info.Height;
		desc.MipLevels = 1;
		desc.ArraySize = 1; // number of subresources is 1 in this case
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DECODER;
		/*desc.MiscFlags = 0;*/
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		if ((MFX_MEMTYPE_FROM_VPPIN & request->Type) &&
		    ((DXGI_FORMAT_YUY2 == desc.Format) ||
		     (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) ||
		     (DXGI_FORMAT_R10G10B10A2_UNORM == desc.Format) ||
		     (DXGI_FORMAT_R16G16B16A16_UNORM == desc.Format))) {
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (desc.ArraySize > 2)
				return MFX_ERR_MEMORY_ALLOC;
		}

		if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
		    (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET &
		     request->Type)) {
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (desc.ArraySize > 2)
				return MFX_ERR_MEMORY_ALLOC;
		}

		if (DXGI_FORMAT_P8 == desc.Format)
			desc.BindFlags = 0;

		ID3D11Texture2D *pTexture2D = nullptr;

		// Create surface textures
		for (size_t i = 0;
		     i < request->NumFrameSuggested / desc.ArraySize; i++) {
			hRes = g_pD3D11Device->CreateTexture2D(&desc, nullptr,
							       &pTexture2D);

			if (FAILED(hRes))
				return MFX_ERR_MEMORY_ALLOC;

			mids[i]->memId = static_cast<mfxMemId>(pTexture2D);
		}

		desc.ArraySize = 1;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags =
			D3D11_CPU_ACCESS_READ; // | D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		// Create surface staging textures
		for (size_t i = 0; i < request->NumFrameSuggested; i++) {
			hRes = g_pD3D11Device->CreateTexture2D(&desc, nullptr,
							       &pTexture2D);

			if (FAILED(hRes))
				return MFX_ERR_MEMORY_ALLOC;

			mids[i]->memIdStage = static_cast<mfxMemId>(pTexture2D);
		}
	}

	response->mids = reinterpret_cast<mfxMemId *>(mids);
	response->NumFrameActual = request->NumFrameSuggested;

	return MFX_ERR_NONE;
}

mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request,
		       mfxFrameAllocResponse *response)
{
	mfxStatus sts = MFX_ERR_NONE;

	if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
		return MFX_ERR_UNSUPPORTED;

	if (allocDecodeResponses.find(pthis) != allocDecodeResponses.end() &&
	    MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
	    MFX_MEMTYPE_FROM_DECODE & request->Type) {
		// Memory for this request was already allocated during manual allocation stage. Return saved response
		//   When decode acceleration device (DXVA) is created it requires a list of d3d surfaces to be passed.
		//   Therefore Media SDK will ask for the surface info/mids again at Init() stage, thus requiring us to return the saved response
		//   (No such restriction applies to Encode or VPP)
		*response = allocDecodeResponses[pthis];
		allocDecodeRefCount[pthis]++;
	} else {
		sts = _simple_alloc(request, response);

		if (MFX_ERR_NONE == sts) {
			if (MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
			    MFX_MEMTYPE_FROM_DECODE & request->Type) {
				// Decode alloc response handling
				allocDecodeResponses[pthis] = *response;
				allocDecodeRefCount[pthis]++;
			} else {
				// Encode and VPP alloc response handling
				allocResponses[response->mids] = pthis;
			}
		}
	}

	return sts;
}

mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
	pthis; // To suppress warning for this unused parameter

	HRESULT hRes = S_OK;

	D3D11_TEXTURE2D_DESC desc = {0};
	D3D11_MAPPED_SUBRESOURCE lockedRect = {0};

	CustomMemId *memId = static_cast<CustomMemId *>(mid);
	ID3D11Texture2D *pSurface =
		static_cast<ID3D11Texture2D *>(memId->memId);
	ID3D11Texture2D *pStage =
		static_cast<ID3D11Texture2D *>(memId->memIdStage);

	D3D11_MAP mapType = D3D11_MAP_READ;
	UINT mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

	if (nullptr == pStage) {
		hRes = g_pD3D11Ctx->Map(pSurface, 0, mapType, mapFlags,
					&lockedRect);
		desc.Format = DXGI_FORMAT_P8;
	} else {
		pSurface->GetDesc(&desc);

		// copy data only in case of user wants to read from stored surface
		if (memId->rw & WILL_READ) {
			g_pD3D11Ctx->CopySubresourceRegion(
				pStage, 0, 0, 0, 0, pSurface, 0, nullptr);
		}

		do {
			hRes = g_pD3D11Ctx->Map(pStage, 0, mapType, mapFlags,
						&lockedRect);
			if (S_OK != hRes &&
			    DXGI_ERROR_WAS_STILL_DRAWING != hRes)
				return MFX_ERR_LOCK_MEMORY;
		} while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
	}

	if (FAILED(hRes))
		return MFX_ERR_LOCK_MEMORY;

	switch (desc.Format) {
	case DXGI_FORMAT_P016:
	case DXGI_FORMAT_NV12:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->Y = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->U = static_cast<mfxU8 *>(
			static_cast<mfxU8 *>(lockedRect.pData) +
			desc.Height * lockedRect.RowPitch);
		ptr->V = (desc.Format == DXGI_FORMAT_P010) ? ptr->U + 2
							   : ptr->U + 1;
		break;
	case DXGI_FORMAT_420_OPAQUE: // can be unsupported by standard ms guid
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->Y = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->V = static_cast<mfxU8 *>(
			static_cast<mfxU8 *>(lockedRect.pData) +
			desc.Height * lockedRect.RowPitch);
		ptr->U = static_cast<mfxU8 *>(
			ptr->V + (desc.Height * lockedRect.RowPitch) / 4);
		break;
	case DXGI_FORMAT_YUY2:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->Y = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->U = ptr->Y + 1;
		ptr->V = ptr->Y + 3;
		break;
	case DXGI_FORMAT_P8:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->Y = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->U = 0;
		ptr->V = 0;
		break;
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->B = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->A = ptr->B + 3;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->B = static_cast<mfxU8 *>(lockedRect.pData);
		ptr->G = ptr->B + 1;
		ptr->R = ptr->B + 2;
		ptr->A = ptr->B + 3;
		break;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		ptr->V16 = static_cast<mfxU16 *>(lockedRect.pData);
		ptr->U16 = ptr->V16 + 1;
		ptr->Y16 = ptr->V16 + 2;
		ptr->A = reinterpret_cast<mfxU8 *>(ptr->V16 + 3);
		ptr->PitchHigh =
			static_cast<mfxU16>(lockedRect.RowPitch / (1 << 16));
		ptr->PitchLow =
			static_cast<mfxU16>(lockedRect.RowPitch % (1 << 16));
		break;
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
		ptr->Pitch = static_cast<mfxU16>(lockedRect.RowPitch);
		ptr->Y16 = static_cast<mfxU16 *>(lockedRect.pData);
		ptr->U16 = 0;
		ptr->V16 = 0;
		break;
#if (MFX_VERSION >= 1031)
	case DXGI_FORMAT_Y416:
		ptr->PitchHigh =
			static_cast<mfxU16>(lockedRect.RowPitch / (1 << 16));
		ptr->PitchLow =
			static_cast<mfxU16>(lockedRect.RowPitch % (1 << 16));
		ptr->U16 = static_cast<mfxU16 *>(lockedRect.pData);
		ptr->Y16 = static_cast<mfxU16 *>(ptr->U16 + 1);
		ptr->V16 = static_cast<mfxU16 *>(ptr->Y16 + 1);
		ptr->A = reinterpret_cast<mfxU8 *>(ptr->V16 + 1);
		break;
	case DXGI_FORMAT_Y216:
#endif
#if (MFX_VERSION >= 1027)
	case DXGI_FORMAT_Y210:
		ptr->PitchHigh =
			static_cast<mfxU16>(lockedRect.RowPitch / (1 << 16));
		ptr->PitchLow =
			static_cast<mfxU16>(lockedRect.RowPitch % (1 << 16));
		ptr->Y16 = static_cast<mfxU16 *>(lockedRect.pData);
		ptr->U16 = static_cast<mfxU16 *>(ptr->Y16 + 1);
		ptr->V16 = static_cast<mfxU16 *>(ptr->Y16 + 3);

		break;

	case DXGI_FORMAT_Y410:
		ptr->PitchHigh =
			static_cast<mfxU16>(lockedRect.RowPitch / (1 << 16));
		ptr->PitchLow =
			static_cast<mfxU16>(lockedRect.RowPitch % (1 << 16));
		ptr->Y410 = static_cast<mfxY410 *>(lockedRect.pData);
		ptr->Y = 0;
		ptr->V = 0;
		ptr->A = 0;

		break;
#endif
	default:
		return MFX_ERR_LOCK_MEMORY;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
	pthis; // To suppress warning for this unused parameter

	CustomMemId *memId = static_cast<CustomMemId *>(mid);
	ID3D11Texture2D *pSurface =
		static_cast<ID3D11Texture2D *>(memId->memId);
	ID3D11Texture2D *pStage =
		static_cast<ID3D11Texture2D *>(memId->memIdStage);

	if (nullptr == pStage) {
		g_pD3D11Ctx->Unmap(pSurface, 0);
	} else {
		g_pD3D11Ctx->Unmap(pStage, 0);
		// copy data only in case of user wants to write to stored surface
		if (memId->rw & WILL_WRITE)
			g_pD3D11Ctx->CopySubresourceRegion(pSurface, 0, 0, 0, 0,
							   pStage, 0, nullptr);
	}

	if (ptr) {
		ptr->Pitch = 0;
		ptr->UV = ptr->U = ptr->V = ptr->Y = 0;
		ptr->A = ptr->R = ptr->G = ptr->B = 0;
		ptr->Locked = 0;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_copytex(mfxHDL pthis, mfxMemId mid, mfxU32 tex_handle,
			 mfxU64 lock_key, mfxU64 *next_key)
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

mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
	pthis; // To suppress warning for this unused parameter

	if (nullptr == handle)
		return MFX_ERR_INVALID_HANDLE;

	mfxHDLPair *pPair = reinterpret_cast<mfxHDLPair *>(handle);
	CustomMemId *memId = static_cast<CustomMemId *>(mid);

	pPair->first = memId->memId; // surface texture
	pPair->second = 0;

	return MFX_ERR_NONE;
}

mfxStatus _simple_free(mfxFrameAllocResponse *response)
{
	CustomMemId *mid = 0;
	ID3D11Texture2D *pSurface = 0;
	ID3D11Texture2D *pStage = 0;
	if (response->mids) {
		for (mfxU32 i = 0; i < response->NumFrameActual; i++) {
			if (response->mids[i]) {
				mid = static_cast<CustomMemId *>(
					response->mids[i]);
				pSurface = static_cast<ID3D11Texture2D *>(
					mid->memId);
				pStage = static_cast<ID3D11Texture2D *>(
					mid->memIdStage);

				if (pSurface)
					pSurface->Release();
				if (pStage)
					pStage->Release();

				free(mid);
			}
		}
		free(response->mids);
		response->mids = nullptr;
	}

	return MFX_ERR_NONE;
}

mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response)
{
	if (nullptr == response)
		return MFX_ERR_NULL_PTR;

	if (allocResponses.find(response->mids) == allocResponses.end()) {
		// Decode free response handling
		if (--allocDecodeRefCount[pthis] == 0) {
			_simple_free(response);
			allocDecodeResponses.erase(pthis);
			allocDecodeRefCount.erase(pthis);
		}
	} else {
		// Encode and VPP free response handling
		allocResponses.erase(response->mids);
		_simple_free(response);
	}

	return MFX_ERR_NONE;
}
