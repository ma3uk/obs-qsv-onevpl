#include "helpers/common_directx11.h"
#include "obs-qsv-onevpl-encoder-internal.h"
#include "obs-qsv-onevpl-encoder.h"
#include "mfx.h"
#include <VersionHelpers.h>
#include <obs-module.h>

#define do_log(level, format, ...) \
	blog(level, "[qsv encoder: '%s'] " format, "vpl_impl", ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

static mfxU16 mfx_OpenEncodersNum = 0;
static mfxHDL mfx_DX_Handle = nullptr;

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxVersion &version,
						   bool isDGPU)
	: mfx_FrameSurf(nullptr),
	  mfx_VideoENC(nullptr),
	  mfx_InitParams(),
	  mfx_VideoParams(),
	  SPS_BufSize(1024),
	  PPS_BufSize(1024),
	  VPS_BufSize(1024),
	  PPS_Buffer(),
	  SPS_Buffer(),
	  VPS_Buffer(),
	  mfx_TaskPool(0),
	  t_TaskPool(nullptr),
	  n_TaskIdx(0),
	  n_FirstSyncTask(0),
	  mfx_SurfNum(),
	  mfx_Bitstream(),
	  b_isDGPU(isDGPU),
	  mfx_Ext_CO(),
	  mfx_Ext_CO2(),
	  mfx_Ext_CO3(),
	  mfx_Ext_CO_DDI(),
	  mfx_ENC_ExtendedBuffers(),
	  mfx_CtrlExtendedBuffers(),
	  mfx_ENC_Params(),
	  mfx_PPSSPSVPS_Params(),
	  mfx_VPPDenoise(),
	  //mfx_Ext_TuneQuality(),
	  mfx_FrameAllocator(),
	  mfx_FrameAllocResponse(),
	  //mfx_EncToolsConf(),
	  mfx_Ext_VP9Param(),
	  mfx_Ext_AV1BitstreamParam(),
	  mfx_Ext_AV1ResolutionParam(),
	  mfx_Ext_AV1TileParam(),
	  mfx_Ext_AV1AuxData(),
	  mfx_Ext_HEVCParam(),
	  mfx_Ext_VideoSignalInfo(),
	  mfx_Ext_ChromaLocInfo(),
	  mfx_Ext_MasteringDisplayColourVolume(),
	  mfx_Ext_ContentLightLevelInfo(),
	  mfx_Ext_CO_VPS(),
	  mfx_Ext_CO_SPSPPS(),
	  mfx_FrameAllocRequest(),
	  mfx_EncControl(),
	  mfx_HyperModeParam(),
	  mfx_Impl(),
	  mfx_Version(),
	  mfx_Session(),
	  mfx_Loader(),
	  mfx_Platform(),
	  mfx_LoaderVariant(),
	  mfx_LoaderConfig()
{
	mfxIMPL tempImpl = MFX_IMPL_VIA_D3D11;
	mfx_Loader = MFXLoad();
	mfxStatus sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
	if (sts == MFX_ERR_NONE) {
		sts = MFXQueryVersion(mfx_Session, &version);
		if (sts == MFX_ERR_NONE) {
			mfx_Version = version;
			sts = MFXQueryIMPL(mfx_Session, &tempImpl);
			if (sts == MFX_ERR_NONE) {
				mfx_Impl = tempImpl;
				blog(LOG_INFO,
				     "\tImplementation:           D3D11\n"
				     "\tsurf:           D3D11\n");
			}
		} else {
			blog(LOG_INFO, "\tImplementation: [ERROR]\n");
		}
	} else {
		blog(LOG_INFO, "\tImplementation: [ERROR]\n");
	}
	MFXClose(mfx_Session);
	MFXUnload(mfx_Loader);
	return;
}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal()
{
	if (mfx_VideoENC) {
		ClearData();
	}
}
mfxStatus
QSV_VPL_Encoder_Internal::Initialize(mfxVersion ver, mfxSession *pSession,
				     mfxFrameAllocator *mfx_FrameAllocator,
				     mfxHDL *deviceHandle, int deviceNum)
{
	mfx_FrameAllocator; // (Hugh) Currently unused

	mfxStatus sts = MFX_ERR_NONE;

	// If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
	if (mfx_FrameAllocator) {
		// Initialize VPL Session
		mfx_Loader = MFXLoad();
		mfx_LoaderConfig[0] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[0].Type = MFX_VARIANT_TYPE_U32;
		mfx_LoaderVariant[0].Data.U32 = mfxU32(0x8086);
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[0],
			(mfxU8 *)"mfxImplDescription.VendorID",
			mfx_LoaderVariant[0]);

		mfx_LoaderConfig[1] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[1].Type = MFX_VARIANT_TYPE_PTR;
		mfx_LoaderVariant[1].Data.Ptr = mfxHDL("mfx-gen");
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[1],
			(mfxU8 *)"mfxImplDescription.ImplName",
			mfx_LoaderVariant[1]);

		mfx_LoaderConfig[2] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[2].Type = MFX_VARIANT_TYPE_U32;
		mfx_LoaderVariant[2].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[2],
			(mfxU8 *)"mfxImplDescription.Impl.mfxImplType",
			mfx_LoaderVariant[2]);

		mfx_LoaderConfig[3] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[3].Type = MFX_VARIANT_TYPE_U32;
		mfx_LoaderVariant[3].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[3],
			(mfxU8 *)"mfxImplDescription.mfxAccelerationModeDescription.Mode",
			mfx_LoaderVariant[3]);

		mfx_LoaderConfig[4] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[4].Type = MFX_VARIANT_TYPE_U16;
		mfx_LoaderVariant[4].Data.U16 = MFX_GPUCOPY_ON;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[4],
			(mfxU8 *)"mfxInitializationParam.DeviceCopy",
			mfx_LoaderVariant[4]);

		mfx_LoaderConfig[5] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[5].Type = MFX_VARIANT_TYPE_U16;
		mfx_LoaderVariant[5].Data.U16 = MFX_GPUCOPY_ON;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[5],
			(mfxU8 *)"mfxInitializationParam.DeviceCopy",
			mfx_LoaderVariant[5]);

		mfx_LoaderConfig[6] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[6].Type = MFX_VARIANT_TYPE_U32;
		mfx_LoaderVariant[6].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[6],
			(mfxU8 *)"mfxInitializationParam.AccelerationMode",
			mfx_LoaderVariant[6]);

		mfx_LoaderConfig[7] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[7].Type = MFX_VARIANT_TYPE_U16;
		mfx_LoaderVariant[7].Data.U16 = (mfxU16)32;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[7],
			(mfxU8 *)"mfxInitializationParam.mfxExtThreadsParam.NumThread",
			mfx_LoaderVariant[7]);

		mfx_LoaderConfig[8] = MFXCreateConfig(mfx_Loader);
		mfx_LoaderVariant[8].Type = MFX_VARIANT_TYPE_U32;
		mfx_LoaderVariant[8].Data.U32 = MFX_PRIORITY_NORMAL;
		MFXSetConfigFilterProperty(
			mfx_LoaderConfig[8],
			(mfxU8 *)"mfxInitializationParam.mfxExtThreadsParam.Priority",
			mfx_LoaderVariant[8]);

		mfxStatus sts = MFXCreateSession(
			mfx_Loader, (deviceNum >= 0) ? deviceNum : 0,
			&mfx_Session);

		MFXQueryIMPL(mfx_Session, &mfx_Impl);

		MFXSetPriority(mfx_Session, MFX_PRIORITY_HIGH);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		// Create DirectX device context
		if (mfx_DX_Handle == nullptr) {
			sts = CreateHWDevice(mfx_Session, &mfx_DX_Handle);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		}

		if (mfx_DX_Handle == nullptr)
			return MFX_ERR_DEVICE_FAILED;

		// Provide device manager to VPL

		sts = MFXVideoCORE_SetHandle(mfx_Session, DEVICE_MGR_TYPE,
					     mfx_DX_Handle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfx_FrameAllocator->pthis =
			mfx_Session; // We use VPL session ID as the allocation identifier
		mfx_FrameAllocator->Alloc = simple_alloc;
		mfx_FrameAllocator->Free = simple_free;
		mfx_FrameAllocator->Lock = simple_lock;
		mfx_FrameAllocator->Unlock = simple_unlock;
		mfx_FrameAllocator->GetHDL = simple_gethdl;

		// Since we are using video memory we must provide VPL with an external allocator
		sts = MFXVideoCORE_SetFrameAllocator(mfx_Session,
						     mfx_FrameAllocator);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

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
	mfxStatus sts = Initialize(mfx_Version, &mfx_Session,
				   &mfx_FrameAllocator, &mfx_DX_Handle,
				   pParams->nDeviceNum);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_VideoENC = new MFXVideoENCODE(mfx_Session);

	sts = InitENCParams(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitEncCtrlParams(pParams, codec);
	/*MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);*/

	sts = mfx_VideoENC->Query(&mfx_ENC_Params, &mfx_ENC_Params);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = AllocateSurfaces();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_VideoENC->Init(&mfx_ENC_Params);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = GetVideoParam(codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitBitstream();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (sts >= MFX_ERR_NONE) {
		mfx_OpenEncodersNum++;
	} else {
		blog(LOG_INFO, "\tOpen encoder: [ERROR]");
		mfx_VideoENC->Close();
		MFXClose(mfx_Session);
		MFXUnload(mfx_Loader);
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitEncCtrlParams(qsv_param_t *pParams,
						      enum qsv_codec codec)
{
	//if (codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) {
	//	INIT_MFX_EXT_BUFFER(mfx_AVCRoundingOffset,
	//			    MFX_EXTBUFF_AVC_ROUNDING_OFFSET);
	//	mfx_AVCRoundingOffset.EnableRoundingIntra = MFX_CODINGOPTION_ON;
	//	mfx_AVCRoundingOffset.RoundingOffsetIntra = 0;
	//	mfx_AVCRoundingOffset.EnableRoundingInter = MFX_CODINGOPTION_ON;
	//	mfx_AVCRoundingOffset.RoundingOffsetInter = 0;
	//	mfx_CtrlExtendedBuffers.push_back(
	//		(mfxExtBuffer *)&mfx_AVCRoundingOffset);
	//}
	//mfx_EncControl.ExtParam = mfx_ENC_ExtendedBuffers.data();
	//mfx_EncControl.NumExtParam = (mfxU16)mfx_CtrlExtendedBuffers.size();

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::InitENCParams(qsv_param_t *pParams,
						  enum qsv_codec codec)
{
	/*It's only for debug*/
	int mfx_Ext_CO_enable = 1;
	int mfx_Ext_CO2_enable = 1;
	int mfx_Ext_CO3_enable = 1;
	int mfx_Ext_CO_DDI_enable = 1;

	mfx_ENC_Params.mfx.NumThread = 64;
	//mfx_ENC_Params.mfx.RestartInterval = 1;

	switch (codec) {
	case QSV_CODEC_AV1:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_AV1;
		/*mfx_ENC_Params.mfx.CodecLevel = 53;*/
		break;
	case QSV_CODEC_HEVC:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_HEVC;
		mfx_ENC_Params.mfx.CodecLevel = 61;
		break;
	case QSV_CODEC_VP9:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_VP9;
		break;
	case QSV_CODEC_AVC:
	default:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_AVC;
		/*mfx_ENC_Params.mfx.CodecLevel = 52;*/
		break;
	}

	if ((pParams->nNumRefFrame >= 0) && (pParams->nNumRefFrame < 16)) {
		mfx_ENC_Params.mfx.NumRefFrame = pParams->nNumRefFrame;
		blog(LOG_INFO, "\tNumRefFrame set to: %d",
		     pParams->nNumRefFrame);
	}

	mfx_ENC_Params.mfx.TargetUsage = pParams->nTargetUsage;
	mfx_ENC_Params.mfx.CodecProfile = pParams->CodecProfile;
	if (codec == QSV_CODEC_HEVC) {
		mfx_ENC_Params.mfx.CodecProfile |= pParams->HEVCTier;
	}
	mfx_ENC_Params.mfx.FrameInfo.FrameRateExtN = pParams->nFpsNum;
	mfx_ENC_Params.mfx.FrameInfo.FrameRateExtD = pParams->nFpsDen;

	mfx_ENC_Params.vpp.In.FrameRateExtN = pParams->nFpsNum;
	mfx_ENC_Params.vpp.In.FrameRateExtD = pParams->nFpsDen;

	mfx_ENC_Params.mfx.FrameInfo.FourCC = pParams->nFourCC;
	mfx_ENC_Params.vpp.In.FourCC = pParams->nFourCC;

	mfx_ENC_Params.mfx.FrameInfo.BitDepthChroma =
		pParams->video_fmt_10bit ? 10 : 8;
	mfx_ENC_Params.vpp.In.BitDepthChroma = pParams->video_fmt_10bit ? 10
									: 8;

	mfx_ENC_Params.mfx.FrameInfo.BitDepthLuma =
		pParams->video_fmt_10bit ? 10 : 8;
	mfx_ENC_Params.vpp.In.BitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;

	if (pParams->video_fmt_10bit) {
		mfx_ENC_Params.mfx.FrameInfo.Shift = 1;
		mfx_ENC_Params.vpp.In.Shift = 1;
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture

	if (codec == QSV_CODEC_AVC) {

		mfx_ENC_Params.mfx.FrameInfo.Width =
			MSDK_ALIGN16(pParams->nWidth);
		mfx_ENC_Params.vpp.In.Width = MSDK_ALIGN16(pParams->nWidth);

		mfx_ENC_Params.mfx.FrameInfo.Height =
			MSDK_ALIGN16(pParams->nHeight);
		mfx_ENC_Params.vpp.In.Height = MSDK_ALIGN16(pParams->nHeight);
	} else {
		mfx_ENC_Params.mfx.FrameInfo.Width =
			MSDK_ALIGN32(pParams->nWidth);
		mfx_ENC_Params.vpp.In.Width = MSDK_ALIGN32(pParams->nWidth);

		mfx_ENC_Params.mfx.FrameInfo.Height =
			MSDK_ALIGN32(pParams->nHeight);
		mfx_ENC_Params.vpp.In.Height = MSDK_ALIGN32(pParams->nHeight);
	}

	mfx_ENC_Params.mfx.FrameInfo.ChromaFormat = pParams->nChromaFormat;
	mfx_ENC_Params.vpp.In.ChromaFormat = pParams->nChromaFormat;

	mfx_ENC_Params.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_ENC_Params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

	mfx_ENC_Params.mfx.FrameInfo.CropX = 0;
	mfx_ENC_Params.vpp.In.CropX = 0;

	mfx_ENC_Params.mfx.FrameInfo.CropY = 0;
	mfx_ENC_Params.vpp.In.CropY = 0;

	mfx_ENC_Params.mfx.FrameInfo.CropW = pParams->nWidth;
	mfx_ENC_Params.vpp.In.CropW = pParams->nWidth;

	mfx_ENC_Params.mfx.FrameInfo.CropH = pParams->nHeight;
	mfx_ENC_Params.vpp.In.CropH = pParams->nHeight;

	switch ((int)pParams->bLowPower) {
	case 0:
		mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_OFF;
		blog(LOG_INFO, "\tLowpower set: OFF");
		break;
	case 1:
		mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
		break;
	default:
		mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
		blog(LOG_INFO, "\tLowpower set: AUTO");
		break;
	}

	enum qsv_cpu_platform qsv_platform = qsv_get_cpu_platform();

	//if (b_isDGPU == true || (pParams->nGOPRefDist == 1) ||
	//    qsv_platform > QSV_CPU_PLATFORM_RPL) {
	//	mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_ON;
	//	blog(LOG_INFO,
	//	     "\tThe Lowpower mode parameter was automatically changed to: ON");

	//	if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
	//	    pParams->RateControl == MFX_RATECONTROL_LA_HRD ||
	//	    pParams->RateControl == MFX_RATECONTROL_LA) {
	//		pParams->RateControl = MFX_RATECONTROL_VBR;
	//		blog(LOG_INFO,
	//		     "\tRatecontrol is automatically changed to: VBR");
	//	}
	//}

	mfx_ENC_Params.mfx.RateControlMethod = (mfxU16)pParams->RateControl;

	/*This is a multiplier to bypass the limitation of the 16 bit value of
		variables*/
	mfx_ENC_Params.mfx.BRCParamMultiplier = 100;
	int LABuffer = ((pParams->nTargetBitRate /
			 (pParams->nFpsNum / pParams->nFpsDen)) *
			(pParams->nLADepth + 34));

	switch (pParams->RateControl) {
	case MFX_RATECONTROL_LA_HRD:
		mfx_ENC_Params.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.MaxKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.BufferSizeInKB = (mfxU16)LABuffer;

		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		mfx_ENC_Params.mfx.InitialDelayInKB =
			(mfxU16)(mfx_ENC_Params.mfx.BufferSizeInKB / 8);
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB * 100);
		break;
	case MFX_RATECONTROL_CBR:
		mfx_ENC_Params.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.MaxKbps = (mfxU16)0;
		mfx_ENC_Params.mfx.BufferSizeInKB =
			pParams->nLADepth > 10
				? LABuffer
				: (mfxU16)(pParams->nTargetBitRate / 8);
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		mfx_ENC_Params.mfx.InitialDelayInKB =
			(mfxU16)(mfx_ENC_Params.mfx.BufferSizeInKB / 8);
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB * 100);
		break;
	case MFX_RATECONTROL_VBR:
	case MFX_RATECONTROL_VCM:
		mfx_ENC_Params.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.MaxKbps = pParams->nMaxBitRate;
		mfx_ENC_Params.mfx.BufferSizeInKB =
			(mfxU16)(mfx_ENC_Params.mfx.MaxKbps / 8);
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB * 100);
		break;
	case MFX_RATECONTROL_CQP:
		mfx_ENC_Params.mfx.QPI = (mfxU16)pParams->nQPI;
		mfx_ENC_Params.mfx.QPB = (mfxU16)pParams->nQPB;
		mfx_ENC_Params.mfx.QPP = (mfxU16)pParams->nQPP;
		break;
	case MFX_RATECONTROL_AVBR:
		mfx_ENC_Params.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.Accuracy = (mfxU16)pParams->nAccuracy;
		mfx_ENC_Params.mfx.Convergence = (mfxU16)pParams->nConvergence;
		break;
	case MFX_RATECONTROL_ICQ:
	case MFX_RATECONTROL_LA_ICQ:
		mfx_ENC_Params.mfx.ICQQuality = (mfxU16)pParams->nICQQuality;
		break;
	case MFX_RATECONTROL_LA:
		mfx_ENC_Params.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.BufferSizeInKB =
			pParams->nLADepth > 1
				? (mfxU16)LABuffer
				: (mfxU16)(mfx_ENC_Params.mfx.TargetKbps / 8);
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		mfx_ENC_Params.mfx.InitialDelayInKB =
			(mfxU16)(mfx_ENC_Params.mfx.BufferSizeInKB / 8);
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB * 100);
		break;
	}

	mfx_ENC_Params.AsyncDepth = (mfxU16)pParams->nAsyncDepth;

	if (pParams->nKeyIntSec == 0) {
		mfx_ENC_Params.mfx.GopPicSize =
			(mfxU16)(((pParams->nFpsNum + pParams->nFpsDen - 1) /
				  pParams->nFpsDen) *
				 10);
	} else {
		mfx_ENC_Params.mfx.GopPicSize =
			(mfxU16)(pParams->nKeyIntSec * pParams->nFpsNum /
				 (float)pParams->nFpsDen);
	}
	blog(LOG_INFO, "\tGopPicSize set to: %d",
	     mfx_ENC_Params.mfx.GopPicSize);

	switch (pParams->bGopOptFlag) {
	case 1:
		mfx_ENC_Params.mfx.GopOptFlag = 0x00;
		blog(LOG_INFO, "\tGopOptFlag set: OPEN");
		switch (codec) {
		case QSV_CODEC_HEVC:
			mfx_ENC_Params.mfx.IdrInterval =
				1 + ((pParams->nFpsNum + pParams->nFpsDen - 1) /
				     pParams->nFpsDen) *
					    600 / mfx_ENC_Params.mfx.GopPicSize;
			mfx_ENC_Params.mfx.NumSlice = 0;
			break;
		case QSV_CODEC_AVC:
			mfx_ENC_Params.mfx.IdrInterval =
				((pParams->nFpsNum + pParams->nFpsDen - 1) /
				 pParams->nFpsDen) *
				600 / mfx_ENC_Params.mfx.GopPicSize;
			mfx_ENC_Params.mfx.NumSlice = 1;
			break;
		default:
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
			break;
		}
		break;

	case 2:
		mfx_ENC_Params.mfx.GopOptFlag = MFX_GOP_STRICT;
		blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
		switch (codec) {
		case QSV_CODEC_HEVC:
			mfx_ENC_Params.mfx.IdrInterval = 1;
			mfx_ENC_Params.mfx.NumSlice = 0;
			break;
		default:
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
			break;
		}
		break;

	default:
		mfx_ENC_Params.mfx.GopOptFlag = MFX_GOP_CLOSED;
		blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
		switch (codec) {
		case QSV_CODEC_HEVC:
			mfx_ENC_Params.mfx.IdrInterval = 1;
			mfx_ENC_Params.mfx.NumSlice = 0;
			break;
		default:
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
			break;
		}
		break;
	}

	switch (codec) {
	case QSV_CODEC_AV1:
		mfx_ENC_Params.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_ENC_Params.mfx.GopRefDist > 33) {
			mfx_ENC_Params.mfx.GopRefDist = 33;
		} else if (mfx_ENC_Params.mfx.GopRefDist > 16 &&
			   (pParams->bLAExtBRC == 1 ||
			    pParams->bCPUEncTools != 0)) {
			mfx_ENC_Params.mfx.GopRefDist = 16;
			blog(LOG_INFO,
			     "\tExtBRC and CPUEncTools does not support GOPRefDist greater than 16.");
		}
		break;
	default:
		mfx_ENC_Params.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_ENC_Params.mfx.GopRefDist > 16) {
			mfx_ENC_Params.mfx.GopRefDist = 16;
		}
		break;
	}

	if ((pParams->bLAExtBRC == 1 || pParams->bCPUEncTools != 0) &&
	    (mfx_ENC_Params.mfx.GopRefDist != 1 &&
	     mfx_ENC_Params.mfx.GopRefDist != 2 &&
	     mfx_ENC_Params.mfx.GopRefDist != 4 &&
	     mfx_ENC_Params.mfx.GopRefDist != 8 &&
	     mfx_ENC_Params.mfx.GopRefDist != 16)) {
		mfx_ENC_Params.mfx.GopRefDist = 4;
		blog(LOG_INFO,
		     "\tExtBRC and CPUEncTools only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
	}
	blog(LOG_INFO, "\tGOPRefDist set to:     %d",
	     (int)mfx_ENC_Params.mfx.GopRefDist);

	if (mfx_Ext_CO_enable == 1 && codec != QSV_CODEC_VP9) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO, MFX_EXTBUFF_CODING_OPTION);
		/*Don't touch it!*/
		mfx_Ext_CO.CAVLC = MFX_CODINGOPTION_OFF;
		mfx_Ext_CO.MaxDecFrameBuffering =
			pParams->nLADepth > 0
				? pParams->nLADepth +
					  (pParams->nNumRefFrame * 3)
				: pParams->nNumRefFrame;
		//mfx_Ext_CO.ResetRefList = MFX_CODINGOPTION_OFF;
		mfx_Ext_CO.FieldOutput = MFX_CODINGOPTION_ON;
		mfx_Ext_CO.IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_Ext_CO.InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_Ext_CO.MVPrecision = MFX_MVPRECISION_QUARTERPEL;
		mfx_Ext_CO.MECostType = (mfxU16)8;
		mfx_Ext_CO.MESearchType = (mfxU16)16;
		mfx_Ext_CO.MVSearchWindow.x = (mfxI16)4;
		mfx_Ext_CO.MVSearchWindow.y = (mfxI16)4;

		if (pParams->bIntraRefEncoding == 1) {
			mfx_Ext_CO.RecoveryPointSEI = MFX_CODINGOPTION_ON;
		}

		switch ((int)pParams->bRDO) {
		case 0:
			mfx_Ext_CO.RateDistortionOpt = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tRDO set: OFF");
			break;
		case 1:
			mfx_Ext_CO.RateDistortionOpt = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tRDO set: ON");
			break;
		default:
			blog(LOG_INFO, "\tRDO set: AUTO");
			break;
		}

		if (mfx_ENC_Params.mfx.RateControlMethod ==
			    MFX_RATECONTROL_ICQ ||
		    mfx_ENC_Params.mfx.RateControlMethod ==
			    MFX_RATECONTROL_CQP ||
		    mfx_ENC_Params.mfx.RateControlMethod ==
			    MFX_RATECONTROL_LA_ICQ) {
			mfx_Ext_CO.VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
			mfx_Ext_CO.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
			mfx_Ext_CO.NalHrdConformance = MFX_CODINGOPTION_OFF;
		} else {
			mfx_Ext_CO.VuiVclHrdParameters = MFX_CODINGOPTION_ON;
			mfx_Ext_CO.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
			mfx_Ext_CO.NalHrdConformance = MFX_CODINGOPTION_ON;
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO);
	}

	if (mfx_Ext_CO2_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO2, MFX_EXTBUFF_CODING_OPTION2);

		mfx_Ext_CO2.BufferingPeriodSEI = MFX_BPSEI_IFRAME;
		mfx_Ext_CO2.EnableMAD = MFX_CODINGOPTION_ON;

		//if ((mfx_ENC_Params.mfx.RateControlMethod ==
		//	     MFX_RATECONTROL_CBR ||
		//     mfx_ENC_Params.mfx.RateControlMethod ==
		//	     MFX_RATECONTROL_VBR ||
		//     mfx_ENC_Params.mfx.RateControlMethod ==
		//	     MFX_RATECONTROL_ICQ) &&
		//    pParams->bLAExtBRC == 1) {
		//	mfx_Ext_CO2.ExtBRC = MFX_CODINGOPTION_ON;
		//}

		switch ((int)pParams->bCPUEncTools) {
		case -1:
		case 0:
			blog(LOG_INFO, "\tExtBRC set: AUTO");
			break;
		default:
		case 1:
			mfx_Ext_CO2.ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: ON");
			break;
		}

		if (pParams->bIntraRefEncoding == 1) {

			switch ((int)pParams->nIntraRefType) {
			case 0:
				mfx_Ext_CO2.IntRefType = MFX_REFRESH_VERTICAL;
				blog(LOG_INFO, "\tIntraRefType set: VERTICAL");
				break;
			default:
			case 1:
				mfx_Ext_CO2.IntRefType = MFX_REFRESH_HORIZONTAL;
				blog(LOG_INFO,
				     "\tIntraRefType set: HORIZONTAL");
				break;
			}

			mfx_Ext_CO2.IntRefCycleSize =
				(mfxU16)(pParams->nIntraRefCycleSize > 1
						 ? pParams->nIntraRefCycleSize
						 : (mfx_ENC_Params.mfx.GopRefDist >
								    1
							    ? mfx_ENC_Params.mfx
								      .GopRefDist
							    : 2));
			blog(LOG_INFO, "\tIntraRefCycleSize set: %d",
			     mfx_Ext_CO2.IntRefCycleSize);
			if (pParams->nIntraRefQPDelta > -52 &&
			    pParams->nIntraRefQPDelta < 52) {
				mfx_Ext_CO2.IntRefQPDelta =
					(mfxI16)pParams->nIntraRefQPDelta;
				blog(LOG_INFO, "\tIntraRefQPDelta set: %d",
				     mfx_Ext_CO2.IntRefQPDelta);
			}
		}
		/*Starting from the VPL 2.9 version,
			this is a deprecate parameter,
			but it needs to be turned off,
			because it greatly spoils the
				picture.*/
		if (mfx_Version.Major == 2 && mfx_Version.Minor <= 8) {
			mfx_Ext_CO2.BitrateLimit = MFX_CODINGOPTION_OFF;
		}
		if (mfx_ENC_Params.mfx.LowPower == MFX_CODINGOPTION_ON) {
			if (codec == QSV_CODEC_AVC) {
				mfx_Ext_CO2.RepeatPPS = MFX_CODINGOPTION_OFF;
			}
		}

		if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_HEVC) {
			mfx_Ext_CO2.RepeatPPS = MFX_CODINGOPTION_ON;
		}

		if (pParams->bLAExtBRC == 1) {
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:     %d",
			     pParams->nLADepth);
		} else if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
			   pParams->RateControl == MFX_RATECONTROL_LA ||
			   pParams->RateControl == MFX_RATECONTROL_LA_HRD) {
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to: %d",
			     pParams->nLADepth);
		} else if (mfx_ENC_Params.mfx.RateControlMethod ==
				   MFX_RATECONTROL_VBR &&
			   mfx_ENC_Params.mfx.LowPower == MFX_CODINGOPTION_ON &&
			   pParams->nLADepth > 0) {
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to: %d",
			     pParams->nLADepth);
		}

		if (codec != QSV_CODEC_VP9) {
			if (pParams->bMBBRC == 1) {
				mfx_Ext_CO2.MBBRC = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tMBBRC set: ON");
			} else if (pParams->bMBBRC == 0) {
				mfx_Ext_CO2.MBBRC = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tMBBRC set: OFF");
			}
		}

		if (pParams->nGOPRefDist > 1) {
			mfx_Ext_CO2.BRefType = MFX_B_REF_PYRAMID;
			blog(LOG_INFO, "\tBPyramid set: ON");
		} else {
			blog(LOG_INFO, "\tBPyramid set: AUTO");
		}

		switch ((int)pParams->nTrellis) {
		case 0:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_OFF;
			blog(LOG_INFO, "\tTrellis set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_I;
			blog(LOG_INFO, "\tTrellis set: I");
			break;
		case 2:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: IP");
			break;
		case 3:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P |
					      MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IPB");
			break;
		case 4:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IB");
			break;
		case 5:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: P");
			break;
		case 6:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: PB");
			break;
		case 7:
			mfx_Ext_CO2.Trellis = MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: B");
			break;
		default:
			blog(LOG_INFO, "\tTrellis set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveB) {
		case 0:
			mfx_Ext_CO2.AdaptiveI = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveI set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.AdaptiveI = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveI set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveI set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveB) {
		case 0:
			mfx_Ext_CO2.AdaptiveB = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveB set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.AdaptiveB = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveB set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveB set: AUTO");
			break;
		}

		if ((pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
		     pParams->RateControl == MFX_RATECONTROL_LA ||
		     pParams->RateControl == MFX_RATECONTROL_LA_HRD) ||
		    ((pParams->nLADepth >= 1) &&
		     (pParams->RateControl == MFX_RATECONTROL_CBR ||
		      pParams->RateControl == MFX_RATECONTROL_VBR))) {
			switch ((int)pParams->nLookAheadDS) {
			case 0:
				mfx_Ext_CO2.LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
				blog(LOG_INFO, "\tLookAheadDS set: SLOW");
				break;
			case 1:
				mfx_Ext_CO2.LookAheadDS = MFX_LOOKAHEAD_DS_2x;
				blog(LOG_INFO, "\tLookAheadDS set: MEDIUM");
				break;
			case 2:
				mfx_Ext_CO2.LookAheadDS = MFX_LOOKAHEAD_DS_4x;
				blog(LOG_INFO, "\tLookAheadDS set: FAST");
				break;
			default:
				blog(LOG_INFO, "\tLookAheadDS set: AUTO");
				break;
			}

			mfx_Ext_CO2.MaxFrameSize = mfxU32(std::min<size_t>(
				UINT_MAX,
				size_t(mfx_ENC_Params.mfx.FrameInfo.Width *
					       mfx_ENC_Params.mfx.FrameInfo
						       .Height *
					       1 / (16u * 16u) * (3200 / 8) +
				       999u) /
					1000u));
		}

		switch ((int)pParams->bRawRef) {
		case 0:
			mfx_Ext_CO2.UseRawRef = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tUseRawRef set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.UseRawRef = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tUseRawRef set: ON");
			break;
		default:
			blog(LOG_INFO, "\tUseRawRef set: AUTO");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO2);
	}

	if (mfx_Ext_CO3_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO3, MFX_EXTBUFF_CODING_OPTION3);
		/*Don't touch it!*/
		if (pParams->video_fmt_10bit) {
			mfx_Ext_CO3.TargetBitDepthLuma = 10;
			mfx_Ext_CO3.TargetBitDepthChroma = 10;
		} else {
			mfx_Ext_CO3.TargetBitDepthLuma = 8;
			mfx_Ext_CO3.TargetBitDepthChroma = 8;
		}

		if (codec != QSV_CODEC_AVC) {
			mfx_Ext_CO3.RepartitionCheckEnable =
				MFX_CODINGOPTION_ON;
			mfx_Ext_CO3.FadeDetection = MFX_CODINGOPTION_ON;
		}

		//mfx_Ext_CO3.MBDisableSkipMap = MFX_CODINGOPTION_ON;
		//mfx_Ext_CO3.EnableQPOffset = MFX_CODINGOPTION_ON;
		//mfx_Ext_CO3.QPOffset = ;
		mfx_Ext_CO3.BitstreamRestriction = MFX_CODINGOPTION_ON;
		mfx_Ext_CO3.TimingInfoPresent = MFX_CODINGOPTION_ON;

		mfx_Ext_CO3.IntRefCycleDist = (mfxU16)0;

		//mfx_Ext_CO3.ContentInfo = MFX_CONTENT_NOISY_VIDEO |
		//			  MFX_CONTENT_FULL_SCREEN_VIDEO |
		//			  MFX_CONTENT_NON_VIDEO_SCREEN;

		//if (pParams->nLADepth < 1) {
		//	mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_LIVE_STREAMING;
		//} else {
		//	mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
		//}

		if (mfx_ENC_Params.mfx.RateControlMethod ==
		    MFX_RATECONTROL_CQP) {
			mfx_Ext_CO3.EnableMBQP = MFX_CODINGOPTION_ON;
		}

		/*This parameter sets active references for frames. This is fucking magic, it may work with LookAhead,
			or it may not work,
			it seems to depend on the IDE and
				compiler. If you get frame drops, delete it.*/

		//if (codec == QSV_CODEC_AVC) {
		//	if (pParams->bCPUEncTools == 1 &&
		//	    pParams->nLADepth >= 1) {
		//	} else {
		//		if (pParams->nTargetUsage ==
		//			    MFX_TARGETUSAGE_1 &&
		//		    pParams->nGOPRefDist > 4) {
		//			for (int i = 0; i < 8; i++) {

		//				mfx_Ext_CO3.NumRefActiveP
		//					[0] = (mfxU16)AVCGetMaxNumRefActivePL0(
		//					pParams->nTargetUsage,
		//					mfx_ENC_Params.mfx
		//						.LowPower,
		//					mfx_ENC_Params.mfx
		//						.FrameInfo);

		//				mfx_Ext_CO3.NumRefActiveBL0
		//					[0] = (mfxU16)
		//					AVCGetMaxNumRefActiveBL0(
		//						pParams->nTargetUsage,
		//						mfx_ENC_Params
		//							.mfx
		//							.LowPower);
		//				mfx_Ext_CO3.NumRefActiveBL1
		//					[0] = (mfxU16)AVCGetMaxNumRefActiveBL1(
		//					pParams->nTargetUsage,
		//					mfx_ENC_Params.mfx
		//						.LowPower,
		//					mfx_ENC_Params.mfx
		//						.FrameInfo);
		//			}
		//		}
		//	}
		if (codec == QSV_CODEC_HEVC) {
			if (pParams->nNumRefFrameLayers > 0) {
				for (int i = 0; i < pParams->nNumRefFrameLayers;
				     i++) {

					mfx_Ext_CO3.NumRefActiveP[0] = (mfxU16)
						HEVCGetMaxNumRefActivePL0(
							pParams->nTargetUsage,
							mfx_ENC_Params.mfx
								.LowPower,
							mfx_ENC_Params.mfx
								.FrameInfo);

					mfx_Ext_CO3.NumRefActiveBL0[0] = (mfxU16)
						HEVCGetMaxNumRefActiveBL0(
							pParams->nTargetUsage,
							mfx_ENC_Params.mfx
								.LowPower);
					mfx_Ext_CO3.NumRefActiveBL1[0] = (mfxU16)
						HEVCGetMaxNumRefActiveBL1(
							pParams->nTargetUsage,
							mfx_ENC_Params.mfx
								.LowPower,
							mfx_ENC_Params.mfx
								.FrameInfo);
					blog(LOG_INFO,
					     "\tNumRefFrameLayer %d set: %d", i,
					     pParams->nNumRefFrame);
				}
			}
		} else if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) {
			if (pParams->nNumRefFrameLayers > 0) {
				for (int i = 0; i < pParams->nNumRefFrameLayers;
				     i++) {

					mfx_Ext_CO3.NumRefActiveP[0] =
						(mfxU16)pParams->nNumRefFrame;
					mfx_Ext_CO3.NumRefActiveBL0[0] =
						(mfxU16)pParams->nNumRefFrame;
					mfx_Ext_CO3.NumRefActiveBL1[0] =
						(mfxU16)pParams->nNumRefFrame;
					blog(LOG_INFO,
					     "\tNumRefFrameLayer %d set: %d", i,
					     pParams->nNumRefFrame);
				}
			}
		}

		if (codec == QSV_CODEC_HEVC) {
			switch ((int)pParams->bGPB) {
			case 0:
				mfx_Ext_CO3.GPB = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tGPB set: OFF");
				break;
			case 1:
				mfx_Ext_CO3.GPB = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tGPB set: ON");
				break;
			default:
				blog(LOG_INFO, "\tGPB set: AUTO");
				break;
			}
		}

		if ((int)pParams->nGOPRefDist == 1) {
			mfx_Ext_CO3.PRefType = MFX_P_REF_PYRAMID;
			blog(LOG_INFO, "\tPRef set: PYRAMID");
		} else {
			mfx_Ext_CO3.PRefType = MFX_P_REF_DEFAULT;
		}

		switch ((int)pParams->bAdaptiveCQM) {
		case 0:
			mfx_Ext_CO3.AdaptiveCQM = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveCQM set: OFF");
			break;
		case 1:
			mfx_Ext_CO3.AdaptiveCQM = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveCQM set: ON");
			break;
		default:
			blog(LOG_INFO, "\tAdaptiveCQM set: AUTO");
			break;
		}

		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 4) {

			switch ((int)pParams->bAdaptiveRef) {
			case 0:
				mfx_Ext_CO3.AdaptiveRef = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveRef set: OFF");
				break;
			case 1:
				mfx_Ext_CO3.AdaptiveRef = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tAdaptiveRef set: ON");
				break;
			default:
				blog(LOG_INFO, "\tAdaptiveRef set: AUTO");
				break;
			}

			/*A temporary solution until the restoration of work on
				the part of Intel*/
			if (codec == QSV_CODEC_AV1 && pParams->nLADepth > 0) {
				blog(LOG_INFO, "\tAdaptiveLTR set: AUTO");
			} else {
				switch ((int)pParams->bAdaptiveLTR) {
				case 0:
					mfx_Ext_CO3.AdaptiveLTR =
						MFX_CODINGOPTION_OFF;
					blog(LOG_INFO,
					     "\tAdaptiveLTR set: OFF");
					break;
				case 1:
					mfx_Ext_CO3.AdaptiveLTR =
						MFX_CODINGOPTION_ON;
					if (codec == QSV_CODEC_AVC)
						mfx_Ext_CO3.ExtBrcAdaptiveLTR =
							MFX_CODINGOPTION_ON;
					blog(LOG_INFO, "\tAdaptiveLTR set: ON");
					break;
				default:
					blog(LOG_INFO,
					     "\tAdaptiveLTR set: AUTO");
					break;
				}
			}
		}

		if (pParams->nWinBRCMaxAvgSize > 0) {
			mfx_Ext_CO3.WinBRCMaxAvgKbps =
				pParams->nWinBRCMaxAvgSize;
			blog(LOG_INFO, "\tWinBRCMaxSize set: %d",
			     pParams->nWinBRCMaxAvgSize);
		}

		if (pParams->nWinBRCSize > 0) {
			mfx_Ext_CO3.WinBRCSize = (mfxU16)pParams->nWinBRCSize;
			blog(LOG_INFO, "\tWinBRCSize set: %d",
			     mfx_Ext_CO3.WinBRCSize);
		}

		if ((int)pParams->nMotionVectorsOverPicBoundaries >= -1) {
			switch ((int)pParams->nMotionVectorsOverPicBoundaries) {
			case 0:
				mfx_Ext_CO3.MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_OFF;
				blog(LOG_INFO,
				     "\tMotionVectorsOverPicBoundaries set: OFF");
				break;
			case 1:
				mfx_Ext_CO3.MotionVectorsOverPicBoundaries =
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
			mfx_Ext_CO3.WeightedPred = MFX_WEIGHTED_PRED_IMPLICIT;
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
			mfx_Ext_CO3.WeightedBiPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO, "\tWeightedBiPred set: ON");
			break;
		default:
			blog(LOG_INFO, "\tWeightedBiPred set: AUTO");
			break;
		}

		if (pParams->bGlobalMotionBiasAdjustment == 1) {
			mfx_Ext_CO3.GlobalMotionBiasAdjustment =
				MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tGlobalMotionBiasAdjustment set: ON");

			switch (pParams->nMVCostScalingFactor) {
			case 1:
				mfx_Ext_CO3.MVCostScalingFactor = (mfxU16)1;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/2");
				break;
			case 2:
				mfx_Ext_CO3.MVCostScalingFactor = (mfxU16)2;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/4");
				break;
			case 3:
				mfx_Ext_CO3.MVCostScalingFactor = (mfxU16)3;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/8");
				break;
			default:
				mfx_Ext_CO3.MVCostScalingFactor = (mfxU16)56;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: DEFAULT");
				break;
			}
		} else if (pParams->bGlobalMotionBiasAdjustment == 0) {
			mfx_Ext_CO3.GlobalMotionBiasAdjustment =
				MFX_CODINGOPTION_OFF;
		}

		switch ((int)pParams->bDirectBiasAdjustment) {
		case 0:
			mfx_Ext_CO3.DirectBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: OFF");
			break;
		case 1:
			mfx_Ext_CO3.DirectBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: ON");
			break;
		default:
			blog(LOG_INFO, "\tDirectBiasAdjustment set: AUTO");
			break;
		}

		switch ((int)pParams->bCPUEncTools) {
		case 0:
			mfx_Ext_CO3.CPUEncToolsProcessing =
				MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tCPUEncTools set: OFF");
			break;
		case 1:
			mfx_Ext_CO3.CPUEncToolsProcessing = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tCPUEncTools set: ON");
			break;
		default:
			mfx_Ext_CO3.CPUEncToolsProcessing = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tCPUEncTools set: AUTO (ON)");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO3);
	}

	//if (codec != QSV_CODEC_VP9 && (((mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
	//      pParams->bCPUEncTools == 1 && pParams->nLADepth < 1) ||
	//    ((mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
	//     mfx_ENC_Params.mfx.TargetUsage == MFX_TARGETUSAGE_1 &&
	//     (mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
	//      mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_VBR) &&
	//     pParams->nLADepth < 1 && pParams->bCPUEncTools != 0))) {
	//	INIT_MFX_EXT_BUFFER(mfx_EncToolsConf,
	//			    MFX_EXTBUFF_ENCTOOLS_CONFIG);

	//	mfx_ENC_ExtendedBuffers.push_back(
	//		(mfxExtBuffer *)&mfx_EncToolsConf);
	//}

	/*Don't touch it! Magic beyond the control of mere mortals takes place here*/
	if (mfx_Ext_CO_DDI_enable == 1 && codec != QSV_CODEC_AV1) {

		INIT_MFX_EXT_BUFFER(mfx_Ext_CO_DDI, MFX_EXTBUFF_DDI);
		mfx_Ext_CO_DDI.IBC = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.BRCPrecision = 3;
		mfx_Ext_CO_DDI.BiDirSearch = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.GlobalSearch = 2;
		mfx_Ext_CO_DDI.IntraPredCostType = 8;
		mfx_Ext_CO_DDI.MEFractionalSearchType = 16;
		mfx_Ext_CO_DDI.MVPrediction = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.WeightedBiPredIdc =
			pParams->bWeightedBiPred == 1 ? 2 : 0;
		mfx_Ext_CO_DDI.WeightedPrediction =
			pParams->bWeightedPred == 1 ? MFX_CODINGOPTION_ON
						    : MFX_CODINGOPTION_OFF;
		mfx_Ext_CO_DDI.FieldPrediction = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.CabacInitIdcPlus1 = 1;
		mfx_Ext_CO_DDI.DirectCheck = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.FractionalQP = 1;
		mfx_Ext_CO_DDI.Hme = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.LocalSearch = 5;
		mfx_Ext_CO_DDI.MBAFF = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.DDI.InterPredBlockSize = 64;
		mfx_Ext_CO_DDI.DDI.IntraPredBlockSize = 1;
		mfx_Ext_CO_DDI.RefOppositeField = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.RefRaw = pParams->bRawRef == 1
						? MFX_CODINGOPTION_ON
						: MFX_CODINGOPTION_OFF;
		mfx_Ext_CO_DDI.TMVP = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
		mfx_Ext_CO_DDI.DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
		mfx_Ext_CO_DDI.QpAdjust = MFX_CODINGOPTION_ON;
		mfx_Ext_CO_DDI.Transform8x8Mode = MFX_CODINGOPTION_ON;
		//if (codec == QSV_CODEC_AVC) {
		//	if (pParams->bCPUEncTools == 1 &&
		//	    pParams->nLADepth >= 1) {
		//		/*Encoder default value*/
		//	} else {
		//		if (pParams->nTargetUsage ==
		//			    MFX_TARGETUSAGE_1 &&
		//		    pParams->nGOPRefDist > 4) {
		//			mfx_Ext_CO_DDI.NumActiveRefP = (mfxU16)
		//				AVCGetMaxNumRefActivePL0(
		//					pParams->nTargetUsage,
		//					mfx_ENC_Params.mfx
		//						.LowPower,
		//					mfx_ENC_Params.mfx
		//						.FrameInfo);
		//			mfx_Ext_CO_DDI.NumActiveRefBL0 =
		//				(mfxU16)AVCGetMaxNumRefActiveBL0(
		//					pParams->nTargetUsage,
		//					mfx_ENC_Params.mfx
		//						.LowPower);
		//			mfx_Ext_CO_DDI.NumActiveRefBL1 =
		//				(mfxU16)AVCGetMaxNumRefActiveBL1(
		//					pParams->nTargetUsage,
		//					mfx_ENC_Params.mfx
		//						.LowPower,
		//					mfx_ENC_Params.mfx
		//						.FrameInfo);
		//		}
		//	}
		//}
		if (codec == QSV_CODEC_HEVC) {
			mfx_Ext_CO_DDI.NumActiveRefP =
				(mfxU16)HEVCGetMaxNumRefActivePL0(
					pParams->nTargetUsage,
					mfx_ENC_Params.mfx.LowPower,
					mfx_ENC_Params.mfx.FrameInfo);
			mfx_Ext_CO_DDI.NumActiveRefBL0 =
				(mfxU16)HEVCGetMaxNumRefActiveBL0(
					pParams->nTargetUsage,
					mfx_ENC_Params.mfx.LowPower);
			mfx_Ext_CO_DDI.NumActiveRefBL1 =
				(mfxU16)HEVCGetMaxNumRefActiveBL1(
					pParams->nTargetUsage,
					mfx_ENC_Params.mfx.LowPower,
					mfx_ENC_Params.mfx.FrameInfo);
		} else if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) {
			mfx_Ext_CO_DDI.NumActiveRefP = pParams->nNumRefFrame;
			mfx_Ext_CO_DDI.NumActiveRefBL0 = pParams->nNumRefFrame;
			mfx_Ext_CO_DDI.NumActiveRefBL1 = pParams->nNumRefFrame;
		}
		/*You can touch it, this is the LookAHead setting,
			here you can adjust its strength,
			range of quality and dependence.*/
		if (pParams->nLADepth > 39) {
			/*mfx_Ext_CO_DDI.StrengthN = 1000;*/
			mfx_Ext_CO_DDI.QpUpdateRange = 30;
			mfx_Ext_CO_DDI.LaScaleFactor = 1;
			mfx_Ext_CO_DDI.LookAheadDependency = 70;
		}

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_CO_DDI);
	}

	/*This is waiting for a driver with support*/
	//if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9 &&
	//    pParams->nTuneQualityMode != 0) {
	//	INIT_MFX_EXT_BUFFER(mfx_Ext_TuneQuality,
	//			    MFX_EXTBUFF_TUNE_ENCODE_QUALITY);

	//	switch ((int)pParams->nTuneQualityMode) {
	//	case 0:
	//		blog(LOG_INFO, "\tTuneQualityMode set: OFF");
	//		break;
	//	case 1:
	//		mfx_Ext_TuneQuality.TuneQuality = MFX_ENCODE_TUNE_PSNR;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PSNR");
	//		break;
	//	case 2:
	//		mfx_Ext_TuneQuality.TuneQuality = MFX_ENCODE_TUNE_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: SSIM");
	//		break;
	//	case 3:
	//		mfx_Ext_TuneQuality.TuneQuality =
	//			MFX_ENCODE_TUNE_MS_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: MS SSIM");
	//		break;
	//	case 4:
	//		mfx_Ext_TuneQuality.TuneQuality = MFX_ENCODE_TUNE_VMAF;
	//		blog(LOG_INFO, "\tTuneQualityMode set: VMAF");
	//		break;
	//	case 5:
	//		mfx_Ext_TuneQuality.TuneQuality =
	//			MFX_ENCODE_TUNE_PERCEPTUAL;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PERCEPTUAL");
	//		break;
	//	default:
	//		mfx_Ext_TuneQuality.TuneQuality =
	//			MFX_ENCODE_TUNE_DEFAULT;
	//		blog(LOG_INFO, "\tTuneQualityMode set: DEFAULT");
	//		break;
	//	}

	//	mfx_ENC_ExtendedBuffers.push_back(
	//		(mfxExtBuffer *)&mfx_Ext_TuneQuality);
	//}

	if (codec == QSV_CODEC_HEVC) {
		if ((pParams->nWidth & 15) || (pParams->nHeight & 15)) {
			INIT_MFX_EXT_BUFFER(mfx_Ext_HEVCParam,
					    MFX_EXTBUFF_HEVC_PARAM);

			mfx_Ext_HEVCParam.PicWidthInLumaSamples =
				mfx_ENC_Params.mfx.FrameInfo.Width;
			mfx_Ext_HEVCParam.PicHeightInLumaSamples =
				mfx_ENC_Params.mfx.FrameInfo.Height;
			/*mfx_Ext_HEVCParam.LCUSize = pParams->nCTU;*/
			switch ((int)pParams->nSAO) {
			case 0:
				mfx_Ext_HEVCParam.SampleAdaptiveOffset =
					MFX_SAO_DISABLE;
				blog(LOG_INFO, "\tSAO set: DISABLE");
				break;
			case 1:
				mfx_Ext_HEVCParam.SampleAdaptiveOffset =
					MFX_SAO_ENABLE_LUMA;
				blog(LOG_INFO, "\tSAO set: LUMA");
				break;
			case 2:
				mfx_Ext_HEVCParam.SampleAdaptiveOffset =
					MFX_SAO_ENABLE_CHROMA;
				blog(LOG_INFO, "\tSAO set: CHROMA");
				break;
			case 3:
				mfx_Ext_HEVCParam.SampleAdaptiveOffset =
					MFX_SAO_ENABLE_LUMA |
					MFX_SAO_ENABLE_CHROMA;
				blog(LOG_INFO, "\tSAO set: ALL");
				break;
			default:
				blog(LOG_INFO,
				     "\tDirectBiasAdjustment set: AUTO");
				break;
			}

			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_HEVCParam);
		}
	}

	////INIT_MFX_EXT_BUFFER(mfx_VPPPercEnc, MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER);
	////mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPPercEnc);

	if (codec == QSV_CODEC_AVC &&
	    (mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
	     mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
	     mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_CQP) &&
	    pParams->nDenoiseMode > -1) {
		INIT_MFX_EXT_BUFFER(mfx_VPPDenoise, MFX_EXTBUFF_VPP_DENOISE2);

		switch ((int)pParams->nDenoiseMode) {
		case 1:
			mfx_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | BDRATE | PRE ENCODE");
			break;
		case 2:
			mfx_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | ADJUST | POST ENCODE");
			break;
		case 3:
			mfx_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
			break;
		case 4:
			mfx_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
			mfx_VPPDenoise.Strength =
				(mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO,
			     "\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
			     mfx_VPPDenoise.Strength);
			break;
		case 5:
			mfx_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
			mfx_VPPDenoise.Strength =
				(mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO,
			     "\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
			     mfx_VPPDenoise.Strength);
			break;
		default:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_DEFAULT;
			blog(LOG_INFO, "\tDenoise set: DEFAULT");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_VPPDenoise);
	}

	if (codec == QSV_CODEC_AV1) {
		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 5) {
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1BitstreamParam,
					    MFX_EXTBUFF_AV1_BITSTREAM_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1TileParam,
					    MFX_EXTBUFF_AV1_TILE_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1AuxData,
					    MFX_EXTBUFF_AV1_AUXDATA);
			mfx_Ext_AV1BitstreamParam.WriteIVFHeaders =
				MFX_CODINGOPTION_OFF;

			mfx_Ext_AV1TileParam.NumTileGroups = (mfxU16)1;
			mfx_Ext_AV1TileParam.NumTileColumns = (mfxU16)1;
			mfx_Ext_AV1TileParam.NumTileRows = (mfxU16)1;

			mfx_Ext_AV1AuxData.EnableCdef = MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.Cdef.CdefBits = 3;
			mfx_Ext_AV1AuxData.DisableFrameEndUpdateCdf =
				MFX_CODINGOPTION_OFF;
			mfx_Ext_AV1AuxData.DisableCdfUpdate =
				MFX_CODINGOPTION_OFF;

			/*mfx_AV1AuxData.EnableSuperres = MFX_CODINGOPTION_ON;*/

			mfx_Ext_AV1AuxData.InterpFilter = MFX_CODINGOPTION_ON;

			mfx_Ext_AV1AuxData.UniformTileSpacing =
				MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.SwitchInterval = 0;

			mfx_Ext_AV1AuxData.EnableLoopFilter =
				MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.LoopFilter.ModeRefDeltaEnabled = 1;
			mfx_Ext_AV1AuxData.LoopFilter.ModeRefDeltaUpdate = 1;
			mfx_Ext_AV1AuxData.LoopFilterSharpness = 2;

			mfx_Ext_AV1AuxData.EnableOrderHint =
				MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.OrderHintBits = 8;

			mfx_Ext_AV1AuxData.Palette = MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.SegmentTemporalUpdate =
				MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.IBC = MFX_CODINGOPTION_ON;
			mfx_Ext_AV1AuxData.PackOBUFrame = MFX_CODINGOPTION_ON;

			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1AuxData);
			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1BitstreamParam);
			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1TileParam);
		}
	}

	if (codec == QSV_CODEC_VP9) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_VP9Param, MFX_EXTBUFF_VP9_PARAM);
		mfx_Ext_VP9Param.WriteIVFHeaders = MFX_CODINGOPTION_OFF;
		mfx_Ext_VP9Param.NumTileColumns = 1;
		mfx_Ext_VP9Param.NumTileRows = 1;
		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_VP9Param);
	} else {
		INIT_MFX_EXT_BUFFER(mfx_Ext_VideoSignalInfo,
				    MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

		mfx_Ext_VideoSignalInfo.VideoFormat = pParams->VideoFormat;
		mfx_Ext_VideoSignalInfo.VideoFullRange =
			pParams->VideoFullRange;
		mfx_Ext_VideoSignalInfo.ColourDescriptionPresent = 1;
		mfx_Ext_VideoSignalInfo.ColourPrimaries =
			pParams->ColourPrimaries;
		mfx_Ext_VideoSignalInfo.TransferCharacteristics =
			pParams->TransferCharacteristics;
		mfx_Ext_VideoSignalInfo.MatrixCoefficients =
			pParams->MatrixCoefficients;

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_VideoSignalInfo);

		/*You can safely delete it,
		it's a useless parameter*/
		INIT_MFX_EXT_BUFFER(mfx_HyperModeParam,
				    MFX_EXTBUFF_HYPER_MODE_PARAM);
		mfx_HyperModeParam.Mode = MFX_HYPERMODE_ADAPTIVE;
		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_HyperModeParam);
	}

	if (codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_ChromaLocInfo,
				    MFX_EXTBUFF_CHROMA_LOC_INFO);

		mfx_Ext_ChromaLocInfo.ChromaLocInfoPresentFlag = 1;
		mfx_Ext_ChromaLocInfo.ChromaSampleLocTypeTopField =
			pParams->ChromaSampleLocTypeTopField;
		mfx_Ext_ChromaLocInfo.ChromaSampleLocTypeBottomField =
			pParams->ChromaSampleLocTypeBottomField;

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_ChromaLocInfo);
	}

	if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC ||
	     ((codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) &&
	      (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9))) &&
	    pParams->MaxContentLightLevel > 0) {
		INIT_MFX_EXT_BUFFER(
			mfx_Ext_MasteringDisplayColourVolume,
			MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME);
		INIT_MFX_EXT_BUFFER(mfx_Ext_ContentLightLevelInfo,
				    MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO);

		mfx_Ext_MasteringDisplayColourVolume.InsertPayloadToggle =
			MFX_PAYLOAD_IDR;
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesX[0] =
			pParams->DisplayPrimariesX[0];
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesX[1] =
			pParams->DisplayPrimariesX[1];
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesX[2] =
			pParams->DisplayPrimariesX[2];
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesY[0] =
			pParams->DisplayPrimariesY[0];
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesY[1] =
			pParams->DisplayPrimariesY[1];
		mfx_Ext_MasteringDisplayColourVolume.DisplayPrimariesY[2] =
			pParams->DisplayPrimariesY[2];
		mfx_Ext_MasteringDisplayColourVolume.WhitePointX =
			pParams->WhitePointX;
		mfx_Ext_MasteringDisplayColourVolume.WhitePointY =
			pParams->WhitePointY;
		mfx_Ext_MasteringDisplayColourVolume
			.MaxDisplayMasteringLuminance =
			pParams->MaxDisplayMasteringLuminance;
		mfx_Ext_MasteringDisplayColourVolume
			.MinDisplayMasteringLuminance =
			pParams->MinDisplayMasteringLuminance;

		mfx_Ext_ContentLightLevelInfo.InsertPayloadToggle =
			MFX_PAYLOAD_IDR;
		mfx_Ext_ContentLightLevelInfo.MaxContentLightLevel =
			pParams->MaxContentLightLevel;
		mfx_Ext_ContentLightLevelInfo.MaxPicAverageLightLevel =
			pParams->MaxPicAverageLightLevel;

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_MasteringDisplayColourVolume);
		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_ContentLightLevelInfo);
	}

	mfx_ENC_Params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
		mfx_ENC_Params.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	}

	mfx_ENC_Params.ExtParam = mfx_ENC_ExtendedBuffers.data();
	mfx_ENC_Params.NumExtParam = (mfxU16)mfx_ENC_ExtendedBuffers.size();

	blog(LOG_INFO, "Feature extended buffer size: %d",
	     mfx_ENC_ExtendedBuffers.size());
	// We dont check what was valid or invalid here, just try changing lower power.
	// Ensure set values are not overwritten so in case it wasnt lower power we fail
	// during the parameter check.
	mfxVideoParam validParams = {0};
	memcpy(&validParams, &mfx_ENC_Params, sizeof(mfxVideoParam));
	mfxStatus sts = mfx_VideoENC->Query(&mfx_ENC_Params, &validParams);
	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR ||
	    sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM ||
	    sts == MFX_ERR_NULL_PTR) {
		if (mfx_ENC_Params.mfx.LowPower == MFX_CODINGOPTION_ON) {
			mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_OFF;
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
		}
	}
	blog(LOG_INFO, "ENC params status: %d", sts);

	return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(qsv_param_t *pParams)
{
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_CBR:
		mfx_ENC_Params.mfx.TargetKbps = pParams->nTargetBitRate;
		break;
	default:

		break;
	}

	return true;
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder()
{
	return mfx_VideoENC->Reset(&mfx_ENC_Params);
}

