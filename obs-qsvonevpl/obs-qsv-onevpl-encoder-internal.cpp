#define MFX_DEPRECATED_OFF
#define ONEVPL_EXPERIMENTAL
#define MFX_ENABLE_ENCTOOLS

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
mfxHDL QSV_VPL_Encoder_Internal::g_DX_Handle = nullptr;

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxIMPL &impl,
						   mfxVersion &version,
						   bool isDGPU)
	: mfx_FrameSurf(nullptr),
	  mfx_VideoEnc(nullptr),
	  mU16_SPSBufSize(1024),
	  mU16_PPSBufSize(1024),
	  mU16_VPSBufSize(),
	  mU16_TaskPool(0),
	  g_numEncodersOpen(0),
	  t_TaskPool(nullptr),
	  n_TaskIdx(0),
	  n_FirstSyncTask(0),
	  mU16_SurfNum(),
	  mfx_Bitstream(),
	  b_isDGPU(isDGPU),
	  mfx_co(),
	  mfx_co2(),
	  mfx_co3(),
	  mfx_ExtMVOverPicBoundaries(),
	  mfx_ExtendedBuffers(),
	  mfx_CtrlExtendedBuffers(),
	  mfx_EncParams(),
	  mfx_EncCtrl(),
	  mfx_BRCFrameCtrl(),
	  mfx_VPPDenoise(),
	  mfx_AVCRoundingOffset(),
	  mfx_Tune(),
	  mfx_FrameAllocator(),
	  mfx_FrameAllocResponse(),
	  mfx_EncToolsConf(),
	  mfx_ExtAV1BitstreamParam(),
	  mfx_ExtAV1ResolutionParam(),
	  mfx_ExtAV1TileParam(),
	  mfx_ExtHEVCParam{},
	  mfx_ExtVideoSignalInfo{},
	  mfx_ExtChromaLocInfo{},
	  mfx_ExtMasteringDisplayColourVolume{},
	  mfx_ExtContentLightLevelInfo{},
	  mfx_ExtCOVPS{},
	  mfx_ExtCOSPSPPS(),
	  mfx_FrameAllocRequest(),
	  mfx_ResetOption(),
	  mfx_VPPDoUse()
{
	blog(LOG_INFO, "\tSelected QuickSync oneVPL implementation\n");
	mfxIMPL tempImpl = MFX_IMPL_VIA_D3D11;
	mfx_loader = MFXLoad();
	mfxStatus sts = MFXCreateSession(mfx_loader, 0, &mfx_session);
	if (sts == MFX_ERR_NONE) {
		sts = MFXQueryVersion(mfx_session, &version);
		if (sts == MFX_ERR_NONE) {
			mfx_version = version;
			sts = MFXQueryIMPL(mfx_session, &tempImpl);
			if (sts == MFX_ERR_NONE) {
				mfx_impl = MFX_IMPL_VIA_D3D11;
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
	MFXClose(mfx_session);
	MFXUnload(mfx_loader);
	return;
}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal()
{
	if (mfx_VideoEnc) {
		ClearData();
	}
}

mfxStatus
QSV_VPL_Encoder_Internal::Initialize(mfxIMPL impl, mfxVersion ver,
				     mfxSession *pSession,
				     mfxFrameAllocator *mfx_FrameAllocator,
				     mfxHDL *deviceHandle, int deviceNum)
{
	mfx_FrameAllocator; // (Hugh) Currently unused
	mfxConfig mfx_cfg[10] = {};
	mfxVariant mfx_variant[10] = {};
	mfxStatus sts = MFX_ERR_NONE;

	// If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
	if (mfx_FrameAllocator) {
		// Initialize VPL Session
		mfx_loader = MFXLoad();
		mfx_cfg[0] = MFXCreateConfig(mfx_loader);
		mfx_variant[0].Type = MFX_VARIANT_TYPE_U32;
		mfx_variant[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
		MFXSetConfigFilterProperty(
			mfx_cfg[0],
			(mfxU8 *)"mfxImplDescription.Impl.mfxImplType",
			mfx_variant[0]);

		mfx_cfg[1] = MFXCreateConfig(mfx_loader);
		mfx_variant[1].Type = MFX_VARIANT_TYPE_U32;
		mfx_variant[1].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
		MFXSetConfigFilterProperty(
			mfx_cfg[1],
			(mfxU8 *)"mfxImplDescription.mfxAccelerationModeDescription.Mode",
			mfx_variant[1]);

		mfx_cfg[2] = MFXCreateConfig(mfx_loader);
		mfx_variant[2].Type = MFX_VARIANT_TYPE_U16;
		mfx_variant[2].Data.U16 = MFX_GPUCOPY_ON;
		MFXSetConfigFilterProperty(
			mfx_cfg[2],
			(mfxU8 *)"mfxInitializationParam.DeviceCopy",
			mfx_variant[2]);

		mfxStatus sts = MFXCreateSession(
			mfx_loader, (deviceNum >= 0) ? deviceNum : 0,
			&mfx_session);

		MFXQueryIMPL(mfx_session, &mfx_impl);

		MFXSetPriority(mfx_session, MFX_PRIORITY_HIGH);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		// Create DirectX device context
		//if (g_DX_Handle == NULL) {
		//	sts = CreateHWDevice(mfx_session, &g_DX_Handle);
		//	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		//}

		//if (g_DX_Handle == NULL)
		//	return MFX_ERR_DEVICE_FAILED;

		//// Provide device manager to VPL

		//sts = MFXVideoCORE_SetHandle(mfx_session, DEVICE_MGR_TYPE,
		//			     g_DX_Handle);
		//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		//mfx_FrameAllocator->pthis =
		//	mfx_session; // We use VPL session ID as the allocation identifier
		//mfx_FrameAllocator->Alloc = simple_alloc;
		//mfx_FrameAllocator->Free = simple_free;
		//mfx_FrameAllocator->Lock = simple_lock;
		//mfx_FrameAllocator->Unlock = simple_unlock;
		//mfx_FrameAllocator->GetHDL = simple_gethdl;

		//// Since we are using video memory we must provide VPL with an external allocator
		//sts = MFXVideoCORE_SetFrameAllocator(mfx_session,
		//				     mfx_FrameAllocator);
		//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Open(qsv_param_t *pParams,
					 enum qsv_codec codec)
{
	mfxStatus sts = Initialize(mfx_impl, mfx_version, &mfx_session,
				   &mfx_FrameAllocator, &g_DX_Handle,
				   pParams->nDeviceNum);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_VideoEnc = new MFXVideoENCODE(mfx_session);

	sts = InitEncParams(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitEncCtrlParams(pParams, codec);
	/*MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);*/

	sts = mfx_VideoEnc->Query(&mfx_EncParams, &mfx_EncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_VideoEnc->Init(&mfx_EncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = GetVideoParam(codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitBitstream();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (sts >= MFX_ERR_NONE) {
		g_numEncodersOpen++;
	} else {
		blog(LOG_INFO, "\tOpen encoder: [ERROR]");
		mfx_VideoEnc->Close();
		MFXClose(mfx_session);
		MFXUnload(mfx_loader);
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitEncCtrlParams(qsv_param_t *pParams,
						      enum qsv_codec codec)
{
	if (codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_AVCRoundingOffset,
				    MFX_EXTBUFF_AVC_ROUNDING_OFFSET);
		mfx_AVCRoundingOffset.EnableRoundingIntra = MFX_CODINGOPTION_ON;
		mfx_AVCRoundingOffset.RoundingOffsetIntra = 0;
		mfx_AVCRoundingOffset.EnableRoundingInter = MFX_CODINGOPTION_ON;
		mfx_AVCRoundingOffset.RoundingOffsetInter = 0;
		mfx_CtrlExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_AVCRoundingOffset);
	}

	mfx_EncCtrl.ExtParam = mfx_ExtendedBuffers.data();
	mfx_EncCtrl.NumExtParam = (mfxU16)mfx_CtrlExtendedBuffers.size();

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::InitEncParams(qsv_param_t *pParams,
						  enum qsv_codec codec)
{
	int mfx_co_enable = 1;
	int mfx_co2_enable = 1;
	int mfx_co3_enable = 1;
	int mfx_EncToolsConf_enable = 1;
	//mfx_EncParams.mfx.NumThread = 64;
	//mfx_EncParams.mfx.RestartInterval = 1;

	switch (codec) {
	case QSV_CODEC_AV1:
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AV1;
		break;
	case QSV_CODEC_HEVC:
		mfx_EncParams.mfx.CodecId = MFX_CODEC_HEVC;
		break;
	case QSV_CODEC_AVC:
	default:
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AVC;
		break;
	}

	if ((pParams->nNumRefFrame >= 0) && (pParams->nNumRefFrame < 16)) {
		mfx_EncParams.mfx.NumRefFrame = pParams->nNumRefFrame;
		blog(LOG_INFO, "\tNumRefFrame set to: %d",
		     pParams->nNumRefFrame);
	}

	mfx_EncParams.mfx.TargetUsage = pParams->nTargetUsage;
	mfx_EncParams.mfx.CodecProfile = pParams->CodecProfile;
	if (codec == QSV_CODEC_HEVC) {
		mfx_EncParams.mfx.CodecProfile |= pParams->HEVCTier;
	}
	mfx_EncParams.mfx.FrameInfo.FrameRateExtN = pParams->nFpsNum;
	mfx_EncParams.mfx.FrameInfo.FrameRateExtD = pParams->nFpsDen;

	mfx_EncParams.vpp.In.FrameRateExtN = pParams->nFpsNum;
	mfx_EncParams.vpp.In.FrameRateExtD = pParams->nFpsDen;

	if (pParams->video_fmt_10bit) {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
		mfx_EncParams.vpp.In.FourCC = MFX_FOURCC_P010;

		mfx_EncParams.mfx.FrameInfo.BitDepthChroma = 10;
		mfx_EncParams.vpp.In.BitDepthChroma = 10;

		mfx_EncParams.mfx.FrameInfo.BitDepthLuma = 10;
		mfx_EncParams.vpp.In.BitDepthLuma = 10;

		mfx_EncParams.mfx.FrameInfo.Shift = 1;
		mfx_EncParams.vpp.In.Shift = 1;

	} else {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
		mfx_EncParams.vpp.In.FourCC = MFX_FOURCC_NV12;

		mfx_EncParams.mfx.FrameInfo.Shift = 0;
		mfx_EncParams.vpp.In.Shift = 0;

		mfx_EncParams.mfx.FrameInfo.BitDepthChroma = 8;
		mfx_EncParams.vpp.In.BitDepthChroma = 8;

		mfx_EncParams.mfx.FrameInfo.BitDepthLuma = 8;
		mfx_EncParams.vpp.In.BitDepthLuma = 8;
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture

	mfx_EncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(pParams->nWidth);
	mfx_EncParams.vpp.In.Width = MSDK_ALIGN16(pParams->nWidth);

	mfx_EncParams.mfx.FrameInfo.Height = MSDK_ALIGN16(pParams->nHeight);
	mfx_EncParams.vpp.In.Height = MSDK_ALIGN16(pParams->nHeight);

	mfx_EncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_EncParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

	mfx_EncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_EncParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

	mfx_EncParams.mfx.FrameInfo.CropX = 0;
	mfx_EncParams.vpp.In.CropX = 0;

	mfx_EncParams.mfx.FrameInfo.CropY = 0;
	mfx_EncParams.vpp.In.CropY = 0;

	mfx_EncParams.mfx.FrameInfo.CropW = pParams->nWidth;
	mfx_EncParams.vpp.In.CropW = pParams->nWidth;

	mfx_EncParams.mfx.FrameInfo.CropH = pParams->nHeight;
	mfx_EncParams.vpp.In.CropH = pParams->nHeight;

	switch ((int)pParams->bLowPower) {
	case 0:
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
		blog(LOG_INFO, "\tLowpower set: OFF");
		break;
	case 1:
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
		break;
	default:
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
		blog(LOG_INFO, "\tLowpower set: AUTO");
		break;
	}

	enum qsv_cpu_platform qsv_platform = qsv_get_cpu_platform();
	if ((qsv_platform >= QSV_CPU_PLATFORM_ICL ||
	     qsv_platform == QSV_CPU_PLATFORM_UNKNOWN) &&
	    (pParams->nGOPRefDist == 1) &&
	    (mfx_version.Major >= 2 && mfx_version.Minor >= 0)) {
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
	}

	if (mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
		if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
		    pParams->RateControl == MFX_RATECONTROL_LA_HRD ||
		    pParams->RateControl == MFX_RATECONTROL_LA) {
			pParams->RateControl = MFX_RATECONTROL_VBR;
			blog(LOG_INFO, "\tRatecontrol set: VBR");
		}
	}

	if (pParams->RateControl == MFX_RATECONTROL_CBR &&
	    pParams->bExtBRC < 1 && pParams->bLAExtBRC == 1 &&
	    pParams->nLADepth > 0) {
		blog(LOG_INFO,
		     "\tRatecontrol LA_EXT_CBR work only with enabled ExtBRC");
	} else if (pParams->RateControl == MFX_RATECONTROL_VBR &&
		   pParams->bExtBRC < 1 && pParams->bLAExtBRC == 1 &&
		   pParams->nLADepth > 0) {
		blog(LOG_INFO,
		     "\tRatecontrol LA_EXT_VBR work only with enabled ExtBRC");
	} else if (pParams->RateControl == MFX_RATECONTROL_ICQ &&
		   pParams->bExtBRC < 1 && pParams->bLAExtBRC == 1 &&
		   pParams->nLADepth > 0) {
		blog(LOG_INFO,
		     "\tRatecontrol LA_EXT_ICQ work only with enabled ExtBRC");
	}

	mfx_EncParams.mfx.RateControlMethod = (mfxU16)pParams->RateControl;

	mfx_EncParams.mfx.BRCParamMultiplier = 1;
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_LA_HRD:
	case MFX_RATECONTROL_CBR:
		mfx_EncParams.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = mfx_EncParams.mfx.TargetKbps;
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_EncParams.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_EncParams.mfx.BufferSizeInKB);
		break;
	case MFX_RATECONTROL_VBR:
	case MFX_RATECONTROL_VCM:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.MaxKbps = pParams->nMaxBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = mfx_EncParams.mfx.MaxKbps;
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_EncParams.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_EncParams.mfx.BufferSizeInKB);
		break;
	case MFX_RATECONTROL_CQP:
		mfx_EncParams.mfx.QPI = (mfxU16)pParams->nQPI;
		mfx_EncParams.mfx.QPB = (mfxU16)pParams->nQPB;
		mfx_EncParams.mfx.QPP = (mfxU16)pParams->nQPP;
		break;
	case MFX_RATECONTROL_AVBR:
		mfx_EncParams.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_EncParams.mfx.Accuracy = (mfxU16)pParams->nAccuracy;
		mfx_EncParams.mfx.Convergence = (mfxU16)pParams->nConvergence;
		break;
	case MFX_RATECONTROL_ICQ:
	case MFX_RATECONTROL_LA_ICQ:
		mfx_EncParams.mfx.ICQQuality = (mfxU16)pParams->nICQQuality;
		break;
	case MFX_RATECONTROL_LA:
		mfx_EncParams.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = mfx_EncParams.mfx.TargetKbps;
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_EncParams.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_EncParams.mfx.BufferSizeInKB);
		break;
	}

	mfx_EncParams.AsyncDepth = (mfxU16)pParams->nAsyncDepth;

	if (pParams->nKeyIntSec == 0) {
		mfx_EncParams.mfx.GopPicSize =
			(mfxU16)((pParams->nFpsNum + pParams->nFpsDen - 1) /
				 pParams->nFpsDen) *
			10;
	} else {
		mfx_EncParams.mfx.GopPicSize =
			(mfxU16)(pParams->nKeyIntSec * pParams->nFpsNum /
				 (float)pParams->nFpsDen);
	}
	blog(LOG_INFO, "\tGopPicSize set to: %d", mfx_EncParams.mfx.GopPicSize);

	if (pParams->bGopOptFlag == 1) {
		mfx_EncParams.mfx.GopOptFlag = 0x00;
		blog(LOG_INFO, "\tGopOptFlag set: OPEN");
		if (mfx_EncParams.mfx.CodecId == MFX_CODEC_HEVC) {
			mfx_EncParams.mfx.IdrInterval =
				1 + ((pParams->nFpsNum + pParams->nFpsDen - 1) /
				     pParams->nFpsDen) *
					    600 / mfx_EncParams.mfx.GopPicSize;
			mfx_EncParams.mfx.NumSlice = 0;
		} else if (mfx_EncParams.mfx.CodecId == MFX_CODEC_AVC) {
			mfx_EncParams.mfx.IdrInterval =
				((pParams->nFpsNum + pParams->nFpsDen - 1) /
				 pParams->nFpsDen) *
				600 / mfx_EncParams.mfx.GopPicSize;
			mfx_EncParams.mfx.NumSlice = 1;
		} else {
			mfx_EncParams.mfx.IdrInterval = 0;
			mfx_EncParams.mfx.NumSlice = 1;
		}
	} else if (pParams->bGopOptFlag == 0) {
		mfx_EncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
		blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
		if (mfx_EncParams.mfx.CodecId == MFX_CODEC_HEVC) {
			mfx_EncParams.mfx.IdrInterval = 1;
			mfx_EncParams.mfx.NumSlice = 0;
		} else if (mfx_EncParams.mfx.CodecId == MFX_CODEC_AVC) {
			mfx_EncParams.mfx.IdrInterval = 0;
			mfx_EncParams.mfx.NumSlice = 1;
		} else {
			mfx_EncParams.mfx.IdrInterval = 0;
			mfx_EncParams.mfx.NumSlice = 1;
		}
	}

	if (codec == QSV_CODEC_AV1) {
		mfx_EncParams.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_EncParams.mfx.GopRefDist > 33) {
			mfx_EncParams.mfx.GopRefDist = 33;
		} else if (mfx_EncParams.mfx.GopRefDist > 16 &&
			   pParams->bExtBRC == 1) {
			mfx_EncParams.mfx.GopRefDist = 16;
			blog(LOG_INFO,
			     "\tExtBRC does not support GOPRefDist greater than 16.");
		}
		if (pParams->bExtBRC == 1 &&
		    (mfx_EncParams.mfx.GopRefDist != 1 &&
		     mfx_EncParams.mfx.GopRefDist != 2 &&
		     mfx_EncParams.mfx.GopRefDist != 4 &&
		     mfx_EncParams.mfx.GopRefDist != 8 &&
		     mfx_EncParams.mfx.GopRefDist != 16)) {
			mfx_EncParams.mfx.GopRefDist = 4;
			blog(LOG_INFO,
			     "\tExtBRC only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
		} else {
			blog(LOG_INFO, "\tGOPRefDist set to:     %d",
			     pParams->nGOPRefDist);
		}
	} else {
		mfx_EncParams.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_EncParams.mfx.GopRefDist > 16) {
			mfx_EncParams.mfx.GopRefDist = 16;
		}
		if (pParams->bExtBRC == 1 &&
		    (mfx_EncParams.mfx.GopRefDist != 1 &&
		     mfx_EncParams.mfx.GopRefDist != 2 &&
		     mfx_EncParams.mfx.GopRefDist != 4 &&
		     mfx_EncParams.mfx.GopRefDist != 8 &&
		     mfx_EncParams.mfx.GopRefDist != 16)) {
			mfx_EncParams.mfx.GopRefDist = 4;
			blog(LOG_INFO,
			     "\tExtBRC only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
		}
		blog(LOG_INFO, "\tGOPRefDist set to:     %d",
		     (int)mfx_EncParams.mfx.GopRefDist);
	}

	if (mfx_co_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co, MFX_EXTBUFF_CODING_OPTION);

		mfx_co.CAVLC = MFX_CODINGOPTION_OFF;
		mfx_co.ResetRefList = MFX_CODINGOPTION_ON;
		mfx_co.RefPicMarkRep = MFX_CODINGOPTION_ON;
		mfx_co.PicTimingSEI = MFX_CODINGOPTION_ON;
		mfx_co.FieldOutput = MFX_CODINGOPTION_ON;
		mfx_co.IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_co.InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_co.MVPrecision = MFX_MVPRECISION_QUARTERPEL;

		mfx_co.MaxDecFrameBuffering = pParams->nNumRefFrame;

		switch ((int)pParams->bRDO) {
		case 0:
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tRDO set: OFF");
			break;
		case 1:
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tRDO set: ON");
			break;
		default:
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tRDO set: AUTO");
			break;
		}

		switch ((int)pParams->bVuiNalHrd) {
		case 0:
			mfx_co.VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
			mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: OFF");
			break;
		case 1:
			mfx_co.VuiVclHrdParameters = MFX_CODINGOPTION_ON;
			mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: ON");
			break;
		default:
			mfx_co.VuiVclHrdParameters = MFX_CODINGOPTION_UNKNOWN;
			mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: AUTO");
			break;
		}

		switch ((int)pParams->bNalHrdConformance) {
		case 0:
			mfx_co.NalHrdConformance = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tNalHrdConformance set: OFF");
			break;
		case 1:
			mfx_co.NalHrdConformance = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tNalHrdConformance set: ON");
			break;
		default:
			mfx_co.NalHrdConformance = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tNalHrdConformance set: AUTO");
			break;
		}

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_co);
	}

	if (mfx_co2_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co2, MFX_EXTBUFF_CODING_OPTION2);

		
		/*mfx_co2.BufferingPeriodSEI = MFX_BPSEI_IFRAME;*/
		mfx_co2.FixedFrameRate = MFX_CODINGOPTION_ON;
		/*mfx_co2.EnableMAD = MFX_CODINGOPTION_ON;*/

		if (mfx_version.Major == 2 && mfx_version.Minor <= 8) {
			mfx_co2.BitrateLimit = MFX_CODINGOPTION_OFF;
		}
		if (mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
			if (codec == QSV_CODEC_AVC) {
				mfx_co2.RepeatPPS = MFX_CODINGOPTION_OFF;
			}
		}

		if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_HEVC) {
			mfx_co2.RepeatPPS = MFX_CODINGOPTION_ON;
		}

		//mfx_co2.IntRefType = /*MFX_REFRESH_NO */ MFX_REFRESH_VERTICAL;

		if (pParams->bExtBRC == 1 && pParams->bLAExtBRC == 1) {
			mfx_co2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:     %d",
			     pParams->nLADepth);
		}

		if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
		    pParams->RateControl == MFX_RATECONTROL_LA ||
		    pParams->RateControl == MFX_RATECONTROL_LA_HRD ||
		    (pParams->bLowPower == 1 ||
		     mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON)) {
			mfx_co2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:%d",
			     pParams->nLADepth);
		}

		if (pParams->bMBBRC == 1) {
			if ((pParams->RateControl == MFX_RATECONTROL_ICQ) ||
			    (pParams->RateControl == MFX_RATECONTROL_CQP) ||
			    (pParams->RateControl == MFX_RATECONTROL_CBR)) {
				mfx_co2.MBBRC = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tMBBRC set: ON");
			}
		} else if (pParams->bMBBRC == 0) {
			mfx_co2.MBBRC = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tMBBRC set: OFF");
		}

		switch ((int)pParams->bDeblockingIdc) {
		case 0:
			mfx_co2.DisableDeblockingIdc = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDeblockingIdc set: OFF");
			break;
		case 1:
			mfx_co2.DisableDeblockingIdc = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tDeblockingIdc set: ON");
			break;
		default:
			mfx_co2.DisableDeblockingIdc = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tDeblockingIdc set: AUTO");
			break;
		}

		switch ((int)pParams->bBPyramid) {
		case 0:
			mfx_co2.BRefType = MFX_B_REF_OFF;
			blog(LOG_INFO, "\tBPyramid set: OFF");
			break;
		case 1:
			mfx_co2.BRefType = MFX_B_REF_PYRAMID;
			blog(LOG_INFO, "\tBPyramid set: ON");
			break;
		default:
			if (pParams->nGOPRefDist >= 2) {
				mfx_co2.BRefType = MFX_B_REF_PYRAMID;
				blog(LOG_INFO, "\tBPyramid set: ON");
			} else {
				mfx_co2.BRefType = MFX_B_REF_UNKNOWN;
				blog(LOG_INFO, "\tBPyramid set: AUTO");
			}
			break;
		}

		switch ((int)pParams->nTrellis) {
		case 0:
			mfx_co2.Trellis = MFX_TRELLIS_OFF;
			blog(LOG_INFO, "\tTrellis set: OFF");
			break;
		case 1:
			mfx_co2.Trellis = MFX_TRELLIS_I;
			blog(LOG_INFO, "\tTrellis set: I");
			break;
		case 2:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: IP");
			break;
		case 3:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P |
					  MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IPB");
			break;
		case 4:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: IB");
			break;
		case 5:
			mfx_co2.Trellis = MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set: P");
			break;
		case 6:
			mfx_co2.Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: PB");
			break;
		case 7:
			mfx_co2.Trellis = MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set: B");
			break;
		default:
			mfx_co2.Trellis = MFX_TRELLIS_UNKNOWN;
			blog(LOG_INFO, "\tTrellis set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveB) {
		case 0:
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveI set: OFF");
			break;
		case 1:
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveI set: ON");
			break;
		default:
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tAdaptiveI set: AUTO");
			break;
		}

		switch ((int)pParams->bAdaptiveB) {
		case 0:
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveB set: OFF");
			break;
		case 1:
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveB set: ON");
			break;
		default:
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tAdaptiveB set: AUTO");
			break;
		}

		if ((pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
		     pParams->RateControl == MFX_RATECONTROL_LA ||
		     pParams->RateControl == MFX_RATECONTROL_LA_HRD) ||
		    ((pParams->bExtBRC == 1 && pParams->bLAExtBRC == 1) &&
		     (pParams->RateControl == MFX_RATECONTROL_CBR ||
		      pParams->RateControl == MFX_RATECONTROL_VBR))) {
			switch ((int)pParams->nLookAheadDS) {
			case 0:
				mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
				blog(LOG_INFO, "\tLookAheadDS set: SLOW");
				break;
			case 1:
				mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_2x;
				blog(LOG_INFO, "\tLookAheadDS set: MEDIUM");
				break;
			case 2:
				mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_4x;
				blog(LOG_INFO, "\tLookAheadDS set: FAST");
				break;
			default:
				mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_UNKNOWN;
				blog(LOG_INFO, "\tLookAheadDS set: AUTO");
				break;
			}
		}

		switch ((int)pParams->bRawRef) {
		case 0:
			mfx_co2.UseRawRef = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tUseRawRef set: OFF");
			break;
		case 1:
			mfx_co2.UseRawRef = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tUseRawRef set: ON");
			break;
		default:
			mfx_co2.UseRawRef = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tUseRawRef set: AUTO");
			break;
		}

		switch ((int)pParams->bExtBRC) {
		case 0:
			mfx_co2.ExtBRC = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tExtBRC set: OFF");
			break;
		case 1:
			mfx_co2.ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: ON");
			break;
		default:
			mfx_co2.ExtBRC = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tExtBRC set: AUTO");
			break;
		}

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_co2);
	}

	if (mfx_co3_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co3, MFX_EXTBUFF_CODING_OPTION3);

		mfx_co3.BitstreamRestriction = MFX_CODINGOPTION_ON;

		//mfx_co3.ContentInfo = MFX_CONTENT_FULL_SCREEN_VIDEO;
		//if (mfx_version.Major >= 2 && mfx_version.Minor >= 8) {
		//	mfx_co3.ContentInfo = MFX_CONTENT_NOISY_VIDEO;
		//}

		switch ((int)pParams->nScenario) {
		case 0:
			blog(LOG_INFO, "\tScenario set: OFF");
			break;
		case 1:
			mfx_co3.ScenarioInfo = MFX_SCENARIO_ARCHIVE;
			blog(LOG_INFO, "\tScenario set: ARCHIVE");
			break;
		case 2:
			mfx_co3.ScenarioInfo = MFX_SCENARIO_LIVE_STREAMING;
			blog(LOG_INFO, "\tScenario set: LIVE STREAMING");
			break;
		case 3:
			mfx_co3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
			blog(LOG_INFO, "\tScenario set: GAME STREAMING");
			break;
		case 4:
			mfx_co3.ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
			blog(LOG_INFO, "\tScenario set: REMOTE GAMING");
			break;
		default:
			mfx_co3.ScenarioInfo = MFX_SCENARIO_UNKNOWN;
			blog(LOG_INFO, "\tScenario set: AUTO");
			break;
		}

		if (mfx_EncParams.mfx.RateControlMethod ==
		    MFX_RATECONTROL_CQP) {
			mfx_co3.EnableQPOffset = MFX_CODINGOPTION_ON;
			mfx_co3.EnableMBQP = MFX_CODINGOPTION_ON;
		}

		if (pParams->nNumRefFrame > 0 && codec != QSV_CODEC_AV1) {
			if (mfx_EncParams.mfx.RateControlMethod !=
				    MFX_RATECONTROL_LA &&
			    mfx_EncParams.mfx.RateControlMethod !=
				    MFX_RATECONTROL_LA_HRD) {
				blog(LOG_INFO, "\tNumRefFrameLayers set: %d",
				     pParams->nNumRefFrameLayers);
				if (pParams->nNumRefFrameLayers > 0) {
					for (int i = 0;
					     i < pParams->nNumRefFrameLayers;
					     i++) {
						if (codec == QSV_CODEC_HEVC &&
						    (codec == QSV_CODEC_AVC &&
						     pParams->bLAExtBRC == 0)) {
							mfx_co3.NumRefActiveP[i] =
								(mfxU16)pParams
									->nNumRefFrame;
						} else {
							mfx_co3.NumRefActiveP[i] =
								(mfxU16)2;
						}
						mfx_co3.NumRefActiveBL0[i] =
							(mfxU16)pParams
								->nNumRefFrame;
						mfx_co3.NumRefActiveBL1[i] =
							(mfxU16)pParams
								->nNumRefFrame;
					}
				} else {
					if (codec == QSV_CODEC_HEVC &&
					    (codec == QSV_CODEC_AVC &&
					     pParams->bLAExtBRC == 0)) {
						mfx_co3.NumRefActiveP[0] =
							(mfxU16)pParams
								->nNumRefFrame;
					} else {
						mfx_co3.NumRefActiveP[0] =
							(mfxU16)2;
					}
					mfx_co3.NumRefActiveBL0[0] =
						(mfxU16)pParams->nNumRefFrame;
					mfx_co3.NumRefActiveBL1[0] =
						(mfxU16)pParams->nNumRefFrame;
				}
			} else {
				mfx_co3.NumRefActiveP[0] = (mfxU16)2;
				mfx_co3.NumRefActiveBL0[0] = (mfxU16)2;
				mfx_co3.NumRefActiveBL1[0] = (mfxU16)2;
			}
		}

		switch ((int)pParams->bCPUEncTools) {
		case 0:
			mfx_co3.CPUEncToolsProcessing = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tCPUEncTools set: OFF");
			break;
		case 1:
			mfx_co3.CPUEncToolsProcessing = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tCPUEncTools set: ON");
			break;
		default:
			mfx_co3.CPUEncToolsProcessing = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tCPUEncTools set: AUTO (ON)");
			break;
		}

		if (codec == QSV_CODEC_HEVC) {
			switch ((int)pParams->bGPB) {
			case 0:
				mfx_co3.GPB = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tGPB set: OFF");
				break;
			case 1:
				mfx_co3.GPB = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tGPB set: ON");
				break;
			default:
				mfx_co3.GPB = MFX_CODINGOPTION_UNKNOWN;
				blog(LOG_INFO, "\tGPB set: AUTO");
				break;
			}
		}

		if ((int)pParams->nMaxFrameSizeType >= -1) {
			switch ((int)pParams->nMaxFrameSizeType) {
			case 0:
				mfx_co3.MaxFrameSizeI =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizeIMultiplier);
				mfx_co3.MaxFrameSizeP =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizePMultiplier);
				blog(LOG_INFO,
				     "\tMaxFrameSizeType set: MANUAL\n \tMaxFrameSizeI set: %d Kb\n \tMaxFrameSizeP set: %d Kb",
				     (int)(mfx_co3.MaxFrameSizeI / 8 / 1024),
				     (int)(mfx_co3.MaxFrameSizeP / 8 / 1024));
				break;
			case 1:
				mfx_co3.MaxFrameSizeI =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizeIMultiplier);
				mfx_co3.MaxFrameSizeP =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizePMultiplier);
				mfx_co3.AdaptiveMaxFrameSize =
					MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tMaxFrameSizeType set: MANUAL | ADAPTIVE\n \tMaxFrameSizeI set: %d Kb\n \tMaxFrameSize set: %d Kb\n \tAdaptiveMaxFrameSize set: ON",
				     (int)(mfx_co3.MaxFrameSizeI / 8 / 1024),
				     (int)(mfx_co3.MaxFrameSizeP / 8 / 1024));
				break;
			default:
				break;
			}
		}

		if ((int)pParams->nPRefType >= -1) {
			switch ((int)pParams->nPRefType) {
			case 0:
				mfx_co3.PRefType = MFX_P_REF_SIMPLE;
				blog(LOG_INFO, "\tPRef set: SIMPLE");
				break;
			case 1:
				mfx_co3.PRefType = MFX_P_REF_PYRAMID;
				blog(LOG_INFO, "\tPRef set: PYRAMID");
				break;
			default:
				mfx_co3.PRefType = MFX_P_REF_DEFAULT;
				blog(LOG_INFO, "\tPRef set: DEFAULT");
				break;
			}
		}

		switch ((int)pParams->bAdaptiveCQM) {
		case 0:
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveCQM set: OFF");
			break;
		case 1:
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveCQM set: ON");
			break;
		default:
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tAdaptiveCQM set: AUTO");
			break;
		}

		if (mfx_version.Major >= 2 && mfx_version.Minor >= 4) {

			switch ((int)pParams->bAdaptiveRef) {
			case 0:
				mfx_co3.AdaptiveRef = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveRef set: OFF");
				break;
			case 1:
				mfx_co3.AdaptiveRef = MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tAdaptiveRef set: ON\n Warning! If you get video artifacts, please turn off AdaptiveRef");
				break;
			default:
				mfx_co3.AdaptiveRef = MFX_CODINGOPTION_UNKNOWN;
				blog(LOG_INFO, "\tAdaptiveRef set: AUTO");
				break;
			}

			switch ((int)pParams->bAdaptiveLTR) {
			case 0:
				mfx_co3.AdaptiveLTR = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveLTR set: OFF");
				break;
			case 1:
				mfx_co3.AdaptiveLTR = MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tAdaptiveLTR set: ON\n Warning! If you get an encoder error or video artifacts, please turn off AdaptiveLTR");
				break;
			default:
				mfx_co3.AdaptiveLTR = MFX_CODINGOPTION_UNKNOWN;
				blog(LOG_INFO, "\tAdaptiveLTR set: AUTO");
				break;
			}
		}

		if (pParams->nWinBRCMaxAvgSize > 0) {
			mfx_co3.WinBRCMaxAvgKbps = pParams->nWinBRCMaxAvgSize;
			blog(LOG_INFO, "\tWinBRCMaxSize set: %d",
			     pParams->nWinBRCMaxAvgSize);
		}

		if (pParams->nWinBRCSize > 0) {
			mfx_co3.WinBRCSize = (mfxU16)pParams->nWinBRCSize;
			blog(LOG_INFO, "\tWinBRCSize set: %d",
			     mfx_co3.WinBRCSize);
		}

		if ((int)pParams->nMotionVectorsOverPicBoundaries >= -1) {
			switch ((int)pParams->nMotionVectorsOverPicBoundaries) {
			case 0:
				mfx_co3.MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_OFF;
				blog(LOG_INFO,
				     "\tMotionVectorsOverPicBoundaries set: OFF");
				break;
			case 1:
				mfx_co3.MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tMotionVectorsOverPicBoundaries set: ON");
				break;
			default:
				mfx_co3.MotionVectorsOverPicBoundaries =
					MFX_CODINGOPTION_UNKNOWN;
				blog(LOG_INFO,
				     "\tMotionVectorsOverPicBoundaries set: AUTO");
				break;
			}
		}

		// Not supported on dGPU, pending a fix from Intel
		//if (mfx_EncParams.mfx.LowPower != MFX_CODINGOPTION_ON || pParams->bLowPower != 1)
		//{
		//	switch ((int)pParams->nRepartitionCheckEnable) {
		//		case (int)0:
		//			mfx_co3.RepartitionCheckEnable =
		//				MFX_CODINGOPTION_OFF;
		//			blog(LOG_INFO,
		//			     "\tRepartitionCheckEnable set: PERFORMANCE");
		//			break;
		//		case (int)1:
		//			mfx_co3.RepartitionCheckEnable =
		//				MFX_CODINGOPTION_ON;
		//			blog(LOG_INFO,
		//			     "\tRepartitionCheckEnable set: QUALITY");
		//			break;
		//		case (int)2:
		//			mfx_co3.RepartitionCheckEnable =
		//				MFX_CODINGOPTION_UNKNOWN;
		//			blog(LOG_INFO,
		//			     "\tRepartitionCheckEnable set: AUTO");
		//			break;
		//	}
		//}

		switch ((int)pParams->bWeightedPred) {
		case 0:
			blog(LOG_INFO, "\tWeightedPred set: OFF");
			break;
		case 1:
			mfx_co3.WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
			blog(LOG_INFO, "\tWeightedPred set: ON");
			break;
		default:
			mfx_co3.WeightedPred = MFX_WEIGHTED_PRED_UNKNOWN;
			blog(LOG_INFO, "\tWeightedPred set: AUTO");
			break;
		}

		switch ((int)pParams->bWeightedBiPred) {
		case 0:
			blog(LOG_INFO, "\tWeightedBiPred set: OFF");
			break;
		case 1:
			mfx_co3.WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
			blog(LOG_INFO, "\tWeightedBiPred set: ON");
			break;
		default:
			mfx_co3.WeightedBiPred = MFX_WEIGHTED_PRED_UNKNOWN;
			blog(LOG_INFO, "\tWeightedBiPred set: AUTO");
			break;
		}

		if (pParams->bGlobalMotionBiasAdjustment == 1) {
			mfx_co3.GlobalMotionBiasAdjustment =
				MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tGlobalMotionBiasAdjustment set: ON");

			switch (pParams->nMVCostScalingFactor) {
			case 1:
				mfx_co3.MVCostScalingFactor = (mfxU16)1;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/2");
				break;
			case 2:
				mfx_co3.MVCostScalingFactor = (mfxU16)2;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/4");
				break;
			case 3:
				mfx_co3.MVCostScalingFactor = (mfxU16)3;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: 1/8");
				break;
			default:
				mfx_co3.MVCostScalingFactor = (mfxU16)0;
				blog(LOG_INFO,
				     "\tMVCostScalingFactor set: DEFAULT");
				break;
			}
		} else if (pParams->bGlobalMotionBiasAdjustment == 0) {
			mfx_co3.GlobalMotionBiasAdjustment =
				MFX_CODINGOPTION_OFF;
		}

		switch ((int)pParams->bDirectBiasAdjustment) {
		case 0:
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: OFF");
			break;
		case 1:
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: ON");
			break;
		default:
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: AUTO");
			break;
		}

		//Not supported on dGPU, pending a fix from Intel
		//if ((bool)pParams->bFadeDetection) {
		//	mfx_co3.FadeDetection = MFX_CODINGOPTION_ON;
		//	blog(LOG_INFO, "\tFadeDetection set: ON");
		//}

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_co3);
	}

	if (pParams->nMotionVectorsOverPicBoundaries > -1) {
		INIT_MFX_EXT_BUFFER(mfx_ExtMVOverPicBoundaries,
				    MFX_EXTBUFF_MV_OVER_PIC_BOUNDARIES);

		switch ((int)pParams->nMotionVectorsOverPicBoundaries) {
		case 0:
			mfx_ExtMVOverPicBoundaries.StickBottom =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickLeft =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickRight =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickTop =
				MFX_CODINGOPTION_ON;
			break;
		case 1:
			mfx_ExtMVOverPicBoundaries.StickBottom =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickLeft =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickRight =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickTop =
				MFX_CODINGOPTION_OFF;
			break;
		default:
			mfx_ExtMVOverPicBoundaries.StickBottom =
				MFX_CODINGOPTION_UNKNOWN;
			mfx_ExtMVOverPicBoundaries.StickLeft =
				MFX_CODINGOPTION_UNKNOWN;
			mfx_ExtMVOverPicBoundaries.StickRight =
				MFX_CODINGOPTION_UNKNOWN;
			mfx_ExtMVOverPicBoundaries.StickTop =
				MFX_CODINGOPTION_UNKNOWN;
			break;
		}
	}

	if (mfx_EncToolsConf_enable == 1 &&
	    (mfx_version.Major >= 2 && mfx_version.Minor >= 8) &&
	    pParams->bCPUEncTools == 1) {
		INIT_MFX_EXT_BUFFER(mfx_EncToolsConf,
				    MFX_EXTBUFF_ENCTOOLS_CONFIG);

		mfx_EncToolsConf.AdaptiveI = pParams->bAdaptiveI == 1
						     ? MFX_CODINGOPTION_ON
						     : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveB = pParams->bAdaptiveB == 1
						     ? MFX_CODINGOPTION_ON
						     : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.SceneChange =
			(pParams->bAdaptiveB == 1 || pParams->bAdaptiveI == 1)
				? MFX_CODINGOPTION_ON
				: MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptivePyramidQuantP =
			pParams->bAdaptiveI == 1 ? MFX_CODINGOPTION_ON
						 : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptivePyramidQuantB =
			pParams->bAdaptiveB == 1 ? MFX_CODINGOPTION_ON
						 : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveQuantMatrices =
			pParams->bAdaptiveCQM == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;

		if (mfx_EncParams.mfx.RateControlMethod ==
		    MFX_RATECONTROL_CQP) {
			mfx_EncToolsConf.AdaptiveMBQP = MFX_CODINGOPTION_ON;
		}

		if (pParams->bLAExtBRC == 1 ||
		    pParams->RateControl == MFX_RATECONTROL_LA ||
		    pParams->RateControl == MFX_RATECONTROL_LA_HRD) {
			mfx_EncToolsConf.SaliencyMapHint = MFX_CODINGOPTION_OFF;
		} else {
			mfx_EncToolsConf.SaliencyMapHint = MFX_CODINGOPTION_ON;
		}

		mfx_EncToolsConf.AdaptiveLTR =
			pParams->bAdaptiveLTR == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefP =
			pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefB =
			pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		if (pParams->bCPUBufferHints == 1) {

			mfx_EncToolsConf.BRCBufferHints = MFX_CODINGOPTION_ON;
		}
		if (pParams->bCPUBRCControl == 1) {
			mfx_EncToolsConf.BRC = MFX_CODINGOPTION_ON;
		}
		mfx_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_EncToolsConf);
	}
	//Wait next VPL release for support this
	//if (mfx_version.Major >= 2 && mfx_version.Minor >= 9 &&
	//    pParams->nTuneQualityMode > -1) {
	//	INIT_MFX_EXT_BUFFER(mfx_Tune, MFX_EXTBUFF_TUNE_ENCODE_QUALITY);

	//	switch ((int)pParams->nTuneQualityMode) {
	//	case 1:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_PSNR;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PSNR");
	//		break;
	//	case 2:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: SSIM");
	//		break;
	//	case 3:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
	//		blog(LOG_INFO, "\tTuneQualityMode set: MS SSIM");
	//		break;
	//	case 4:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_VMAF;
	//		blog(LOG_INFO, "\tTuneQualityMode set: VMAF");
	//		break;
	//	case 5:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
	//		blog(LOG_INFO, "\tTuneQualityMode set: PERCEPTUAL");
	//		break;
	//	default:
	//		mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_DEFAULT;
	//		break;
	//	}

	//	mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Tune);
	//}

	if (codec == QSV_CODEC_HEVC) {
		if ((pParams->nWidth & 15) || (pParams->nHeight & 15)) {
			INIT_MFX_EXT_BUFFER(mfx_ExtHEVCParam,
					    MFX_EXTBUFF_HEVC_PARAM);

			mfx_ExtHEVCParam.PicWidthInLumaSamples =
				mfx_EncParams.mfx.FrameInfo.Width;
			mfx_ExtHEVCParam.PicHeightInLumaSamples =
				mfx_EncParams.mfx.FrameInfo.Height;
			mfx_ExtHEVCParam.LCUSize = pParams->nCTU;
			mfx_ExtHEVCParam.SampleAdaptiveOffset = pParams->SAO;

			mfx_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_ExtHEVCParam);
		}
	}

	if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
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

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPDenoise);
	}

	//TODO: Add support VPP filters
	//INIT_MFX_EXT_BUFFER(mfx_VPPScaling, MFX_EXTBUFF_VPP_SCALING);

	//mfx_VPPScaling.ScalingMode = MFX_SCALING_MODE_QUALITY;
	//mfx_VPPScaling.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

	//mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPScaling);

	if (codec == QSV_CODEC_AV1) {
		if (mfx_version.Major >= 2 && mfx_version.Minor >= 5) {
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1BitstreamParam,
					    MFX_EXTBUFF_AV1_BITSTREAM_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1ResolutionParam,
					    MFX_EXTBUFF_AV1_RESOLUTION_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1TileParam,
					    MFX_EXTBUFF_AV1_TILE_PARAM);

			mfx_ExtAV1BitstreamParam.WriteIVFHeaders =
				MFX_CODINGOPTION_OFF;

			mfx_ExtAV1ResolutionParam.FrameHeight =
				mfx_EncParams.mfx.FrameInfo.Height;
			mfx_ExtAV1ResolutionParam.FrameWidth =
				mfx_EncParams.mfx.FrameInfo.Width;

			mfx_ExtAV1TileParam.NumTileGroups = 1;
			mfx_ExtAV1TileParam.NumTileColumns = 1;
			mfx_ExtAV1TileParam.NumTileRows = 1;

			mfx_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_ExtAV1BitstreamParam);
			mfx_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_ExtAV1ResolutionParam);
			mfx_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_ExtAV1TileParam);
		}
	}

	INIT_MFX_EXT_BUFFER(mfx_ExtVideoSignalInfo,
			    MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

	mfx_ExtVideoSignalInfo.VideoFormat = pParams->VideoFormat;
	mfx_ExtVideoSignalInfo.VideoFullRange = pParams->VideoFullRange;
	mfx_ExtVideoSignalInfo.ColourDescriptionPresent = 1;
	mfx_ExtVideoSignalInfo.ColourPrimaries = pParams->ColourPrimaries;
	mfx_ExtVideoSignalInfo.TransferCharacteristics =
		pParams->TransferCharacteristics;
	mfx_ExtVideoSignalInfo.MatrixCoefficients = pParams->MatrixCoefficients;

	mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_ExtVideoSignalInfo);

	if (codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_ExtChromaLocInfo,
				    MFX_EXTBUFF_CHROMA_LOC_INFO);

		mfx_ExtChromaLocInfo.ChromaLocInfoPresentFlag = 1;
		mfx_ExtChromaLocInfo.ChromaSampleLocTypeTopField =
			pParams->ChromaSampleLocTypeTopField;
		mfx_ExtChromaLocInfo.ChromaSampleLocTypeBottomField =
			pParams->ChromaSampleLocTypeBottomField;

		mfx_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_ExtChromaLocInfo);
	}

	if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC ||
	     (codec == QSV_CODEC_AV1 &&
	      (mfx_version.Major >= 2 && mfx_version.Minor >= 9))) &&
	    pParams->MaxContentLightLevel > 0) {
		INIT_MFX_EXT_BUFFER(
			mfx_ExtMasteringDisplayColourVolume,
			MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME);
		INIT_MFX_EXT_BUFFER(mfx_ExtContentLightLevelInfo,
				    MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO);

		mfx_ExtMasteringDisplayColourVolume.InsertPayloadToggle =
			MFX_PAYLOAD_IDR;
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesX[0] =
			pParams->DisplayPrimariesX[0];
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesX[1] =
			pParams->DisplayPrimariesX[1];
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesX[2] =
			pParams->DisplayPrimariesX[2];
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesY[0] =
			pParams->DisplayPrimariesY[0];
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesY[1] =
			pParams->DisplayPrimariesY[1];
		mfx_ExtMasteringDisplayColourVolume.DisplayPrimariesY[2] =
			pParams->DisplayPrimariesY[2];
		mfx_ExtMasteringDisplayColourVolume.WhitePointX =
			pParams->WhitePointX;
		mfx_ExtMasteringDisplayColourVolume.WhitePointY =
			pParams->WhitePointY;
		mfx_ExtMasteringDisplayColourVolume
			.MaxDisplayMasteringLuminance =
			pParams->MaxDisplayMasteringLuminance;
		mfx_ExtMasteringDisplayColourVolume
			.MinDisplayMasteringLuminance =
			pParams->MinDisplayMasteringLuminance;

		mfx_ExtContentLightLevelInfo.InsertPayloadToggle =
			MFX_PAYLOAD_IDR;
		mfx_ExtContentLightLevelInfo.MaxContentLightLevel =
			pParams->MaxContentLightLevel;
		mfx_ExtContentLightLevelInfo.MaxPicAverageLightLevel =
			pParams->MaxPicAverageLightLevel;

		mfx_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_ExtMasteringDisplayColourVolume);
		mfx_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_ExtContentLightLevelInfo);
	}

	mfx_EncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
	if (codec == QSV_CODEC_AVC || mfx_VPPDoUse.NumAlg > 0) {
		mfx_EncParams.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	}

	mfx_EncParams.ExtParam = mfx_ExtendedBuffers.data();
	mfx_EncParams.NumExtParam = (mfxU16)mfx_ExtendedBuffers.size();

	blog(LOG_INFO, "Feature extended buffer size: %d",
	     mfx_ExtendedBuffers.size());
	// We dont check what was valid or invalid here, just try changing lower power.
	// Ensure set values are not overwritten so in case it wasnt lower power we fail
	// during the parameter check.
	mfxVideoParam validParams = {0};
	memcpy(&validParams, &mfx_EncParams, sizeof(validParams));
	mfxStatus sts = mfx_VideoEnc->Query(&mfx_EncParams, &validParams);
	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
		if (mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
			mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
			mfx_co2.LookAheadDepth = pParams->nLADepth;
		}
	}

	return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(qsv_param_t *pParams)
{
	if (pParams->bResetAllowed == true) {

		switch (pParams->RateControl) {
		case MFX_RATECONTROL_CBR:
			mfx_EncParams.mfx.TargetKbps =
				(mfxU16)pParams->nTargetBitRate;
			break;
		case MFX_RATECONTROL_VBR:
			mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
			mfx_EncParams.mfx.MaxKbps =
				pParams->nTargetBitRate > pParams->nMaxBitRate
					? pParams->nTargetBitRate
					: pParams->nMaxBitRate;
			break;
		case MFX_RATECONTROL_CQP:
			mfx_EncParams.mfx.QPI = pParams->nQPI;
			mfx_EncParams.mfx.QPP = pParams->nQPP;
			mfx_EncParams.mfx.QPB = pParams->nQPB;
			break;
		case MFX_RATECONTROL_ICQ:
			mfx_EncParams.mfx.ICQQuality = pParams->nICQQuality;
			break;
		default:
			return false;
			break;
		}
	}
	return true;
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder()
{
	std::vector<mfxExtBuffer *> extendedBuffers;
	extendedBuffers.reserve(1);

	INIT_MFX_EXT_BUFFER(mfx_ResetOption, MFX_EXTBUFF_ENCODER_RESET_OPTION);
	mfx_ResetOption.StartNewSequence = MFX_CODINGOPTION_ON;

	extendedBuffers.push_back((mfxExtBuffer *)&mfx_ResetOption);

	mfx_EncParams.ExtParam = extendedBuffers.data();
	mfx_EncParams.NumExtParam = (mfxU16)extendedBuffers.size();

	mfxStatus sts = mfx_VideoEnc->Query(&mfx_EncParams, &mfx_EncParams);

	if (sts >= MFX_ERR_NONE) {
		sts = mfx_VideoEnc->Reset(&mfx_EncParams);
	}
	blog(LOG_INFO, "\tReset status:     %d", sts);
	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec)
{
	INIT_MFX_EXT_BUFFER(mfx_ExtCOSPSPPS, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

	std::vector<mfxExtBuffer *> extendedBuffers;
	extendedBuffers.reserve(2);

	mfx_ExtCOSPSPPS.SPSBuffer = mU8_SPSBuffer;
	mfx_ExtCOSPSPPS.PPSBuffer = mU8_PPSBuffer;
	mfx_ExtCOSPSPPS.SPSBufSize = 1024; //  m_nSPSBufferSize;
	mfx_ExtCOSPSPPS.PPSBufSize = 1024; //  m_nPPSBufferSize;

	if (codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_ExtCOVPS,
				    MFX_EXTBUFF_CODING_OPTION_VPS);

		mfx_ExtCOVPS.VPSBuffer = mU8_VPSBuffer;
		mfx_ExtCOVPS.VPSBufSize = 1024;

		extendedBuffers.push_back((mfxExtBuffer *)&mfx_ExtCOVPS);
	}

	extendedBuffers.push_back((mfxExtBuffer *)&mfx_ExtCOSPSPPS);

	mfx_EncParams.ExtParam = extendedBuffers.data();
	mfx_EncParams.NumExtParam = (mfxU16)extendedBuffers.size();

	blog(LOG_INFO, "Video params extended buffer size: %d",
	     extendedBuffers.size());

	mfxStatus sts = mfx_VideoEnc->GetVideoParam(&mfx_EncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (codec == QSV_CODEC_HEVC) {
		mU16_VPSBufSize = mfx_ExtCOVPS.VPSBufSize;
	}

	mU16_SPSBufSize = mfx_ExtCOSPSPPS.SPSBufSize;
	mU16_PPSBufSize = mfx_ExtCOSPSPPS.PPSBufSize;

	return sts;
}

void QSV_VPL_Encoder_Internal::GetSPSPPS(mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
					 mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf)
{
	*pSPSBuf = mU8_SPSBuffer;
	*pPPSBuf = mU8_PPSBuffer;
	*pnSPSBuf = mU16_SPSBufSize;
	*pnPPSBuf = mU16_PPSBufSize;
}

void QSV_VPL_Encoder_Internal::GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
					    mfxU8 **pPPSBuf, mfxU16 *pnVPSBuf,
					    mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf)
{
	*pVPSBuf = mU8_VPSBuffer;
	*pnVPSBuf = mU16_VPSBufSize;

	*pSPSBuf = mU8_SPSBuffer;
	*pnSPSBuf = mU16_SPSBufSize;

	*pPPSBuf = mU8_PPSBuffer;
	*pnPPSBuf = mU16_PPSBufSize;
}

mfxStatus QSV_VPL_Encoder_Internal::InitBitstream()
{
	mU16_TaskPool = mfx_EncParams.AsyncDepth;
	n_FirstSyncTask = 0;

	t_TaskPool = new Task[mU16_TaskPool];
	memset(t_TaskPool, 0, sizeof(Task) * mU16_TaskPool);

	//int DataLenght = mfx_EncParams.mfx.RateControlMethod ==
	//					 MFX_RATECONTROL_CQP ||
	//				 mfx_EncParams.mfx.RateControlMethod ==
	//					 MFX_RATECONTROL_ICQ ||
	//				 mfx_EncParams.mfx.RateControlMethod ==
	//					 MFX_RATECONTROL_LA_ICQ
	//			 ? 0
	//			 : (mfx_EncParams.mfx.BufferSizeInKB * 1024 * 16);

	for (int i = 0; i < mU16_TaskPool; i++) {
		t_TaskPool[i].mfxBS.MaxLength =
			mfx_EncParams.mfx.BufferSizeInKB * 1024 * 10;
		t_TaskPool[i].mfxBS.Data =
			new mfxU8[t_TaskPool[i].mfxBS.MaxLength];
		t_TaskPool[i].mfxBS.DataOffset = 0;
		t_TaskPool[i].mfxBS.DataLength = (mfxU32)0;
		mfx_Bitstream.CodecId = mfx_EncParams.mfx.CodecId;
		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS.Data,
				   MFX_ERR_MEMORY_ALLOC);
	}

	mfx_Bitstream.MaxLength = mfx_EncParams.mfx.BufferSizeInKB * 1024 * 10;
	mfx_Bitstream.Data = new mfxU8[mfx_Bitstream.MaxLength];
	mfx_Bitstream.DataOffset = 0;
	mfx_Bitstream.DataLength = (mfxU32)0;
	mfx_Bitstream.CodecId = mfx_EncParams.mfx.CodecId;

	blog(LOG_INFO, "\tTaskPool count:    %d", mU16_TaskPool);

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
	const mfxU16 line_size = w * 2;

	// load Y plane
	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataY + i * strideY, line_size);

	// load UV plane
	h /= 2;
	ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;

	for (i = 0; i < h; i++)
		memcpy(ptr + i * pitch, pDataUV + i * strideUV, line_size);
	/*pSurface->FrameInterface->Release(pSurface);*/
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
	/*pSurface->FrameInterface->Release(pSurface);*/
	return MFX_ERR_NONE;
}

