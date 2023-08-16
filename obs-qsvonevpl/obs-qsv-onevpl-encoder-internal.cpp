#include "obs-qsv-onevpl-encoder-internal.hpp"

#define do_log(level, format, ...) \
	blog(level, "[qsv encoder: '%s'] " format, "vpl_impl", ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

mfxU16 mfx_OpenEncodersNum = 0;
mfxHDL mfx_DX_Handle = nullptr;

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxVersion& version,
	bool isDGPU)
	: mfx_Impl(MFX_IMPL_HARDWARE_ANY),
	mfx_Platform(),
	mfx_Version(),
	mfx_Loader(),
	mfx_LoaderConfig(),
	mfx_LoaderVariant(),
	mfx_Session(),
	mfx_SurfacePool(),
	mfx_VideoENC(),
	VPS_Buffer(),
	SPS_Buffer(),
	PPS_Buffer(),
	VPS_BufferSize(1024),
	SPS_BufferSize(1024),
	PPS_BufferSize(1024),
	n_TaskNum(),
	t_TaskPool(),
	n_FirstSyncTask(0),
	mfx_Bitstream(),
	b_isDGPU(isDGPU),
	m_mfxEncParams()
{
	mfxIMPL tempImpl = MFX_IMPL_VIA_D3D11;
	isD3D11 = true;
#if defined(__linux__)
	tempImpl = MFX_IMPL_VIA_VAAPI;
	isD3D11 = false;
#endif
	mfx_Loader = MFXLoad();
	mfxStatus sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
	blog(LOG_INFO, "Session init status: %d", sts);
	if (sts == MFX_ERR_NONE) {
		sts = MFXQueryVersion(mfx_Session, &version);
		blog(LOG_INFO, "Version query status: %d", sts);
		if (sts == MFX_ERR_NONE) {
			mfx_Version = version;
			sts = MFXQueryIMPL(mfx_Session, &tempImpl);
			blog(LOG_INFO, "Impl query status: %d", sts);
			if (sts == MFX_ERR_NONE) {
				mfx_Impl = tempImpl;
				blog(LOG_INFO,
					 "\tImplementation:           %s\n"
					 "\tsurf:           %s\n",
					 isD3D11 ? "D3D11" : "VA-API",
					 isD3D11 ? "D3D11" : "VA-API");
			}
		} else {
			blog(LOG_INFO, "\tImplementation: [ERROR]\n");
		}
	} else {
		blog(LOG_INFO, "\tImplementation: [ERROR]\n");
	}
	MFXClose(mfx_Session);
	MFXUnload(mfx_Loader);
}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal()
{
	if (mfx_VideoENC) {
		ClearData();
	}
}
mfxStatus
QSV_VPL_Encoder_Internal::Initialize(int deviceNum)
{
	mfxStatus sts = MFX_ERR_NONE;

	// Initialize VPL Session
	mfx_Loader = MFXLoad();
	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
	MFXSetConfigFilterProperty(
			mfx_LoaderConfig,
			reinterpret_cast<const mfxU8 *>(
					"mfxImplDescription.Impl.mfxImplType"),
			mfx_LoaderVariant);

	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = static_cast<mfxU32>(0x8086);
	MFXSetConfigFilterProperty(
			mfx_LoaderConfig,
			reinterpret_cast<const mfxU8 *>("mfxImplDescription.VendorID"),
			mfx_LoaderVariant);

#if defined(_WIN32) || defined(_WIN64)
	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_PTR;
	mfx_LoaderVariant.Data.Ptr = mfxHDL("mfx-gen");
	MFXSetConfigFilterProperty(
			mfx_LoaderConfig,
			reinterpret_cast<const mfxU8 *>("mfxImplDescription.ImplName"),
			mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>(
			"mfxImplDescription.mfxAccelerationModeDescription.Mode"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_HANDLE_D3D11_DEVICE;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>("mfxHandleType"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
	mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_OFF;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>("mfxInitParam.GPUCopy"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
	mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_OFF;
	MFXSetConfigFilterProperty(mfx_LoaderConfig,
				   reinterpret_cast<const mfxU8 *>(
					   "mfxInitializationParam.DeviceCopy"),
				   mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
	mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_OFF;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig, reinterpret_cast<const mfxU8 *>("DeviceCopy"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>(
			"mfxInitializationParam.AccelerationMode"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
	mfx_LoaderVariant.Data.U16 = (mfxU16)64;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>(
			"mfxInitializationParam.mfxExtThreadsParam.NumThread"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_PRIORITY_HIGH;
	MFXSetConfigFilterProperty(
		mfx_LoaderConfig,
		reinterpret_cast<const mfxU8 *>(
			"mfxInitializationParam.mfxExtThreadsParam.Priority"),
		mfx_LoaderVariant);

	mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = 32;
	MFXSetConfigFilterProperty(mfx_LoaderConfig,
				   reinterpret_cast<const mfxU8 *>("NumThread"),
				   mfx_LoaderVariant);
#elif defined(__linux__)
	mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
	mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_VAAPI_DRM_RENDER_NODE;
	MFXSetConfigFilterProperty(
			mfx_LoaderConfig, reinterpret_cast<const mfxU8 *>("mfxImplDescription.AccelerationMode"),
			mfx_LoaderVariant);

	mfxHDL vaDisplay = nullptr;
	if (obs_get_nix_platform() == OBS_NIX_PLATFORM_X11_EGL) {
		vaDisplay =
				vaGetDisplay((Display *)obs_get_nix_platform_display());
	} else if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND) {
		vaDisplay = vaGetDisplayWl(
				(wl_display *)obs_get_nix_platform_display());
	}
#endif

	sts = MFXCreateSession(mfx_Loader, deviceNum,
			       &mfx_Session);

	MFXQueryIMPL(mfx_Session, &mfx_Impl);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#if defined(_WIN32) || defined(_WIN64)

	// Create DirectX device context
	if (mfx_DX_Handle == nullptr) {
		D3D11Device = new QSV_VPL_D3D11(mfx_Session);
		sts = D3D11Device->CreateHWDevice(&mfx_DX_Handle, deviceNum);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

	if (mfx_DX_Handle == nullptr)
		return MFX_ERR_DEVICE_FAILED;

	// Provide device manager to VPL

	sts = MFXVideoCORE_SetHandle(mfx_Session, MFX_HANDLE_D3D11_DEVICE,
		mfx_DX_Handle);

#elif defined(__linux__)

	int major;
	int minor;
	if (vaInitialize(vaDisplay, &major, &minor) != VA_STATUS_SUCCESS) {
		vaTerminate(vaDisplay);
		return MFX_ERR_DEVICE_FAILED;
	}
	sts = MFXVideoCORE_SetHandle(mfx_Session, MFX_HANDLE_VA_DISPLAY,
								 vaDisplay);

#endif
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	MFXVideoCORE_QueryPlatform(mfx_Session, &mfx_Platform);
	if (mfx_Platform.MediaAdapterType == MFX_MEDIA_DISCRETE) {
		blog(LOG_INFO, "\tAdapter type: Discrete");
		b_isDGPU = true;
	} else {
		blog(LOG_INFO, "\tAdapter type: Integrate");
		b_isDGPU = false;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Open(qsv_param_t *pParams,
					 enum qsv_codec codec)
{
	mfxStatus sts = Initialize(pParams->nDeviceNum);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_VideoENC = new MFXVideoENCODE(mfx_Session);

	InitENCParams(pParams, codec);
	
	sts = mfx_VideoENC->Query(&m_mfxEncParams, &m_mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_VideoENC->Init(&m_mfxEncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_VideoENC->GetVideoParam(&m_mfxEncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = GetVideoParam(codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitBitstream();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (sts >= MFX_ERR_NONE) {
		mfx_OpenEncodersNum++;
	} else {
		blog(LOG_INFO, "\tOpen encoder: [ERROR]");
		ClearData();
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitENCCtrlParams(qsv_param_t *pParams,
						      enum qsv_codec codec)
{
	auto CO2 = encCTRL.AddExtBuffer<mfxExtCodingOption2>();
	CO2->LookAheadDepth = 100;
	CO2->ExtBRC = MFX_CODINGOPTION_ON;
	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::InitENCParams(qsv_param_t* pParams,
	enum qsv_codec codec)
{
	/*It's only for debug*/
	int mfx_Ext_CO_enable = 1;
	int mfx_Ext_CO2_enable = 1;
	int mfx_Ext_CO3_enable = 1;
	int mfx_Ext_CO_DDI_enable = 1;

	switch (codec) {
	case QSV_CODEC_AV1:
		m_mfxEncParams.mfx.CodecId = MFX_CODEC_AV1;
		/*m_mfxEncParams.mfx.CodecLevel = 53;*/
		break;
	case QSV_CODEC_HEVC:
		m_mfxEncParams.mfx.CodecId = MFX_CODEC_HEVC;
		m_mfxEncParams.mfx.CodecLevel = 61;
		break;
	case QSV_CODEC_VP9:
		m_mfxEncParams.mfx.CodecId = MFX_CODEC_VP9;
		break;
	case QSV_CODEC_AVC:
	default:
		m_mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;
		/*m_mfxEncParams.mfx.CodecLevel = 52;*/
		break;
	}

	if ((pParams->nNumRefFrame > 0) && (pParams->nNumRefFrame < 16)) {
		m_mfxEncParams.mfx.NumRefFrame =
			static_cast<mfxU16>(pParams->nNumRefFrame);
		blog(LOG_INFO, "\tNumRefFrame set to: %d",
			pParams->nNumRefFrame);
	}

	m_mfxEncParams.mfx.TargetUsage =
		static_cast<mfxU16>(pParams->nTargetUsage);
	m_mfxEncParams.mfx.CodecProfile =
		static_cast<mfxU16>(pParams->CodecProfile);
	if (codec == QSV_CODEC_HEVC) {
		m_mfxEncParams.mfx.CodecProfile |= pParams->HEVCTier;
	}
	m_mfxEncParams.mfx.FrameInfo.FrameRateExtN =
		static_cast<mfxU32>(pParams->nFpsNum);
	m_mfxEncParams.mfx.FrameInfo.FrameRateExtD =
		static_cast<mfxU32>(pParams->nFpsDen);

	m_mfxEncParams.vpp.In.FrameRateExtN =
		static_cast<mfxU32>(pParams->nFpsNum);
	m_mfxEncParams.vpp.In.FrameRateExtD =
		static_cast<mfxU32>(pParams->nFpsDen);

	m_mfxEncParams.mfx.FrameInfo.FourCC =
		static_cast<mfxU32>(pParams->nFourCC);
	m_mfxEncParams.vpp.In.FourCC = static_cast<mfxU32>(pParams->nFourCC);

	m_mfxEncParams.mfx.FrameInfo.BitDepthChroma =
		pParams->video_fmt_10bit ? 10 : 8;
	m_mfxEncParams.vpp.In.BitDepthChroma = pParams->video_fmt_10bit ? 10
		: 8;

	m_mfxEncParams.mfx.FrameInfo.BitDepthLuma =
		pParams->video_fmt_10bit ? 10 : 8;
	m_mfxEncParams.vpp.In.BitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;

	if (pParams->video_fmt_10bit) {
		m_mfxEncParams.mfx.FrameInfo.Shift = 1;
		m_mfxEncParams.vpp.In.Shift = 1;
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture

	m_mfxEncParams.mfx.FrameInfo.Width =
		MSDK_ALIGN16(static_cast<mfxU16>(pParams->nWidth));
	m_mfxEncParams.vpp.In.Width =
		MSDK_ALIGN16(static_cast<mfxU16>(pParams->nWidth));

	m_mfxEncParams.mfx.FrameInfo.Height =
		MSDK_ALIGN16(static_cast<mfxU16>(pParams->nHeight));
	m_mfxEncParams.vpp.In.Height =
		MSDK_ALIGN16(static_cast<mfxU16>(pParams->nHeight));

	m_mfxEncParams.mfx.FrameInfo.ChromaFormat =
		static_cast<mfxU16>(pParams->nChromaFormat);
	m_mfxEncParams.vpp.In.ChromaFormat =
		static_cast<mfxU16>(pParams->nChromaFormat);

	m_mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	m_mfxEncParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

	m_mfxEncParams.mfx.FrameInfo.CropX = 0;
	m_mfxEncParams.vpp.In.CropX = 0;

	m_mfxEncParams.mfx.FrameInfo.CropY = 0;
	m_mfxEncParams.vpp.In.CropY = 0;

	m_mfxEncParams.mfx.FrameInfo.CropW =
		static_cast<mfxU16>(pParams->nWidth);
	m_mfxEncParams.vpp.In.CropW = static_cast<mfxU16>(pParams->nWidth);

	m_mfxEncParams.mfx.FrameInfo.CropH =
		static_cast<mfxU16>(pParams->nHeight);
	m_mfxEncParams.vpp.In.CropH = static_cast<mfxU16>(pParams->nHeight);

	switch ((int)pParams->bLowPower) {
	case 0:
		m_mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
		blog(LOG_INFO, "\tLowpower set: OFF");
		break;
	case 1:
		m_mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
		break;
	default:
		m_mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
		blog(LOG_INFO, "\tLowpower set: AUTO");
		break;
	}

	enum qsv_cpu_platform qsv_platform = qsv_get_cpu_platform();

	if (b_isDGPU == true || (pParams->nGOPRefDist == 1) ||
		qsv_platform > QSV_CPU_PLATFORM_RPL ||
		qsv_platform == QSV_CPU_PLATFORM_UNKNOWN ||
		codec == QSV_CODEC_AV1) {
		m_mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO,
			"\tThe Lowpower mode parameter was automatically changed to: ON");
	}

	m_mfxEncParams.mfx.RateControlMethod =
		static_cast<mfxU16>(pParams->RateControl);

	/*This is a multiplier to bypass the limitation of the 16 bit value of
		variables*/
	m_mfxEncParams.mfx.BRCParamMultiplier = 100;

	switch (pParams->RateControl) {
	case MFX_RATECONTROL_CBR:
		m_mfxEncParams.mfx.TargetKbps =
			static_cast<mfxU16>(pParams->nTargetBitRate);

		m_mfxEncParams.mfx.BufferSizeInKB =
			pParams->bLookahead &&
			pParams->nLADepth >
			(m_mfxEncParams.mfx.FrameInfo
				.FrameRateExtN /
				m_mfxEncParams.mfx.FrameInfo
				.FrameRateExtD)
			? static_cast<mfxU16>(
				(m_mfxEncParams.mfx.TargetKbps /
					static_cast<float>(8)) /
				(static_cast<float>(
					m_mfxEncParams.mfx.FrameInfo
					.FrameRateExtN) /
					m_mfxEncParams.mfx.FrameInfo
					.FrameRateExtD) *
				(pParams->nLADepth +
					(static_cast<float>(
						m_mfxEncParams.mfx.FrameInfo
						.FrameRateExtN) /
						m_mfxEncParams.mfx.FrameInfo
						.FrameRateExtD)))
			: static_cast<mfxU16>(
				(pParams->nTargetBitRate / 8) * 2);
		if (pParams->bCustomBufferSize == true &&
			pParams->nBufferSize > 0) {
			m_mfxEncParams.mfx.BufferSizeInKB =
				static_cast<mfxU16>(pParams->nBufferSize);
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		m_mfxEncParams.mfx.InitialDelayInKB = static_cast<mfxU16>(
			m_mfxEncParams.mfx.BufferSizeInKB / 2);
		blog(LOG_INFO, "\tBufferSize set to: %d KB",
			m_mfxEncParams.mfx.BufferSizeInKB * 100);
		m_mfxEncParams.mfx.MaxKbps =
			m_mfxEncParams.mfx.TargetKbps + 100;
		break;
	case MFX_RATECONTROL_VBR:
		m_mfxEncParams.mfx.TargetKbps =
			static_cast<mfxU16>(pParams->nTargetBitRate);
		m_mfxEncParams.mfx.MaxKbps =
			static_cast<mfxU16>(pParams->nMaxBitRate);
		m_mfxEncParams.mfx.BufferSizeInKB =
			pParams->bLookahead &&
			pParams->nLADepth >
			(m_mfxEncParams.mfx.FrameInfo
				.FrameRateExtN /
				m_mfxEncParams.mfx.FrameInfo
				.FrameRateExtD)
			? static_cast<mfxU16>(
				(m_mfxEncParams.mfx.TargetKbps /
					static_cast<float>(8)) /
				(static_cast<float>(
					m_mfxEncParams.mfx.FrameInfo
					.FrameRateExtN) /
					m_mfxEncParams.mfx.FrameInfo
					.FrameRateExtD) *
				(pParams->nLADepth +
					(static_cast<float>(
						m_mfxEncParams.mfx.FrameInfo
						.FrameRateExtN) /
						m_mfxEncParams.mfx.FrameInfo
						.FrameRateExtD)))
			: static_cast<mfxU16>(
				(m_mfxEncParams.mfx.TargetKbps / 8) *
				2);
		if (pParams->bCustomBufferSize == true &&
			pParams->nBufferSize > 0) {
			m_mfxEncParams.mfx.BufferSizeInKB =
				static_cast<mfxU16>(pParams->nBufferSize);
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		m_mfxEncParams.mfx.InitialDelayInKB = static_cast<mfxU16>(
			m_mfxEncParams.mfx.BufferSizeInKB / 2);
		blog(LOG_INFO, "\tBufferSize set to: %d KB",
			m_mfxEncParams.mfx.BufferSizeInKB * 100);
		break;
	case MFX_RATECONTROL_CQP:
		m_mfxEncParams.mfx.QPI = static_cast<mfxU16>(pParams->nQPI);
		m_mfxEncParams.mfx.QPB = static_cast<mfxU16>(pParams->nQPB);
		m_mfxEncParams.mfx.QPP = static_cast<mfxU16>(pParams->nQPP);
		break;
	case MFX_RATECONTROL_ICQ:
		m_mfxEncParams.mfx.ICQQuality =
			static_cast<mfxU16>(pParams->nICQQuality);
		break;
	}

	if (!b_isDGPU && qsv_platform <= QSV_CPU_PLATFORM_RPL &&
		pParams->bLookahead && pParams->nLADepth > 0 &&
		m_mfxEncParams.mfx.LowPower != MFX_CODINGOPTION_ON &&
		pParams->bExtBRC < 1 && codec == QSV_CODEC_AVC) {
		switch (pParams->RateControl) {
		case MFX_RATECONTROL_CBR:
			m_mfxEncParams.mfx.RateControlMethod =
				MFX_RATECONTROL_LA_HRD;
			break;
		case MFX_RATECONTROL_VBR:
			m_mfxEncParams.mfx.RateControlMethod =
				MFX_RATECONTROL_LA;
			break;
		case MFX_RATECONTROL_ICQ:
			m_mfxEncParams.mfx.RateControlMethod =
				MFX_RATECONTROL_LA_ICQ;
			break;
		}
	}

	m_mfxEncParams.AsyncDepth = static_cast<mfxU16>(pParams->nAsyncDepth);

	m_mfxEncParams.mfx.GopPicSize =
		(pParams->nKeyIntSec > 0)
		? static_cast<mfxU16>(
			pParams->nKeyIntSec *
			m_mfxEncParams.mfx.FrameInfo.FrameRateExtN /
			static_cast<float>(
				m_mfxEncParams.mfx.FrameInfo
				.FrameRateExtD))
		: 240;

	blog(LOG_INFO, "\tGopPicSize set to: %d",
		m_mfxEncParams.mfx.GopPicSize);

	if (pParams->bAdaptiveI != 1 && pParams->bAdaptiveB != 1) {
		m_mfxEncParams.mfx.GopOptFlag = MFX_GOP_STRICT;
		blog(LOG_INFO, "\tGopOptFlag set: STRICT");
	}
	else {
		m_mfxEncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
		blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
	}

	switch (codec) {
	case QSV_CODEC_HEVC:
		m_mfxEncParams.mfx.IdrInterval = 1;
		m_mfxEncParams.mfx.NumSlice = 1;
		break;
	default:
		m_mfxEncParams.mfx.IdrInterval = 0;
		m_mfxEncParams.mfx.NumSlice = 1;
		break;
	}

	switch (codec) {
	case QSV_CODEC_AV1:
		m_mfxEncParams.mfx.GopRefDist = pParams->nGOPRefDist;
		if (m_mfxEncParams.mfx.GopRefDist > 33) {
			m_mfxEncParams.mfx.GopRefDist = 33;
		}
		else if (m_mfxEncParams.mfx.GopRefDist > 16 &&
			(pParams->bExtBRC > 0 ||
				pParams->bEncTools == true)) {
			m_mfxEncParams.mfx.GopRefDist = 16;
			blog(LOG_INFO,
				"\tExtBRC and CPUEncTools does not support GOPRefDist greater than 16.");
		}
		break;
	default:
		m_mfxEncParams.mfx.GopRefDist =
			static_cast<mfxU16>(pParams->nGOPRefDist);
		if (m_mfxEncParams.mfx.GopRefDist > 16) {
			m_mfxEncParams.mfx.GopRefDist = 16;
		}
		else if (((pParams->bExtBRC > 0 && pParams->bLookahead) ||
			pParams->bEncTools == true) &&
			m_mfxEncParams.mfx.GopRefDist > 8) {
			m_mfxEncParams.mfx.GopRefDist = 8;
			blog(LOG_INFO,
				"\tExtBRC and CPUEncTools with Lookahead does not support GOPRefDist greater than 8.");
		}
		break;
	}

	if ((pParams->bExtBRC > 0 || pParams->bEncTools == true) &&
		(m_mfxEncParams.mfx.GopRefDist != 1 &&
			m_mfxEncParams.mfx.GopRefDist != 2 &&
			m_mfxEncParams.mfx.GopRefDist != 4 &&
			m_mfxEncParams.mfx.GopRefDist != 8 &&
			m_mfxEncParams.mfx.GopRefDist != 16)) {
		m_mfxEncParams.mfx.GopRefDist = 4;
		blog(LOG_INFO,
			"\tExtBRC and CPUEncTools only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
	}
	blog(LOG_INFO, "\tGOPRefDist set to: %d frames (%d b-frames)",
		(int)m_mfxEncParams.mfx.GopRefDist,
		(int)(m_mfxEncParams.mfx.GopRefDist - 1));

	m_mfxEncParams.mfx.MaxDecFrameBuffering = static_cast<mfxU16>(
		(m_mfxEncParams.mfx.FrameInfo.FrameRateExtN /
			m_mfxEncParams.mfx.FrameInfo.FrameRateExtD) +
		pParams->nLADepth);

	if (mfx_Ext_CO_enable == 1 && codec != QSV_CODEC_VP9) {
		auto CO = m_mfxEncParams.AddExtBuffer<mfxExtCodingOption>();
		/*Don't touch it!*/
		CO->CAVLC = MFX_CODINGOPTION_OFF;
		//CO->SingleSeiNalUnit = MFX_CODINGOPTION_ON;
		CO->RefPicMarkRep = MFX_CODINGOPTION_OFF;
		//CO->PicTimingSEI = MFX_CODINGOPTION_OFF;
		//CO->AUDelimiter = MFX_CODINGOPTION_OFF;

		CO->MaxDecFrameBuffering = static_cast<mfxU16>(
			(m_mfxEncParams.mfx.FrameInfo.FrameRateExtN /
				m_mfxEncParams.mfx.FrameInfo.FrameRateExtD) +
			pParams->nLADepth);

		CO->ResetRefList = MFX_CODINGOPTION_ON;
		CO->FieldOutput = MFX_CODINGOPTION_ON;
		CO->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		CO->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		CO->MVPrecision = MFX_MVPRECISION_QUARTERPEL;
		CO->MECostType = 8;
		CO->MESearchType = 16;
		CO->MVSearchWindow.x = 64;
		CO->MVSearchWindow.y = 64;

		if (pParams->bIntraRefEncoding == 1) {
			CO->RecoveryPointSEI = MFX_CODINGOPTION_ON;
		}

		switch ((int)pParams->bRDO) {
		case 0:
			CO->RateDistortionOpt = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tRDO set: OFF");
			break;
		case 1:
			CO->RateDistortionOpt = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tRDO set: ON");
			break;
		default:
			blog(LOG_INFO, "\tRDO set: AUTO");
			break;
		}

		CO->VuiVclHrdParameters = MFX_CODINGOPTION_ON;
		CO->VuiNalHrdParameters = MFX_CODINGOPTION_ON;
		CO->NalHrdConformance = MFX_CODINGOPTION_ON;
	}

	if (mfx_Ext_CO2_enable == 1) {
		auto CO2 = m_mfxEncParams.AddExtBuffer<mfxExtCodingOption2>();

		CO2->RepeatPPS = MFX_CODINGOPTION_OFF;

		switch ((int)pParams->bExtBRC) {
		case 0:
			blog(LOG_INFO, "\tExtBRC set: OFF");
			break;
		case 1:
			CO2->ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: IMPLICIT");
			break;
		case 2:
			CO2->ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: EXPLICIT");
			break;
		}

		if (pParams->bIntraRefEncoding == 1) {

			switch ((int)pParams->nIntraRefType) {
			case 0:
				CO2->IntRefType = MFX_REFRESH_VERTICAL;
				blog(LOG_INFO, "\tIntraRefType set: VERTICAL");
				break;
			default:
			case 1:
				CO2->IntRefType = MFX_REFRESH_HORIZONTAL;
				blog(LOG_INFO,
					"\tIntraRefType set: HORIZONTAL");
				break;
			}

			CO2->IntRefCycleSize = static_cast<mfxU16>(
				pParams->nIntraRefCycleSize > 1
				? pParams->nIntraRefCycleSize
				: (m_mfxEncParams.mfx.GopRefDist > 1
					? m_mfxEncParams.mfx.GopRefDist
					: 2));
			blog(LOG_INFO, "\tIntraRefCycleSize set: %d",
				CO2->IntRefCycleSize);
			if (pParams->nIntraRefQPDelta > -52 &&
				pParams->nIntraRefQPDelta < 52) {
				CO2->IntRefQPDelta = static_cast<mfxU16>(
					pParams->nIntraRefQPDelta);
				blog(LOG_INFO, "\tIntraRefQPDelta set: %d",
					CO2->IntRefQPDelta);
			}
		}

		if (pParams->bLookahead && pParams->nLADepth > 0) {
			CO2->LookAheadDepth = pParams->nLADepth;

			blog(LOG_INFO, "\tLookahead set to: %d",
				pParams->nLADepth);
		}

		if (codec != QSV_CODEC_VP9) {
			if (pParams->bMBBRC == 1) {
				CO2->MBBRC = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tMBBRC set: ON");
			}
			else if (pParams->bMBBRC == 0) {
				CO2->MBBRC = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tMBBRC set: OFF");
			}
		}

		if (pParams->nGOPRefDist > 1) {
			CO2->BRefType = MFX_B_REF_PYRAMID;
			blog(LOG_INFO, "\tBPyramid set: ON");
		}
		else {
			CO2->BRefType = MFX_B_REF_UNKNOWN;
			blog(LOG_INFO, "\tBPyramid set: AUTO");
		}

		switch ((int)pParams->nTrellis) {
		case 0:
			CO2->Trellis = MFX_TRELLIS_OFF;
			blog(LOG_INFO, "\tTrellis set: OFF");
			break;
		case 1:
			CO2->Trellis = MFX_TRELLIS_I;
			blog(LOG_INFO, "\tTrellis set: I");
			break;
		case 2:
			CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: IP");
			break;
		case 3:
			CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P |
				MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IPB");
			break;
		case 4:
			CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IB");
			break;
		case 5:
			CO2->Trellis = MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: P");
			break;
		case 6:
			CO2->Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: PB");
			break;
		case 7:
			CO2->Trellis = MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: B");
			break;
		default:
			blog(LOG_INFO, "\tTrellis set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveI) {
		case 0:
			CO2->AdaptiveI = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveI set: OFF");
			break;
		case 1:
			CO2->AdaptiveI = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveI set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveI set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveB) {
		case 0:
			CO2->AdaptiveB = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveB set: OFF");
			break;
		case 1:
			CO2->AdaptiveB = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveB set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveB set: AUTO");
			break;
		}

		if ((pParams->bLookahead && pParams->nLADepth > 0) &&
			(pParams->RateControl == MFX_RATECONTROL_CBR ||
				pParams->RateControl == MFX_RATECONTROL_VBR ||
				pParams->RateControl == MFX_RATECONTROL_CQP)) {
			switch ((int)pParams->nLookAheadDS) {
			case 0:
				CO2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
				blog(LOG_INFO, "\tLookAheadDS set: SLOW");
				break;
			case 1:
				CO2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
				blog(LOG_INFO, "\tLookAheadDS set: MEDIUM");
				break;
			case 2:
				CO2->LookAheadDS = MFX_LOOKAHEAD_DS_4x;
				blog(LOG_INFO, "\tLookAheadDS set: FAST");
				break;
			default:
				blog(LOG_INFO, "\tLookAheadDS set: AUTO");
				break;
			}
		}

		switch ((int)pParams->bRawRef) {
		case 0:
			CO2->UseRawRef = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tUseRawRef set: OFF");
			break;
		case 1:
			CO2->UseRawRef = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tUseRawRef set: ON");
			break;
		default:
			blog(LOG_INFO, "\tUseRawRef set: AUTO");
			break;
		}
	}

	if (mfx_Ext_CO3_enable == 1) {
		auto CO3 = m_mfxEncParams.AddExtBuffer<mfxExtCodingOption3>();
		/*Don't touch it!*/
		CO3->TargetBitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;
		CO3->TargetBitDepthChroma = pParams->video_fmt_10bit ? 10 : 8;
		CO3->TargetChromaFormatPlus1 = static_cast<mfxU16>(
			m_mfxEncParams.mfx.FrameInfo.ChromaFormat + 1);

		CO3->MBDisableSkipMap = MFX_CODINGOPTION_ON;
		CO3->EnableQPOffset = MFX_CODINGOPTION_ON;
		CO3->BitstreamRestriction = MFX_CODINGOPTION_ON;

		if (pParams->bIntraRefEncoding == 1) {
			CO3->IntRefCycleDist = 0;
		}

		if (pParams->bExtBRC > 0 && pParams->bLookahead == true &&
			codec == QSV_CODEC_AVC) {
			CO3->ScenarioInfo = MFX_SCENARIO_UNKNOWN;
			blog(LOG_INFO, "\tScenario: UNKNOWN");
		}
		else if (pParams->bExtBRC == 0 &&
			pParams->bLookahead == true) {
			CO3->ScenarioInfo = MFX_SCENARIO_UNKNOWN;
			blog(LOG_INFO, "\tScenario: UNKNOWN");
		}
		else if (pParams->bLookahead == false) {
			CO3->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
			blog(LOG_INFO, "\tScenario: REMOTE GAMING");
		}
		//CO3->ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
		if (m_mfxEncParams.mfx.RateControlMethod ==
			MFX_RATECONTROL_CQP) {
			CO3->EnableMBQP = MFX_CODINGOPTION_ON;
		}

		/*This parameter sets active references for frames. This is fucking magic, it may work with LookAhead,
			or it may not work,
			it seems to depend on the IDE and
				compiler. If you get frame drops, delete it.*/
		if (codec == QSV_CODEC_AVC) {
			std::fill(CO3->NumRefActiveP, CO3->NumRefActiveP + 8,
				(mfxU16)AVCGetMaxNumRefActivePL0(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					pParams->bLookahead,
					m_mfxEncParams.mfx.FrameInfo));
			std::fill(CO3->NumRefActiveBL0,
				CO3->NumRefActiveBL0 + 8,
				(mfxU16)AVCGetMaxNumRefActiveBL0(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					pParams->bLookahead));
			std::fill(CO3->NumRefActiveBL1,
				CO3->NumRefActiveBL1 + 8,
				(mfxU16)AVCGetMaxNumRefActiveBL1(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					pParams->bLookahead,
					m_mfxEncParams.mfx.FrameInfo));

		}
		else if (codec == QSV_CODEC_HEVC) {
			if (pParams->nNumRefFrameLayers > 0) {

				std::fill(
					CO3->NumRefActiveP,
					CO3->NumRefActiveP + 8,
					(mfxU16)HEVCGetMaxNumRefActivePL0(
						m_mfxEncParams.mfx.TargetUsage,
						m_mfxEncParams.mfx.LowPower,
						m_mfxEncParams.mfx.FrameInfo));
				std::fill(
					CO3->NumRefActiveBL0,
					CO3->NumRefActiveBL0 + 8,
					(mfxU16)HEVCGetMaxNumRefActiveBL0(
						m_mfxEncParams.mfx.TargetUsage,
						m_mfxEncParams.mfx.LowPower));
				std::fill(
					CO3->NumRefActiveBL1,
					CO3->NumRefActiveBL1 + 8,
					(mfxU16)HEVCGetMaxNumRefActiveBL1(
						m_mfxEncParams.mfx.TargetUsage,
						m_mfxEncParams.mfx.LowPower,
						m_mfxEncParams.mfx.FrameInfo));

				blog(LOG_INFO, "\tNumRefFrameLayer set: %d",
					pParams->nNumRefFrame);
			}
		}
		else if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) {
			if (pParams->nNumRefFrameLayers > 0) {
				std::fill(CO3->NumRefActiveP,
					CO3->NumRefActiveP + 8,
					m_mfxEncParams.mfx.NumRefFrame);
				std::fill(CO3->NumRefActiveBL0,
					CO3->NumRefActiveBL0 + 8,
					m_mfxEncParams.mfx.NumRefFrame);
				std::fill(CO3->NumRefActiveBL1,
					CO3->NumRefActiveBL1 + 8,
					m_mfxEncParams.mfx.NumRefFrame);

				blog(LOG_INFO, "\tNumRefFrameLayer set: %d",
					pParams->nNumRefFrame);
			}
		}

		if (codec == QSV_CODEC_HEVC) {
			switch ((int)pParams->bGPB) {
			case 0:
				CO3->GPB = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tGPB set: OFF");
				break;
			case 1:
				CO3->GPB = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tGPB set: ON");
				break;
			default:
				blog(LOG_INFO, "\tGPB set: AUTO");
				break;
			}
		}

		if ((int)pParams->nGOPRefDist == 1) {
			CO3->PRefType = MFX_P_REF_PYRAMID;
			blog(LOG_INFO, "\tPRef set: PYRAMID");
		}
		else {
			CO3->PRefType = MFX_P_REF_SIMPLE;
		}

		switch ((int)pParams->bAdaptiveCQM) {
		case 0:
			CO3->AdaptiveCQM = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveCQM set: OFF");
			break;
		case 1:
			CO3->AdaptiveCQM = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveCQM set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveCQM set: AUTO");
			break;
		}

		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 4) {

			switch ((int)pParams->bAdaptiveRef) {
			case 0:
				CO3->AdaptiveRef = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveRef set: OFF");
				break;
			case 1:
				CO3->AdaptiveRef = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tAdaptiveRef set: ON");
				break;
			default:
				blog(LOG_INFO, "\tAdaptiveRef set: AUTO");
				break;
			}

			switch ((int)pParams->bAdaptiveLTR) {
			case 0:
				CO3->AdaptiveLTR = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveLTR set: OFF");
				break;
			case 1:
				CO3->AdaptiveLTR = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tAdaptiveLTR set: ON");
				break;
			default:
				blog(LOG_INFO, "\tAdaptiveLTR set: AUTO");
				break;
			}
		}

		if (pParams->nWinBRCMaxAvgSize > 0) {
			CO3->WinBRCMaxAvgKbps =
				static_cast<mfxU16>(pParams->nWinBRCMaxAvgSize);
			blog(LOG_INFO, "\tWinBRCMaxSize set: %d",
				CO3->WinBRCMaxAvgKbps);
		}

		if (pParams->nWinBRCSize > 0) {
			CO3->WinBRCSize =
				static_cast<mfxU16>(pParams->nWinBRCSize);
			blog(LOG_INFO, "\tWinBRCSize set: %d", CO3->WinBRCSize);
		}

		if ((int)pParams->nMotionVectorsOverPicBoundaries >= -1) {
			switch ((int)pParams->nMotionVectorsOverPicBoundaries) {
			case 0:
				CO3->MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_OFF;
				blog(LOG_INFO,
					"\tMotionVectorsOverPicBoundaries set: OFF");
				break;
			case 1:
				CO3->MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
					"\tMotionVectorsOverPicBoundaries set: ON");
				break;
			default:
				blog(LOG_INFO,
					"\tMotionVectorsOverPicBoundaries set: AUTO");
				break;
			}
		}

		switch ((int)pParams->bWeightedPred) {
		case 0:
			blog(LOG_INFO, "\tWeightedPred set: OFF");
			break;
		case 1:
			CO3->WeightedPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO, "\tWeightedPred set: ON");
			break;
		default:
			blog(LOG_INFO, "\tWeightedPred set: AUTO");
			break;
		}

		switch ((int)pParams->bWeightedBiPred) {
		case 0:
			blog(LOG_INFO, "\tWeightedBiPred set: OFF");
			break;
		case 1:
			CO3->WeightedBiPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO, "\tWeightedBiPred set: ON");
			break;
		default:
			blog(LOG_INFO, "\tWeightedBiPred set: AUTO");
			break;
		}

		if (pParams->bGlobalMotionBiasAdjustment == 1) {
			CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tGlobalMotionBiasAdjustment set: ON");

			switch (pParams->nMVCostScalingFactor) {
			case 1:
				CO3->MVCostScalingFactor = 1;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set: 1/2");
				break;
			case 2:
				CO3->MVCostScalingFactor = 2;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set: 1/4");
				break;
			case 3:
				CO3->MVCostScalingFactor = 3;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set: 1/8");
				break;
			default:
				CO3->MVCostScalingFactor = 56;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set: DEFAULT");
				break;
			}
		}
		else if (pParams->bGlobalMotionBiasAdjustment == 0) {
			CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
		}

		switch ((int)pParams->bDirectBiasAdjustment) {
		case 0:
			CO3->DirectBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: OFF");
			break;
		case 1:
			CO3->DirectBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: ON");
			break;
		default:
			blog(LOG_INFO, "\tDirectBiasAdjustment set: AUTO");
			break;
		}
	}

#if defined(_WIN32) || defined(_WIN64)
	if (codec != QSV_CODEC_VP9 &&
		((mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
			pParams->nTargetUsage == MFX_TARGETUSAGE_1 &&
			/*(pParams->bLookahead == false) &&*/
			(m_mfxEncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
				m_mfxEncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR)) &&
		pParams->bEncTools == true && pParams->bExtBRC != 1) {
		auto EncToolsParam =
			m_mfxEncParams.AddExtBuffer<mfxExtEncToolsConfig>();
		if (pParams->bExtBRC < 2) {
			EncToolsParam->BRC =
				MFX_CODINGOPTION_ON; //Включить обязательно
			EncToolsParam->BRCBufferHints =
				MFX_CODINGOPTION_ON; //Включить обязательно
			EncToolsParam->SceneChange =
				MFX_CODINGOPTION_ON; //Включить обязательно
		}
	}


	/*Don't touch it! Magic beyond the control of mere mortals takes place here*/
	if (mfx_Ext_CO_DDI_enable == 1 && codec != QSV_CODEC_AV1) {
		auto CODDI =
			m_mfxEncParams.AddExtBuffer<mfxExtCodingOptionDDI>();
		CODDI->WriteIVFHeaders = MFX_CODINGOPTION_OFF;
		CODDI->IBC = MFX_CODINGOPTION_ON;
		CODDI->BRCPrecision = 3;
		CODDI->BiDirSearch = MFX_CODINGOPTION_ON;
		CODDI->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
		CODDI->GlobalSearch = 1;
		CODDI->IntraPredCostType = 8;
		CODDI->MEFractionalSearchType = 16;
		CODDI->MVPrediction = MFX_CODINGOPTION_ON;
		CODDI->WeightedBiPredIdc = pParams->bWeightedBiPred == 1 ? 2
			: 0;
		CODDI->WeightedPrediction =
			(pParams->bWeightedPred == 1)
			? static_cast<mfxU16>(MFX_CODINGOPTION_ON)
			: static_cast<mfxU16>(MFX_CODINGOPTION_OFF);
		CODDI->FieldPrediction = MFX_CODINGOPTION_ON;
		//CO_DDI->CabacInitIdcPlus1 = (mfxU16)1;
		CODDI->DirectCheck = MFX_CODINGOPTION_ON;
		CODDI->FractionalQP = 1;
		CODDI->Hme = MFX_CODINGOPTION_ON;
		CODDI->LocalSearch = 6;
		CODDI->MBAFF = MFX_CODINGOPTION_ON;
		CODDI->DDI.InterPredBlockSize = 64;
		CODDI->DDI.IntraPredBlockSize = 1;
		CODDI->RefOppositeField = MFX_CODINGOPTION_ON;
		CODDI->RefRaw =
			(pParams->bRawRef == 1)
			? static_cast<mfxU16>(MFX_CODINGOPTION_ON)
			: static_cast<mfxU16>(MFX_CODINGOPTION_OFF);
		CODDI->TMVP = MFX_CODINGOPTION_ON;
		CODDI->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
		CODDI->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
		CODDI->QpAdjust = MFX_CODINGOPTION_ON;
		CODDI->Transform8x8Mode = MFX_CODINGOPTION_ON;

		if (codec == QSV_CODEC_AVC) {
			CODDI->NumActiveRefP = (mfxU16)AVCGetMaxNumRefActivePL0(
				m_mfxEncParams.mfx.TargetUsage,
				m_mfxEncParams.mfx.LowPower,
				pParams->bLookahead,
				m_mfxEncParams.mfx.FrameInfo);
			CODDI->NumActiveRefBL0 =
				(mfxU16)AVCGetMaxNumRefActiveBL0(
					m_mfxEncParams.mfx.TargetUsage,
					pParams->bLookahead,
					m_mfxEncParams.mfx.LowPower);
			CODDI->NumActiveRefBL1 =
				(mfxU16)AVCGetMaxNumRefActiveBL1(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					pParams->bLookahead,
					m_mfxEncParams.mfx.FrameInfo);

		}
		else if (codec == QSV_CODEC_HEVC) {
			CODDI->NumActiveRefP =
				static_cast<mfxU16>(HEVCGetMaxNumRefActivePL0(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					m_mfxEncParams.mfx.FrameInfo));
			CODDI->NumActiveRefBL0 =
				static_cast<mfxU16>(HEVCGetMaxNumRefActiveBL0(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower));
			CODDI->NumActiveRefBL1 =
				static_cast<mfxU16>(HEVCGetMaxNumRefActiveBL1(
					m_mfxEncParams.mfx.TargetUsage,
					m_mfxEncParams.mfx.LowPower,
					m_mfxEncParams.mfx.FrameInfo));
		}
		else if (codec == QSV_CODEC_VP9) {
			CODDI->NumActiveRefP =
				static_cast<mfxU16>(pParams->nNumRefFrame);
			CODDI->NumActiveRefBL0 =
				static_cast<mfxU16>(pParams->nNumRefFrame);
			CODDI->NumActiveRefBL1 =
				static_cast<mfxU16>(pParams->nNumRefFrame);
		}
		/*You can touch it, this is the LookAHead setting,
			here you can adjust its strength,
			range of quality and dependence.*/
		if (pParams->bLookahead && pParams->nLADepth >= 20) {
			CODDI->StrengthN = 1000;
			CODDI->QpUpdateRange = 30;
			CODDI->LaScaleFactor = 1;
			CODDI->LookAheadDependency =
				static_cast<mfxU16>(pParams->nLADepth - 10);
		}
	}
#endif

	/*This is waiting for a driver with support*/
	//if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9 &&
	//    pParams->nTuneQualityMode != 0) {
	//	auto TuneQuality =
	//		m_mfxEncParams.AddExtBuffer<mfxExtTuneEncodeQuality>();

	//	switch ((int)pParams->nTuneQualityMode) {
	//	case 0:
	//		blog(LOG_INFO, "\tTuneQualityMode set: OFF");
	//		break;
	//	case 1:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_PSNR;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PSNR");
	//		break;
	//	case 2:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: SSIM");
	//		break;
	//	case 3:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: MS SSIM");
	//		break;
	//	case 4:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_VMAF;
	//		blog(LOG_INFO, "\tTuneQualityMode set: VMAF");
	//		break;
	//	case 5:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PERCEPTUAL");
	//		break;
	//	default:
	//		TuneQuality->TuneQuality = MFX_ENCODE_TUNE_OFF;
	//		blog(LOG_INFO, "\tTuneQualityMode set: DEFAULT");
	//		break;
	//	}
	//}

	if (codec == QSV_CODEC_HEVC) {
		auto HevcParam = m_mfxEncParams.AddExtBuffer<mfxExtHEVCParam>();

		auto ChromaLocParam =
			m_mfxEncParams.AddExtBuffer<mfxExtChromaLocInfo>();

		auto HevcTilesParam =
			m_mfxEncParams.AddExtBuffer<mfxExtHEVCTiles>();

		ChromaLocParam->ChromaLocInfoPresentFlag = 1;
		ChromaLocParam->ChromaSampleLocTypeTopField =
			static_cast<mfxU16>(
				pParams->ChromaSampleLocTypeTopField);
		ChromaLocParam->ChromaSampleLocTypeBottomField =
			static_cast<mfxU16>(
				pParams->ChromaSampleLocTypeBottomField);

		HevcParam->PicWidthInLumaSamples =
			m_mfxEncParams.mfx.FrameInfo.Width;
		HevcParam->PicHeightInLumaSamples =
			m_mfxEncParams.mfx.FrameInfo.Height;
		/*	HevcParam->LCUSize = pParams->nCTU;*/
		switch ((int)pParams->nSAO) {
		case 0:
			HevcParam->SampleAdaptiveOffset = MFX_SAO_DISABLE;
			blog(LOG_INFO, "\tSAO set: DISABLE");
			break;
		case 1:
			HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_LUMA;
			blog(LOG_INFO, "\tSAO set: LUMA");
			break;
		case 2:
			HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_CHROMA;
			blog(LOG_INFO, "\tSAO set: CHROMA");
			break;
		case 3:
			HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_LUMA |
				MFX_SAO_ENABLE_CHROMA;
			blog(LOG_INFO, "\tSAO set: ALL");
			break;
		default:
			blog(LOG_INFO, "\tDirectBiasAdjustment set: AUTO");
			break;
		}
		HevcTilesParam->NumTileColumns = 1;
		HevcTilesParam->NumTileRows = 1;
	}

	if (codec == QSV_CODEC_AVC &&
		(m_mfxEncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
			m_mfxEncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
			m_mfxEncParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP) &&
		pParams->nDenoiseMode > -1) {
		auto DenoiseParam =
			m_mfxEncParams.AddExtBuffer<mfxExtVPPDenoise2>();

		switch (pParams->nDenoiseMode) {
		case 1:
			DenoiseParam->Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
			blog(LOG_INFO,
				"\tDenoise set: AUTO | BDRATE | PRE ENCODE");
			break;
		case 2:
			DenoiseParam->Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
			blog(LOG_INFO,
				"\tDenoise set: AUTO | ADJUST | POST ENCODE");
			break;
		case 3:
			DenoiseParam->Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
			blog(LOG_INFO,
				"\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
			break;
		case 4:
			DenoiseParam->Mode =
				MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
			DenoiseParam->Strength =
				static_cast<mfxU16>(pParams->nDenoiseStrength);
			blog(LOG_INFO,
				"\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
				DenoiseParam->Strength);
			break;
		case 5:
			DenoiseParam->Mode =
				MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
			DenoiseParam->Strength =
				static_cast<mfxU16>(pParams->nDenoiseStrength);
			blog(LOG_INFO,
				"\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
				DenoiseParam->Strength);
			break;
		default:
			DenoiseParam->Mode = MFX_DENOISE_MODE_DEFAULT;
			blog(LOG_INFO, "\tDenoise set: DEFAULT");
			break;
		}
	}

	if (codec == QSV_CODEC_AV1) {
		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 5) {
			auto AV1BitstreamParam =
				m_mfxEncParams
				.AddExtBuffer<mfxExtAV1BitstreamParam>();

			auto AV1TileParam =
				m_mfxEncParams
				.AddExtBuffer<mfxExtAV1TileParam>();

			

			AV1BitstreamParam->WriteIVFHeaders =
				MFX_CODINGOPTION_OFF;

			AV1TileParam->NumTileGroups = 1;
			if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
				AV1TileParam->NumTileColumns = 2;
				AV1TileParam->NumTileRows = 2;
			}
			else {
				AV1TileParam->NumTileColumns = 1;
				AV1TileParam->NumTileRows = 1;
			}

#if defined(_WIN32) || defined(_WIN64)
			auto AV1AuxParam =
				m_mfxEncParams.AddExtBuffer<mfxExtAV1AuxData>();
			AV1AuxParam->EnableCdef = MFX_CODINGOPTION_ON;
			AV1AuxParam->Cdef.CdefBits = 3;
			AV1AuxParam->DisableFrameEndUpdateCdf =
				MFX_CODINGOPTION_OFF;
			AV1AuxParam->DisableCdfUpdate = MFX_CODINGOPTION_OFF;

			/*AV1AuxParam->EnableSuperres = MFX_CODINGOPTION_ON;*/

			AV1AuxParam->InterpFilter = MFX_CODINGOPTION_ON;

			AV1AuxParam->UniformTileSpacing = MFX_CODINGOPTION_ON;
			AV1AuxParam->SwitchInterval = 0;

			AV1AuxParam->EnableLoopFilter = MFX_CODINGOPTION_ON;
			AV1AuxParam->LoopFilter.ModeRefDeltaEnabled = 1;
			AV1AuxParam->LoopFilter.ModeRefDeltaUpdate = 1;
			AV1AuxParam->LoopFilterSharpness = 2;

			AV1AuxParam->EnableOrderHint = MFX_CODINGOPTION_ON;
			AV1AuxParam->OrderHintBits = 8;

			AV1AuxParam->Palette = MFX_CODINGOPTION_ON;
			AV1AuxParam->SegmentTemporalUpdate =
				MFX_CODINGOPTION_ON;
			AV1AuxParam->IBC = MFX_CODINGOPTION_ON;
			AV1AuxParam->PackOBUFrame = MFX_CODINGOPTION_ON;
#endif

		}
	}

	if (codec == QSV_CODEC_VP9) {
		auto VP9Param = m_mfxEncParams.AddExtBuffer<mfxExtVP9Param>();

		VP9Param->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

		if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
			VP9Param->NumTileColumns = 2;
			VP9Param->NumTileRows = 2;
		}
		else {
			VP9Param->NumTileColumns = 1;
			VP9Param->NumTileRows = 1;
		}

	}
#if defined(_WIN32) || defined(_WIN64)
	else {
		auto VideoSignalParam =
			m_mfxEncParams.AddExtBuffer<mfxExtVideoSignalInfo>();

		VideoSignalParam->VideoFormat =
			static_cast<mfxU16>(pParams->VideoFormat);
		VideoSignalParam->VideoFullRange =
			static_cast<mfxU16>(pParams->VideoFullRange);
		VideoSignalParam->ColourDescriptionPresent = 1;
		VideoSignalParam->ColourPrimaries =
			static_cast<mfxU16>(pParams->ColourPrimaries);
		VideoSignalParam->TransferCharacteristics =
			static_cast<mfxU16>(pParams->TransferCharacteristics);
		VideoSignalParam->MatrixCoefficients =
			static_cast<mfxU16>(pParams->MatrixCoefficients);
	}
#endif

	if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC ||
		((codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) &&
			(mfx_Version.Major >= 2 && mfx_Version.Minor >= 9))) &&
		pParams->MaxContentLightLevel > 0) {
		auto ColourVolumeParam = m_mfxEncParams.AddExtBuffer<
			mfxExtMasteringDisplayColourVolume>();
		auto ContentLightLevelParam =
			m_mfxEncParams
			.AddExtBuffer<mfxExtContentLightLevelInfo>();

		ColourVolumeParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
		ColourVolumeParam->DisplayPrimariesX[0] =
			static_cast<mfxU16>(pParams->DisplayPrimariesX[0]);
		ColourVolumeParam->DisplayPrimariesX[1] =
			static_cast<mfxU16>(pParams->DisplayPrimariesX[1]);
		ColourVolumeParam->DisplayPrimariesX[2] =
			static_cast<mfxU16>(pParams->DisplayPrimariesX[2]);
		ColourVolumeParam->DisplayPrimariesY[0] =
			static_cast<mfxU16>(pParams->DisplayPrimariesY[0]);
		ColourVolumeParam->DisplayPrimariesY[1] =
			static_cast<mfxU16>(pParams->DisplayPrimariesY[1]);
		ColourVolumeParam->DisplayPrimariesY[2] =
			static_cast<mfxU16>(pParams->DisplayPrimariesY[2]);
		ColourVolumeParam->WhitePointX =
			static_cast<mfxU16>(pParams->WhitePointX);
		ColourVolumeParam->WhitePointY =
			static_cast<mfxU16>(pParams->WhitePointY);
		ColourVolumeParam->MaxDisplayMasteringLuminance =
			static_cast<mfxU16>(
				pParams->MaxDisplayMasteringLuminance);
		ColourVolumeParam->MinDisplayMasteringLuminance =
			static_cast<mfxU16>(
				pParams->MinDisplayMasteringLuminance);

		ContentLightLevelParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
		ContentLightLevelParam->MaxContentLightLevel =
			static_cast<mfxU16>(pParams->MaxContentLightLevel);
		ContentLightLevelParam->MaxPicAverageLightLevel =
			static_cast<mfxU16>(pParams->MaxPicAverageLightLevel);
	}

	m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
		m_mfxEncParams.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	}

	blog(LOG_INFO, "Feature extended buffer size: %d",
		m_mfxEncParams.NumExtParam);

	// We dont check what was valid or invalid here, just try changing lower power.
	// Ensure set values are not overwritten so in case it wasnt lower power we fail
	// during the parameter check.
	mfxVideoParam validParams = { 0 };
	memcpy(&validParams, &m_mfxEncParams, sizeof(mfxVideoParam));
	mfxStatus sts = mfx_VideoENC->Query(&m_mfxEncParams, &validParams);
	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
		if (m_mfxEncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
			m_mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
			auto CO2 =
				m_mfxEncParams
				.AddExtBuffer<mfxExtCodingOption2>();
			CO2->LookAheadDepth = 0;
		}
	}
	return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(qsv_param_t *pParams)
{
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_CBR:
		m_mfxEncParams.mfx.TargetKbps =
			static_cast<mfxU16>(pParams->nTargetBitRate);
		break;
	default:

		break;
	}

	return true;
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder()
{
	return mfx_VideoENC->Reset(&m_mfxEncParams);
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec)
{
	if (codec != QSV_CODEC_VP9) {
		auto SPSPPSParams =
			m_mfxEncParams.AddExtBuffer<mfxExtCodingOptionSPSPPS>();
		SPSPPSParams->SPSBuffer = SPS_Buffer;
		SPSPPSParams->PPSBuffer = PPS_Buffer;
		SPSPPSParams->SPSBufSize = 1024;
		SPSPPSParams->PPSBufSize = 1024;
	}
	if (codec == QSV_CODEC_HEVC) {
		auto VPSParams =
			m_mfxEncParams.AddExtBuffer<mfxExtCodingOptionVPS>();

		VPSParams->VPSBuffer = VPS_Buffer;
		VPSParams->VPSBufSize = 1024;
	}

	mfxStatus sts = mfx_VideoENC->GetVideoParam(&m_mfxEncParams);

	blog(LOG_INFO, "\tGetVideoParam status:     %d", sts);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (codec == QSV_CODEC_HEVC) {
		auto VPSParams =
			m_mfxEncParams.GetExtBuffer<mfxExtCodingOptionVPS>();
		VPS_BufferSize = VPSParams->VPSBufSize;
	}

	if (codec != QSV_CODEC_VP9) {
		auto SPSPPSParams =
			m_mfxEncParams.GetExtBuffer<mfxExtCodingOptionSPSPPS>();
		SPS_BufferSize = SPSPPSParams->SPSBufSize;
		PPS_BufferSize = SPSPPSParams->PPSBufSize;
	}

	return sts;
}

void QSV_VPL_Encoder_Internal::GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
					    mfxU8 **pPPSBuf, mfxU16 *pnVPSBuf,
					    mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf)
{
	*pVPSBuf = VPS_Buffer;
	*pnVPSBuf = VPS_BufferSize;

	*pSPSBuf = SPS_Buffer;
	*pnSPSBuf = SPS_BufferSize;

	*pPPSBuf = PPS_Buffer;
	*pnPPSBuf = PPS_BufferSize;
}

mfxStatus QSV_VPL_Encoder_Internal::InitBitstream()
{
	n_TaskNum = m_mfxEncParams.AsyncDepth;
	n_FirstSyncTask = 0;

	t_TaskPool = new struct Task[n_TaskNum];
	memset(t_TaskPool, 0, sizeof(Task) * n_TaskNum);

	mfxU32 MaxLength = static_cast<mfxU32>(
		(m_mfxEncParams.mfx.BufferSizeInKB * 1000 * 100 * 3));

	for (int i = 0; i < n_TaskNum; i++) {
		t_TaskPool[i].mfxBS = new mfxBitstream;
		memset(t_TaskPool[i].mfxBS, 0, sizeof(mfxBitstream));
		t_TaskPool[i].mfxBS->MaxLength = MaxLength;
		t_TaskPool[i].mfxBS->Data =
			new mfxU8[t_TaskPool[i].mfxBS->MaxLength];
		t_TaskPool[i].mfxBS->DataOffset = 0;
		t_TaskPool[i].mfxBS->DataLength = 0;
		t_TaskPool[i].mfxBS->CodecId = m_mfxEncParams.mfx.CodecId;
		t_TaskPool[i].mfxBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME |
						MFX_BITSTREAM_EOS;
		t_TaskPool[i].syncp = nullptr;
		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS->Data,
				   MFX_ERR_MEMORY_ALLOC);
	}
	mfx_Bitstream = new mfxBitstream;
	memset(mfx_Bitstream, 0, sizeof(mfxBitstream));
	mfx_Bitstream->MaxLength = MaxLength;
	mfx_Bitstream->Data = new mfxU8[mfx_Bitstream->MaxLength];
	mfx_Bitstream->DataOffset = 0;
	mfx_Bitstream->DataLength = 0;
	mfx_Bitstream->CodecId = m_mfxEncParams.mfx.CodecId;
	mfx_Bitstream->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME |
				  MFX_BITSTREAM_EOS;
	blog(LOG_INFO, "\tTaskPool count:    %d", n_TaskNum);

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadP010(mfxFrameSurface1 *pSurface,
					     uint8_t *pDataY, uint8_t *pDataUV,
					     uint32_t strideY,
					     uint32_t strideUV)
{
	mfxU16 w, h, i, pitch;
	mfxU8 *ptr;
	mfxFrameInfo *pInfo = &pSurface->Info;
	mfxFrameData *pData = &pSurface->Data;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	} else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	pitch = pData->Pitch;
	ptr = static_cast<mfxU8 *>(
		(pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch));
	const size_t line_size = (size_t)w * 2;

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, line_size);

	// load UV plane
	h /= 2;
	ptr = static_cast<mfxU8 *>(
		(pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch));

	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataUV + i * strideUV, line_size);

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadNV12(mfxFrameSurface1 *pSurface,
					     uint8_t *pDataY, uint8_t *pDataUV,
					     uint32_t strideY,
					     uint32_t strideUV)
{
	mfxU16 w, h, i, pitch;
	mfxU8 *ptr;
	mfxFrameInfo *pInfo = &pSurface->Info;
	mfxFrameData *pData = &pSurface->Data;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	} else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	pitch = pData->Pitch;
	ptr = static_cast<mfxU8 *>(
		(pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch));

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, w);

	// load UV plane
	h /= 2;
	ptr = static_cast<mfxU8 *>(
		(pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch));

	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataUV + i * strideUV, w);

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadBGRA(mfxFrameSurface1 *pSurface,
					     uint8_t *pDataY, uint32_t strideY)
{
	mfxU16 w, h, i, pitch;
	mfxU8 *ptr;
	mfxFrameInfo *pInfo = &pSurface->Info;
	mfxFrameData *pData = &pSurface->Data;
	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	} else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	pitch = pData->Pitch;
	ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, w);

	ptr = static_cast<mfxU8 *>(
		(pData->B + pInfo->CropX + pInfo->CropY * pData->Pitch));

	for (i = 0; i < h; i++) {
		memcpy(ptr + i * pitch, pDataY + i * strideY, pitch);
	}

	return MFX_ERR_NONE;
}

int QSV_VPL_Encoder_Internal::GetFreeTaskIndex(Task *TaskPool, mfxU16 nPoolSize)
{
	if (TaskPool) {
		for (int i = 0; i < nPoolSize; i++) {
			if (static_cast<mfxSyncPoint>(nullptr) ==
			    TaskPool[i].syncp) {
				return i;
			}
		}
	}
	return MFX_ERR_NOT_FOUND;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(uint64_t ts, uint8_t *pDataY,
					   uint8_t *pDataUV, uint32_t strideY,
					   uint32_t strideUV,
					   mfxBitstream **pBS)
{
	mfxStatus sts = MFX_ERR_NONE;
	*pBS = nullptr;
	int n_TaskIdx = GetFreeTaskIndex(t_TaskPool, n_TaskNum);

	while (MFX_ERR_NOT_FOUND == n_TaskIdx) {
		// No more free tasks or surfaces, need to sync
		//sts = MFXVideoCORE_SyncOperation(
		//	mfx_Session, t_TaskPool[n_FirstSyncTask].syncp,
		//	static_cast<mfxU32>(100));

		mfxU8 *pTemp = mfx_Bitstream->Data;
		memcpy(mfx_Bitstream, t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS->Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS->DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS->DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp =
			static_cast<mfxSyncPoint>(nullptr);
		n_TaskIdx = n_FirstSyncTask;
		n_FirstSyncTask =
			(n_FirstSyncTask + 1) % static_cast<int>(n_TaskNum);
		*pBS = mfx_Bitstream;
	}

	sts = mfx_VideoENC->GetSurface(&mfx_SurfacePool);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_SurfacePool->FrameInterface->Map(mfx_SurfacePool,
						   MFX_MAP_WRITE);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_SurfacePool->Info = m_mfxEncParams.mfx.FrameInfo;

	(mfx_SurfacePool->Info.FourCC == MFX_FOURCC_P010)
		      ? LoadP010(mfx_SurfacePool, pDataY, pDataUV, strideY,
				 strideUV)
	      : (mfx_SurfacePool->Info.FourCC == MFX_FOURCC_NV12)
		      ? LoadNV12(mfx_SurfacePool, pDataY, pDataUV, strideY,
				 strideUV)
		      : LoadBGRA(mfx_SurfacePool, pDataY, strideY);

	mfx_SurfacePool->Data.TimeStamp = ts;

	sts = mfx_SurfacePool->FrameInterface->Unmap(mfx_SurfacePool);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	for (;;) {
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoENC->EncodeFrameAsync(
			nullptr, mfx_SurfacePool, t_TaskPool[n_TaskIdx].mfxBS,
			&t_TaskPool[n_TaskIdx].syncp);

		mfx_SurfacePool->FrameInterface->Release(mfx_SurfacePool);

		if (MFX_ERR_NONE == sts && t_TaskPool[n_TaskIdx].syncp) {
			sts = MFXVideoCORE_SyncOperation(
				mfx_Session, t_TaskPool[n_TaskIdx].syncp,
				static_cast<mfxU32>(INFINITE));

			break;
		} else if (MFX_ERR_NONE < sts && !t_TaskPool[n_TaskIdx].syncp) {
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(
					1); // Wait if device is busy, then repeat the same call
		} else if (MFX_ERR_NONE < sts && t_TaskPool[n_TaskIdx].syncp) {
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		} else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			// Allocate more bitstream buffer memory here if needed...
			break;
		} else {
			break;
		}
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain()
{
	mfxStatus sts = MFX_ERR_NONE;

	while (t_TaskPool && t_TaskPool[n_FirstSyncTask].syncp) {
		//mfx_VideoENC->EncodeFrameAsync(
		//	NULL, NULL, t_TaskPool[n_FirstSyncTask].mfxBS,
		//	&t_TaskPool[n_FirstSyncTask].syncp);
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp,
			static_cast<mfxU32>(INFINITE));
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % n_TaskNum;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::ClearData()
{
	mfxStatus sts = MFX_ERR_NONE;
	sts = Drain();

	if (mfx_VideoENC != nullptr) {
		sts = mfx_VideoENC->Close();
		delete mfx_VideoENC;
		mfx_VideoENC = nullptr;

		//m_mfxEncParams.~ExtBufManager();
	}

	mfx_SurfacePool = nullptr;

	if (t_TaskPool != nullptr) {
		for (int i = 0; i < n_TaskNum; i++) {
			delete t_TaskPool[i].mfxBS->Data;
			delete t_TaskPool[i].mfxBS;
		}
		delete[] t_TaskPool;
		t_TaskPool = nullptr;
	}

	if (mfx_Bitstream->Data != nullptr) {
		delete[] mfx_Bitstream->Data;
		mfx_Bitstream->Data = nullptr;

		delete mfx_Bitstream;
		mfx_Bitstream = nullptr;
	}

	if (sts >= MFX_ERR_NONE) {
		mfx_OpenEncodersNum--;
	}
#if defined(_WIN32) || defined(_WIN64)
	if (D3D11Device != nullptr || mfx_DX_Handle != nullptr) {
		D3D11Device->CleanupHWDevice();
		delete D3D11Device;
		D3D11Device = nullptr;
		mfx_DX_Handle = nullptr;
	}
#endif
	if (mfx_OpenEncodersNum <= 0) {
		mfx_DX_Handle = nullptr;
	}

	if (mfx_Session) {
		MFXClose(mfx_Session);
		MFXDispReleaseImplDescription(mfx_Loader, nullptr);
		MFXUnload(mfx_Loader);
		mfx_Session = nullptr;
		mfx_Loader = nullptr;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Reset(qsv_param_t *pParams,
					  enum qsv_codec codec)
{
	mfxStatus sts = ClearData();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = Open(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::GetCurrentFourCC(mfxU32 &fourCC)
{
	if (mfx_VideoENC != nullptr) {
		fourCC = m_mfxEncParams.mfx.FrameInfo.FourCC;
		return MFX_ERR_NONE;
	} else {
		return MFX_ERR_NOT_INITIALIZED;
	}
}