mfxStatus QSV_VPL_Encoder_Internal::AllocateSurfaces()
{
	// Query number of required surfaces for encoder
	mfxStatus sts = mfx_VideoENC->QueryIOSurf(&mfx_ENC_Params,
						  &mfx_FrameAllocRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_FrameAllocRequest.Type |= WILL_WRITE | MFX_MEMTYPE_FROM_VPPIN;

	// SNB hack. On some SNB, it seems to require more surfaces
	mfx_FrameAllocRequest.NumFrameSuggested +=
		mfx_ENC_Params.AsyncDepth + 100;

	// Allocate required surfaces
	sts = mfx_FrameAllocator.Alloc(mfx_FrameAllocator.pthis,
				       &mfx_FrameAllocRequest,
				       &mfx_FrameAllocResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_SurfNum = mfx_FrameAllocResponse.NumFrameActual;

	mfx_FrameSurf = new mfxFrameSurface1 *[mfx_SurfNum];
	MSDK_CHECK_POINTER(mfx_FrameSurf, MFX_ERR_MEMORY_ALLOC);

	for (int i = 0; i < mfx_SurfNum; i++) {
		mfx_FrameSurf[i] = new mfxFrameSurface1;
		memset(mfx_FrameSurf[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(mfx_FrameSurf[i]->Info),
		       &(mfx_ENC_Params.mfx.FrameInfo), sizeof(mfxFrameInfo));
		mfx_FrameSurf[i]->Data.MemId = mfx_FrameAllocResponse.mids[i];
	}
	blog(LOG_INFO, "\tSurface count:     %d", mfx_SurfNum);

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec)
{
	mfx_VideoENC->GetVideoParam(&mfx_PPSSPSVPS_Params);

	std::vector<mfxExtBuffer *> extendedBuffers;
	extendedBuffers.reserve(2);

	INIT_MFX_EXT_BUFFER(mfx_Ext_CO_SPSPPS,
			    MFX_EXTBUFF_CODING_OPTION_SPSPPS);

	mfx_Ext_CO_SPSPPS.SPSBuffer = SPS_Buffer;
	mfx_Ext_CO_SPSPPS.PPSBuffer = PPS_Buffer;
	mfx_Ext_CO_SPSPPS.SPSBufSize = 1024; //  m_nSPSBufferSize;
	mfx_Ext_CO_SPSPPS.PPSBufSize = 1024; //  m_nPPSBufferSize;

	if (codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO_VPS,
				    MFX_EXTBUFF_CODING_OPTION_VPS);

		mfx_Ext_CO_VPS.VPSBuffer = VPS_Buffer;
		mfx_Ext_CO_VPS.VPSBufSize = 1024;

		extendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO_VPS);
	}
	if (codec != QSV_CODEC_VP9) {
		extendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO_SPSPPS);
	}

	mfx_PPSSPSVPS_Params.ExtParam = extendedBuffers.data();
	mfx_PPSSPSVPS_Params.NumExtParam = (mfxU16)extendedBuffers.size();

	//blog(LOG_INFO, "Video params extended buffer size: %d",
	//     extendedBuffers.size());

	mfxStatus sts = mfx_VideoENC->GetVideoParam(&mfx_PPSSPSVPS_Params);
	blog(LOG_INFO, "\tGetVideoParam status:     %d", sts);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (codec == QSV_CODEC_HEVC) {
		VPS_BufSize = mfx_Ext_CO_VPS.VPSBufSize;
	}

	SPS_BufSize = mfx_Ext_CO_SPSPPS.SPSBufSize;
	PPS_BufSize = mfx_Ext_CO_SPSPPS.PPSBufSize;

	return sts;
}