int QSV_VPL_Encoder_Internal::GetFreeTaskIndex(Task *t_TaskPool,
					       mfxU16 nPoolSize)
{
	if (t_TaskPool) {
		for (int i = 0; i < nPoolSize; i++) {
			if (t_TaskPool[i].syncp == nullptr) {
				return i;
			}
		}
	}
	return -1;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(uint64_t ts, uint8_t *pDataY,
					   uint8_t *pDataUV, uint32_t strideY,
					   uint32_t strideUV,
					   mfxBitstream **pBS)
{
	mfxStatus sts = MFX_ERR_NONE;

	mfxFrameSurface1 *mfx_FrameSurface = nullptr;
	*pBS = nullptr;
	for (;;) {

		int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mU16_TaskPool);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mU16_TaskPool;
		*pBS = &mfx_Bitstream;

		sts = MFXMemory_GetSurfaceForEncode(mfx_session,
						    &mfx_FrameSurface);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		mfx_FrameSurface->FrameInterface->AddRef(mfx_FrameSurface);
		sts = mfx_FrameSurface->FrameInterface->Map(mfx_FrameSurface,
							    MFX_MAP_WRITE);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		memcpy(&mfx_FrameSurface->Info, &mfx_EncParams.mfx.FrameInfo,
		       sizeof(mfxFrameInfo));
		if (mfx_FrameSurface->Info.FourCC == MFX_FOURCC_P010) {
			LoadP010(mfx_FrameSurface, pDataY, pDataUV, strideY,
				 strideUV);
		} else {

			LoadNV12(mfx_FrameSurface, pDataY, pDataUV, strideY,
				 strideUV);
		}

		mfx_FrameSurface->Data.TimeStamp = ts;

		sts = mfx_FrameSurface->FrameInterface->Unmap(mfx_FrameSurface);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		mfx_FrameSurface->FrameInterface->AddRef(mfx_FrameSurface);
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoEnc->EncodeFrameAsync(
			&mfx_EncCtrl, mfx_FrameSurface,
			&t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		MFXVideoCORE_SyncOperation(
			mfx_session, t_TaskPool[n_FirstSyncTask].syncp, 100);
		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts) {
				MSDK_SLEEP(
					10); // Wait if device is busy, then repeat the same call
			}
		} else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		} else if (MFX_ERR_NONE == sts) {
			MFXVideoCORE_SyncOperation(
				mfx_session, t_TaskPool[n_FirstSyncTask].syncp,
				100);
			break;
		} else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
			MFXVideoCORE_SyncOperation(
				mfx_session, t_TaskPool[n_FirstSyncTask].syncp,
				100);
			break;
		} else {

			break;
		}
	}
	mfx_FrameSurface = nullptr;
	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain()
{
	mfxStatus sts = MFX_ERR_NONE;

	while (t_TaskPool && t_TaskPool[n_FirstSyncTask].syncp) {
		sts = MFXVideoCORE_SyncOperation(
			mfx_session, t_TaskPool[n_FirstSyncTask].syncp, 100);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		t_TaskPool[n_FirstSyncTask].syncp = nullptr;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mU16_TaskPool;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::ClearData()
{
	mfxStatus sts = MFX_ERR_NONE;
	sts = Drain();

	if (mfx_VideoEnc) {
		sts = mfx_VideoEnc->Close();
		delete mfx_VideoEnc;
		mfx_VideoEnc = nullptr;
	}

	if (t_TaskPool) {
		for (int i = 0; i < mU16_TaskPool; i++)
			delete t_TaskPool[i].mfxBS.Data;
		MSDK_SAFE_DELETE_ARRAY(t_TaskPool);
	}

	if (mfx_Bitstream.Data) {
		delete[] mfx_Bitstream.Data;
		mfx_Bitstream.Data = nullptr;
	}

	if (sts >= MFX_ERR_NONE) {
		g_numEncodersOpen--;
	}

	if (g_numEncodersOpen <= 0) {
		g_DX_Handle = nullptr;
	}

	if (mfx_session) {
		MFXClose(mfx_session);
		MFXDispReleaseImplDescription(mfx_loader, nullptr);
		MFXUnload(mfx_loader);
		mfx_session = nullptr;
		mfx_loader = nullptr;
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