void QSV_VPL_Encoder_Internal::GetSPSPPS(mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
					 mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf)
{
	*pSPSBuf = SPS_Buffer;
	*pPPSBuf = PPS_Buffer;
	*pnSPSBuf = SPS_BufSize;
	*pnPPSBuf = PPS_BufSize;
}

void QSV_VPL_Encoder_Internal::GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
					    mfxU8 **pPPSBuf, mfxU16 *pnVPSBuf,
					    mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf)
{
	*pVPSBuf = VPS_Buffer;
	*pnVPSBuf = VPS_BufSize;

	*pSPSBuf = SPS_Buffer;
	*pnSPSBuf = SPS_BufSize;

	*pPPSBuf = PPS_Buffer;
	*pnPPSBuf = PPS_BufSize;
}

mfxStatus QSV_VPL_Encoder_Internal::InitBitstream()
{
	int MaxLength =
		mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_CQP ||
				mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_ICQ ||
				mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_LA_ICQ
			? (mfx_PPSSPSVPS_Params.mfx.BufferSizeInKB * 1000 * 100)
		: mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_VBR ||
				mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_AVBR ||
				mfx_PPSSPSVPS_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_VCM
			? (mfx_PPSSPSVPS_Params.mfx.MaxKbps * 1000 * 100)
			: (mfx_PPSSPSVPS_Params.mfx.TargetKbps * 1000 * 100);
	MaxLength = max((mfx_ENC_Params.mfx.BufferSizeInKB * 1000 * 100),
			MaxLength);
	mfx_TaskPool = mfx_ENC_Params.AsyncDepth;
	n_FirstSyncTask = 0;

	t_TaskPool = new Task[mfx_TaskPool];
	memset(t_TaskPool, 0, sizeof(Task) * mfx_TaskPool);

	for (int i = 0; i < mfx_TaskPool; i++) {
		t_TaskPool[i].mfxBS.MaxLength = (mfxU32)MaxLength;
		t_TaskPool[i].mfxBS.Data =
			new mfxU8[t_TaskPool[i].mfxBS.MaxLength];
		t_TaskPool[i].mfxBS.DataOffset = (mfxU32)0;
		t_TaskPool[i].mfxBS.DataLength = (mfxU32)0;
		t_TaskPool[i].mfxBS.CodecId =
			(mfxU32)mfx_ENC_Params.mfx.CodecId;
		t_TaskPool[i].mfxBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME |
					       MFX_BITSTREAM_EOS;
		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS.Data,
				   MFX_ERR_MEMORY_ALLOC);
	}

	mfx_Bitstream.MaxLength = (mfxU32)MaxLength;
	mfx_Bitstream.Data = new mfxU8[mfx_Bitstream.MaxLength];
	mfx_Bitstream.DataOffset = (mfxU32)0;
	mfx_Bitstream.DataLength = (mfxU32)0;
	mfx_Bitstream.CodecId = (mfxU32)mfx_ENC_Params.mfx.CodecId;
	mfx_Bitstream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME |
				 MFX_BITSTREAM_EOS;
	blog(LOG_INFO, "\tTaskPool count:    %d", mfx_TaskPool);

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
	ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;
	const size_t line_size = w * 2;

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, line_size);

	// load UV plane
	h /= 2;
	ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;

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
	ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, w);

	// load UV plane
	h /= 2;
	ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;

	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataUV + i * strideUV, w);

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadBGRA(mfxFrameSurface1 *pSurface,
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
	//ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

	//// load Y plane
	//for (i = 0; i < h; i++)
	//	memcpy(ptr + i * pitch, pDataY + i * strideY, w);

	ptr = pData->B + pInfo->CropX + pInfo->CropY * pData->Pitch;

	for (i = 0; i < h; i++) {
		memcpy(ptr + i * pitch, pDataY + i * strideY, pitch);
	}

	return MFX_ERR_NONE;
}

int QSV_VPL_Encoder_Internal::GetFreeTaskIndex(Task *t_TaskPool,
					       mfxU16 nPoolSize)
{
	if (t_TaskPool) {
		for (int i = 0; i < nPoolSize; i++) {
			if (!t_TaskPool[i].syncp) {
				return i;
			}
		}
	}
	return MFX_ERR_NOT_FOUND;
}

int QSV_VPL_Encoder_Internal::GetFreeSurfaceIndex(
	mfxFrameSurface1 **pSurfacesPool, mfxU16 nPoolSize)
{
	if (pSurfacesPool) {
		for (mfxU16 i = 0; i < nPoolSize; i++) {
			if (0 == pSurfacesPool[i]->Data.Locked) {
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
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mfx_TaskPool);

	int nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mfx_SurfNum);

	while (MFX_ERR_NOT_FOUND == nTaskIdx || MFX_ERR_NOT_FOUND == nSurfIdx) {
		// No more free tasks or surfaces, need to sync
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp,
			1500000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mfx_TaskPool;
		*pBS = &mfx_Bitstream;

		nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mfx_SurfNum);
	}

	mfxFrameSurface1 *pSurface = mfx_FrameSurf[nSurfIdx];
	sts = mfx_FrameAllocator.Lock(mfx_FrameAllocator.pthis,
				      pSurface->Data.MemId, &(pSurface->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = (pSurface->Info.FourCC == MFX_FOURCC_P010)
		      ? LoadP010(pSurface, pDataY, pDataUV, strideY, strideUV)
	      : (pSurface->Info.FourCC == MFX_FOURCC_NV12)
		      ? LoadNV12(pSurface, pDataY, pDataUV, strideY, strideUV)
		      : LoadBGRA(pSurface, pDataY, pDataUV, strideY, strideUV);

	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	pSurface->Data.TimeStamp = ts;

	sts = mfx_FrameAllocator.Unlock(mfx_FrameAllocator.pthis,
					pSurface->Data.MemId,
					&(pSurface->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	for (;;) {
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoENC->EncodeFrameAsync(
			NULL, pSurface, &t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts) {
				MSDK_SLEEP(100);
			}
		} else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		} else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
			// Allocate more bitstream buffer memory here if needed...
			break;
		} else {
			break;
		}
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode_tex(uint64_t ts, uint32_t tex_handle,
					       uint64_t lock_key,
					       uint64_t *next_key,
					       mfxBitstream **pBS)
{
	mfxStatus sts = MFX_ERR_NONE;
	*pBS = nullptr;
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mfx_TaskPool);
	int nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mfx_SurfNum);

	while (MFX_ERR_NOT_FOUND == nTaskIdx || MFX_ERR_NOT_FOUND == nSurfIdx) {
		// No more free tasks or surfaces, need to sync
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp,
			1500000);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mfx_TaskPool;
		*pBS = &mfx_Bitstream;

		nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mfx_SurfNum);
	}

	mfxFrameSurface1 *pSurface = mfx_FrameSurf[nSurfIdx];
	//copy to default surface directly
	pSurface->Data.TimeStamp = ts;
	sts = simple_copytex(mfx_FrameAllocator.pthis, pSurface->Data.MemId,
			     tex_handle, lock_key, next_key);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	for (;;) {
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoENC->EncodeFrameAsync(
			NULL, pSurface, &t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(
					100); // Wait if device is busy, then repeat the same call
		} else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		} else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
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
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp,
			15000000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mfx_TaskPool;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::ClearData()
{
	mfxStatus sts = MFX_ERR_NONE;
	sts = Drain();

	if (mfx_VideoENC) {
		sts = mfx_VideoENC->Close();
		delete mfx_VideoENC;
		mfx_VideoENC = nullptr;
	}

	mfx_FrameAllocator.Free(mfx_FrameAllocator.pthis,
				&mfx_FrameAllocResponse);

	if (mfx_FrameSurf) {
		for (int i = 0; i < mfx_SurfNum; i++) {
			delete mfx_FrameSurf[i]->Data.Y;
			delete mfx_FrameSurf[i];
		}
		MSDK_SAFE_DELETE_ARRAY(mfx_FrameSurf);
	}

	if (t_TaskPool) {
		for (int i = 0; i < mfx_TaskPool; i++)
			delete t_TaskPool[i].mfxBS.Data;
		MSDK_SAFE_DELETE_ARRAY(t_TaskPool);
	}

	if (mfx_Bitstream.Data) {
		delete[] mfx_Bitstream.Data;
		mfx_Bitstream.Data = nullptr;
	}

	if (sts >= MFX_ERR_NONE) {
		mfx_OpenEncodersNum--;
	}

	if (mfx_OpenEncodersNum <= 0) {
		Release();
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
		fourCC = mfx_ENC_Params.mfx.FrameInfo.FourCC;
		return MFX_ERR_NONE;
	} else {
		return MFX_ERR_NOT_INITIALIZED;
	}
}
