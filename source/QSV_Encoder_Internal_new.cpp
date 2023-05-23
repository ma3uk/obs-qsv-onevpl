#define MFX_DEPRECATED_OFF
#define ONEVPL_EXPERIMENTAL
#define MFX_ENABLE_ENCTOOLS

#include "common_directx11.h"
#include "QSV_Encoder_Internal_new.h"
#include "QSV_Encoder.h"
#include "mfx.h"
#include <VersionHelpers.h>
#include <obs-module.h>

#define do_log(level, format, ...) \
	blog(level, "[qsv encoder: '%s'] " format, "vpl_impl", ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

mfxHDL QSV_VPL_Encoder_Internal::g_DX_Handle = nullptr;
mfxU16 QSV_VPL_Encoder_Internal::g_numEncodersOpen = 0;
static mfxLoader mfx_loader = nullptr;
static mfxConfig mfx_cfg[10] = {};
static mfxVariant mfx_variant[10] = {};
static int mfx_EncToolsConf_enable = 1;
static int mfx_co_enable = 1;
static int mfx_co2_enable = 1;
static int mfx_co3_enable = 1;
QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxIMPL &impl,
						   mfxVersion &version,
						   bool isDGPU)
	: mfx_FrameSurf(nullptr),
	  mfx_VideoEnc(nullptr),
	  mU16_SPSBufSize(1024),
	  mU16_PPSBufSize(1024),
	  mU16_VPSBufSize(),
	  mU16_TaskPool(0),
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
	  mfx_EncControl()
{
	mfxIMPL tempImpl = impl | MFX_IMPL_VIA_D3D11;
	mfx_loader = MFXLoad();
	mfxStatus sts = MFXCreateSession(mfx_loader, 0, &mfx_session);
	if (sts == MFX_ERR_NONE) {
		sts = MFXQueryVersion(mfx_session, &version);
		if (sts == MFX_ERR_NONE) {
			mfx_version = version;
			sts = MFXQueryIMPL(mfx_session, &tempImpl);
			if (sts == MFX_ERR_NONE) {
				mfx_impl = impl;
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
mfxStatus QSV_VPL_Encoder_Internal::Initialize(
	mfxIMPL impl, mfxVersion ver, mfxSession *pSession,
	mfxFrameAllocator *mfx_FrameAllocator, mfxHDL *deviceHandle,
	int deviceNum)
{
	mfx_FrameAllocator; // (Hugh) Currently unused

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

		blog(LOG_INFO, "\tSelected GPU: %d", ((int)mfx_impl - 773));

		MFXSetPriority(mfx_session, MFX_PRIORITY_HIGH);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		// Create DirectX device context
		if (g_DX_Handle == NULL) {
			sts = CreateHWDevice(mfx_session, &g_DX_Handle);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		}

		if (g_DX_Handle == NULL)
			return MFX_ERR_DEVICE_FAILED;

		// Provide device manager to VPL

		sts = MFXVideoCORE_SetHandle(mfx_session, DEVICE_MGR_TYPE,
					     g_DX_Handle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfx_FrameAllocator->pthis =
			mfx_session; // We use VPL session ID as the allocation identifier
		mfx_FrameAllocator->Alloc = simple_alloc;
		mfx_FrameAllocator->Free = simple_free;
		mfx_FrameAllocator->Lock = simple_lock;
		mfx_FrameAllocator->Unlock = simple_unlock;
		mfx_FrameAllocator->GetHDL = simple_gethdl;

		// Since we are using video memory we must provide VPL with an external allocator
		sts = MFXVideoCORE_SetFrameAllocator(mfx_session,
						     mfx_FrameAllocator);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
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

	sts = AllocateSurfaces();
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
	mfx_EncControl.ExtParam = mfx_ExtendedBuffers.data();
	mfx_EncControl.NumExtParam = (mfxU16)mfx_CtrlExtendedBuffers.size();

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::InitEncParams(qsv_param_t *pParams,
					       enum qsv_codec codec)
{
	//mfx_EncParams.mfx.NumThread = 64;
	//mfx_EncParams.mfx.RestartInterval = 1;
	

	if (codec == QSV_CODEC_AVC) {
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AVC;
		mfx_EncParams.mfx.CodecLevel = 51;
	} else if (codec == QSV_CODEC_AV1) {
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AV1;
		/*mfx_EncParams.mfx.CodecLevel = 73;*/
	} else if (codec == QSV_CODEC_HEVC) {
		mfx_EncParams.mfx.CodecId = MFX_CODEC_HEVC;
		/*mfx_EncParams.mfx.CodecLevel = 62;*/
	}

	if ((pParams->nNumRefFrame >= 0) && (pParams->nNumRefFrame < 16)) {
		mfx_EncParams.mfx.NumRefFrame = pParams->nNumRefFrame;
		blog(LOG_INFO, "\tNumRefFrame set to: %d",
		     pParams->nNumRefFrame);
	}

	mfx_EncParams.mfx.TargetUsage = pParams->nTargetUsage;
	mfx_EncParams.mfx.CodecProfile = pParams->CodecProfile;
	mfx_EncParams.mfx.CodecProfile |= pParams->HEVCTier;
	mfx_EncParams.mfx.FrameInfo.FrameRateExtN = pParams->nFpsNum;
	mfx_EncParams.mfx.FrameInfo.FrameRateExtD = pParams->nFpsDen;
	if (pParams->video_fmt_10bit) {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
		mfx_EncParams.mfx.FrameInfo.BitDepthChroma = 10;
		mfx_EncParams.mfx.FrameInfo.BitDepthLuma = 10;
		mfx_EncParams.mfx.FrameInfo.Shift = 1;
	} else {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture
	
	mfx_EncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(pParams->nWidth);
	mfx_EncParams.mfx.FrameInfo.Height = MSDK_ALIGN16(pParams->nHeight);
	
	mfx_EncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_EncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_EncParams.mfx.FrameInfo.CropX = 0;
	mfx_EncParams.mfx.FrameInfo.CropY = 0;
	mfx_EncParams.mfx.FrameInfo.CropW = pParams->nWidth;
	mfx_EncParams.mfx.FrameInfo.CropH = pParams->nHeight;

	if (pParams->bLowPower == 1) {
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
	} else if (pParams->bLowPower == 0) {
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
		blog(LOG_INFO, "\tLowpower set: OFF");
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
	}

	mfx_EncParams.mfx.RateControlMethod = pParams->RateControl;

	mfx_EncParams.mfx.BRCParamMultiplier = 1;
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_CBR:
		mfx_EncParams.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		break;
	case MFX_RATECONTROL_VBR:
	case MFX_RATECONTROL_VCM:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.MaxKbps = pParams->nMaxBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = pParams->nTargetBitRate * 2;
		mfx_EncParams.mfx.InitialDelayInKB =
			(mfxU16)(mfx_EncParams.mfx.BufferSizeInKB / 2);
		break;
	case MFX_RATECONTROL_CQP:
		mfx_EncParams.mfx.QPI = pParams->nQPI;
		mfx_EncParams.mfx.QPB = pParams->nQPB;
		mfx_EncParams.mfx.QPP = pParams->nQPP;
		break;
	case MFX_RATECONTROL_AVBR:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.Accuracy = pParams->nAccuracy;
		mfx_EncParams.mfx.Convergence = pParams->nConvergence;
		break;
	case MFX_RATECONTROL_ICQ:
	case MFX_RATECONTROL_LA_ICQ:
		mfx_EncParams.mfx.ICQQuality = pParams->nICQQuality;
		break;
	case MFX_RATECONTROL_LA:
	case MFX_RATECONTROL_LA_HRD:
		mfx_EncParams.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_EncParams.mfx.MaxKbps = (mfxU16)pParams->nTargetBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = (mfxU16)((mfx_EncParams.mfx.TargetKbps / pParams->nFpsNum) * pParams->nLADepth);
		mfx_EncParams.mfx.InitialDelayInKB = (mfxU16)mfx_EncParams.mfx.BufferSizeInKB / 2;
		break;
	}

	mfx_EncParams.AsyncDepth = pParams->nAsyncDepth;

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
		} else if (mfx_EncParams.mfx.GopRefDist > 16 && pParams->bExtBRC == 1) {
			mfx_EncParams.mfx.GopRefDist = 16;
			blog(LOG_INFO, "\tExtBRC does not support GOPRefDist greater than 16.");
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
		blog(LOG_INFO, "\tGOPRefDist set to:     %d", (int)mfx_EncParams.mfx.GopRefDist);
	}

	

	if (mfx_co_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co, MFX_EXTBUFF_CODING_OPTION);

		mfx_co.CAVLC = MFX_CODINGOPTION_OFF;

		mfx_co.SingleSeiNalUnit = MFX_CODINGOPTION_OFF;
		//mfx_co.RefPicMarkRep = MFX_CODINGOPTION_ON;
		//mfx_co.PicTimingSEI = MFX_CODINGOPTION_ON;
		mfx_co.FieldOutput = MFX_CODINGOPTION_ON;
		//mfx_co.IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		//mfx_co.InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		//mfx_co.MVPrecision = MFX_MVPRECISION_QUARTERPEL;

		if ((int)pParams->nMaxDecFrameBuffering > 0) {
			mfx_co.MaxDecFrameBuffering =
				pParams->nMaxDecFrameBuffering;
			blog(LOG_INFO, "\tMaxDecFrameBuffering set: %d",
			     pParams->nMaxDecFrameBuffering);
		}

		if (pParams->bRDO == 1) {
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tRDO set: ON");
		} else if (pParams->bRDO == 0) {
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tRDO set: OFF");
		}

		if (pParams->bVuiNalHrd == 1) {
			mfx_co.VuiVclHrdParameters = MFX_CODINGOPTION_ON;
			mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: ON");
		} else if (pParams->bVuiNalHrd == 0) {
			mfx_co.VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
			mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: OFF");
		}

		if (pParams->bNalHrdConformance == 1) {
			mfx_co.NalHrdConformance = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tNalHrdConformance set: ON");
		} else if (pParams->bNalHrdConformance == 0) {
			mfx_co.NalHrdConformance = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tNalHrdConformance set: OFF");
		}

		/*mfx_co.ResetRefList = MFX_CODINGOPTION_ON;*/
		
		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_co);
	}

	if (mfx_co2_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co2, MFX_EXTBUFF_CODING_OPTION2);

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
		//mfx_co2.IntRefType = MFX_REFRESH_NO /*MFX_REFRESH_VERTICAL*/;

		if (pParams->bEnableMAD == 1) {
			mfx_co2.EnableMAD = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tMAD set: ON");
		} else if (pParams->bEnableMAD == 0) {
			mfx_co2.EnableMAD = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tMAD set: OFF");
		}

		if (pParams->bFixedFrameRate == 1) {
			mfx_co2.FixedFrameRate = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tFixedFrameRate set: ON");
		} else if (pParams->bFixedFrameRate == 0) {
			mfx_co2.FixedFrameRate = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tFixedFrameRate set: OFF");
		}

		if (pParams->bExtBRC == 1 && pParams->bLAExtBRC == 1) {
			mfx_co2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:     %d",
			     pParams->nLADepth);
		}

		if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
		    pParams->RateControl == MFX_RATECONTROL_LA ||
		    pParams->RateControl == MFX_RATECONTROL_LA_HRD) {
			mfx_co2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:     %d",
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

		if (pParams->nGOPRefDist >= 2) {
			mfx_co2.BRefType = MFX_B_REF_PYRAMID;
			blog(LOG_INFO, "\tBPyramid set: ON");
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

		if (pParams->bAdaptiveI == 1) {
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveI set: ON");
		} else if (pParams->bAdaptiveI == 0) {
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveI set: OFF");
		}

		if (pParams->bAdaptiveB == 1) {
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveB set: ON");
		} else if (pParams->bAdaptiveB == 0) {
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveB set: OFF");
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

		if (pParams->bRawRef == 1) {
			mfx_co2.UseRawRef = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tUseRawRef set: ON");
		} else if (pParams->bRawRef == 0) {
			mfx_co2.UseRawRef = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tUseRawRef set: OFF");
		}

		if (pParams->bExtBRC == 1) {
			mfx_co2.ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: ON");
		} else if (pParams->bExtBRC == 0) {
			mfx_co2.ExtBRC = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tExtBRC set: OFF");
		}

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_co2);
	}

	if (mfx_co3_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_co3, MFX_EXTBUFF_CODING_OPTION3);

		if (pParams->bLAExtBRC == 1 && pParams->bExtBRC == 1) {
			mfx_co3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
		} else {
			mfx_co3.ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
		}

		if (mfx_version.Major >= 2 && mfx_version.Minor >= 8) {
			mfx_co3.ContentInfo |= MFX_CONTENT_NOISY_VIDEO;
		}

		mfx_co3.BitstreamRestriction = MFX_CODINGOPTION_ON;
		if (mfx_EncParams.mfx.RateControlMethod ==
		    MFX_RATECONTROL_CQP) {
			mfx_co3.EnableMBQP = MFX_CODINGOPTION_ON;
			mfx_co3.EnableQPOffset = MFX_CODINGOPTION_ON;
		}

		if (pParams->nNumRefFrame > 0 && codec != QSV_CODEC_AV1) {
			/*int RefLayers = (pParams->nNumRefFrameLayers + 1);*/
			blog(LOG_INFO, "\tNumRefFrameLayers set: %d",
			     pParams->nNumRefFrameLayers);
			if (pParams->nNumRefFrameLayers > 0) {
				for (int i = 0; i < pParams->nNumRefFrameLayers; i++) {
					if (codec == QSV_CODEC_HEVC || (codec == QSV_CODEC_AVC && pParams->bLAExtBRC == 0)) {
						mfx_co3.NumRefActiveP[i] =
							(mfxU16)pParams->nNumRefFrame;
					}
					mfx_co3.NumRefActiveBL0[i] =
						(mfxU16)pParams->nNumRefFrame;
					mfx_co3.NumRefActiveBL1[i] =
						(mfxU16)pParams->nNumRefFrame;
				}
			} else {
				if (codec == QSV_CODEC_HEVC ||
				    (codec == QSV_CODEC_AVC &&
				     pParams->bLAExtBRC == 0)) {
					mfx_co3.NumRefActiveP[0] =
						(mfxU16)pParams->nNumRefFrame;
				}
				mfx_co3.NumRefActiveBL0[0] =
					(mfxU16)pParams->nNumRefFrame;
				mfx_co3.NumRefActiveBL1[0] =
					(mfxU16)pParams->nNumRefFrame;
			}
		}

		if (pParams->bExtBRC == 1) {
			mfx_co3.CPUEncToolsProcessing = MFX_CODINGOPTION_ON;
		} else if (pParams->bExtBRC == 0) {
			mfx_co3.CPUEncToolsProcessing = MFX_CODINGOPTION_OFF;
		}

		if (codec == QSV_CODEC_HEVC) {
			if (pParams->bGPB == 1) {
				mfx_co3.GPB = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tGPB set: ON");
			} else if (pParams->bGPB == 0) {
				mfx_co3.GPB = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tGPB set: OFF");
			}
		}

		if (pParams->nGOPRefDist <= 1) {
			mfx_co3.PRefType = MFX_P_REF_PYRAMID;
			blog(LOG_INFO, "\tPRef set: Pyramid");
		} else {
			mfx_co3.PRefType = MFX_P_REF_SIMPLE;
			blog(LOG_INFO, "\tPRef set: Simple");
		}

		if (pParams->bAdaptiveMaxFrameSize == 1) {
			mfx_co3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveMaxFrameSize set: ON");
		} else if (pParams->bAdaptiveMaxFrameSize == 0) {
			mfx_co3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveMaxFrameSize set: OFF");
		}

		if (pParams->bAdaptiveCQM == 1) {
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveCQM set: ON");
		} else if (pParams->bAdaptiveCQM == 0) {
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveCQM set: OFF");
		}

		if (mfx_version.Major >= 2 && mfx_version.Minor >= 4) {

			if (pParams->bAdaptiveRef == 1) {
				mfx_co3.AdaptiveRef = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tAdaptiveRef set: ON");
			} else if (pParams->bAdaptiveRef == 0) {
				mfx_co3.AdaptiveRef = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveRef set: OFF");
			}

			if (pParams->bAdaptiveLTR == 1) {
				mfx_co3.AdaptiveLTR = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tAdaptiveLTR set: ON\n Warning! If you get an encoder error, please turn off AdaptiveLTR");
			} else if (pParams->bAdaptiveLTR == 0) {
				mfx_co3.AdaptiveLTR = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveLTR set: OFF");
			}
		}

		if (pParams->nWinBRCMaxAvgSize > 0) {
			mfx_co3.WinBRCMaxAvgKbps = pParams->nWinBRCMaxAvgSize;
			blog(LOG_INFO, "\tWinBRCMaxSize set: %d",
			     pParams->nWinBRCMaxAvgSize);
		}

		if (pParams->nWinBRCSize > 0) {
			mfx_co3.WinBRCSize = (mfxU16)(pParams->nFpsNum *
						      pParams->nWinBRCSize);
			blog(LOG_INFO, "\tWinBRCSize set: %d",
			     mfx_co3.WinBRCSize);
		}

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
		
		if (pParams->bWeightedPred == 1) {
			mfx_co3.WeightedPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO, "\tWeightedPred set: ON");

			mfx_co3.WeightedBiPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO, "\tWeightedBiPred set: ON");
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

		if (pParams->bDirectBiasAdjustment == 1) {
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: ON");
		} else if (pParams->bDirectBiasAdjustment == 0) {
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tDirectBiasAdjustment set: OFF");
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

		if (pParams->nMotionVectorsOverPicBoundaries == 1) {

			mfx_ExtMVOverPicBoundaries.StickBottom =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickLeft =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickRight =
				MFX_CODINGOPTION_OFF;
			mfx_ExtMVOverPicBoundaries.StickTop =
				MFX_CODINGOPTION_OFF;
		} else if (pParams->nMotionVectorsOverPicBoundaries == 0) {
			mfx_ExtMVOverPicBoundaries.StickBottom =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickLeft =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickRight =
				MFX_CODINGOPTION_ON;
			mfx_ExtMVOverPicBoundaries.StickTop =
				MFX_CODINGOPTION_ON;
		}
	}
	
	if (mfx_EncToolsConf_enable == 1 &&
	    (mfx_version.Major >= 2 && mfx_version.Minor >= 8) &&
	    pParams->bExtBRC == 1) {
		INIT_MFX_EXT_BUFFER(mfx_EncToolsConf, MFX_EXTBUFF_ENCTOOLS_CONFIG);

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

		mfx_EncToolsConf.AdaptiveMBQP = MFX_CODINGOPTION_ON;
		mfx_EncToolsConf.SaliencyMapHint = MFX_CODINGOPTION_ON;
		if (pParams->bLAExtBRC == 1) {
			mfx_EncToolsConf.BRC = MFX_CODINGOPTION_OFF;
		} else {
			mfx_EncToolsConf.BRC = MFX_CODINGOPTION_ON;
		}
		mfx_EncToolsConf.AdaptiveLTR =
			pParams->bAdaptiveLTR == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefP = pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefB =
			pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.BRCBufferHints = MFX_CODINGOPTION_ON;
			

		mfx_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_EncToolsConf);
	}
	//Wait next VPL release for support this
	if (mfx_version.Major >= 2 && mfx_version.Minor >= 9) {
		INIT_MFX_EXT_BUFFER(mfx_Tune, MFX_EXTBUFF_TUNE_ENCODE_QUALITY);

		switch ((int)pParams->nTuneQualityMode) {
		case 1:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_PSNR;
			blog(LOG_INFO, "\tTuneQualityMode set: PSNR");
			break;
		case 2:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_SSIM;
			blog(LOG_INFO, "\tTuneQualityMode set: SSIM");
			break;
		case 3:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
			blog(LOG_INFO, "\tTuneQualityMode set: MS SSIM");
			break;
		case 4:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_VMAF;
			blog(LOG_INFO, "\tTuneQualityMode set: VMAF");
			break;
		case 5:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
			blog(LOG_INFO, "\tTuneQualityMode set: PERCEPTUAL");
			break;
		default:
			mfx_Tune.TuneQuality = MFX_ENCODE_TUNE_DEFAULT;
			break;
		}

		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Tune);
	}

	if (codec == QSV_CODEC_HEVC) {
		if ((pParams->nWidth & 15) || (pParams->nHeight & 15)) {
			INIT_MFX_EXT_BUFFER(mfx_ExtHEVCParam, MFX_EXTBUFF_HEVC_PARAM);

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
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
			blog(LOG_INFO, "\tDenoise set: AUTO | BDRATE | PRE ENCODE");
			break;
		case 2:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
			blog(LOG_INFO, "\tDenoise set: AUTO | ADJUST | POST ENCODE");
			break;
		case 3:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
			blog(LOG_INFO, "\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
			break;
		case 4:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
			mfx_VPPDenoise.Strength = (mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO,
			     "\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE", mfx_VPPDenoise.Strength);
			break;
		case 5:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
			mfx_VPPDenoise.Strength = (mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO, "\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE");
			break;
		default:
			mfx_VPPDenoise.Mode = MFX_DENOISE_MODE_DEFAULT;
			blog(LOG_INFO, "\tDenoise set: DEFAULT");
			break;
		}
		
		mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPDenoise);
	}

	//TODO: Add support VPP filters
	//memset(&mfx_VPPScaling, 0, sizeof(mfxExtVPPScaling));
	//mfx_VPPScaling.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;

	//mfx_VPPScaling.ScalingMode = MFX_SCALING_MODE_QUALITY;
	//mfx_VPPScaling.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

	//extendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPScaling);

	if (codec == QSV_CODEC_AV1) {
		if (mfx_version.Major >= 2 && mfx_version.Minor >= 5) {
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1BitstreamParam, MFX_EXTBUFF_AV1_BITSTREAM_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1ResolutionParam, MFX_EXTBUFF_AV1_RESOLUTION_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_ExtAV1TileParam, MFX_EXTBUFF_AV1_TILE_PARAM);

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

	INIT_MFX_EXT_BUFFER(mfx_ExtVideoSignalInfo, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

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

	if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) && pParams->MaxContentLightLevel > 0) {
		INIT_MFX_EXT_BUFFER(mfx_ExtMasteringDisplayColourVolume, MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME);
		INIT_MFX_EXT_BUFFER(mfx_ExtContentLightLevelInfo, MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO);

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

	mfx_EncParams.IOPattern =
		MFX_IOPATTERN_IN_VIDEO_MEMORY;

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
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_CBR:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		break;
	default:

		break;
	}

	return true;
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder()
{
	return mfx_VideoEnc->Reset(&mfx_EncParams);
}

mfxStatus QSV_VPL_Encoder_Internal::AllocateSurfaces()
{
	// Query number of required surfaces for encoder
	mfxStatus sts = mfx_VideoEnc->QueryIOSurf(&mfx_EncParams,
						  &mfx_FrameAllocRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_FrameAllocRequest.Type |= WILL_WRITE;

	// SNB hack. On some SNB, it seems to require more surfaces
	mfx_FrameAllocRequest.NumFrameSuggested +=
		mfx_EncParams.AsyncDepth + 100;

	// Allocate required surfaces
	sts = mfx_FrameAllocator.Alloc(mfx_FrameAllocator.pthis,
				       &mfx_FrameAllocRequest,
				       &mfx_FrameAllocResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mU16_SurfNum = mfx_FrameAllocResponse.NumFrameActual;

	mfx_FrameSurf = new mfxFrameSurface1 *[mU16_SurfNum];
	MSDK_CHECK_POINTER(mfx_FrameSurf, MFX_ERR_MEMORY_ALLOC);
	
	for (int i = 0; i < mU16_SurfNum; i++) {
		mfx_FrameSurf[i] = new mfxFrameSurface1;
		memset(mfx_FrameSurf[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(mfx_FrameSurf[i]->Info),
		       &(mfx_EncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		mfx_FrameSurf[i]->Data.MemId = mfx_FrameAllocResponse.mids[i];
	}
	blog(LOG_INFO, "\tSurface count:     %d", mU16_SurfNum);

	return sts;
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
		INIT_MFX_EXT_BUFFER(mfx_ExtCOVPS, MFX_EXTBUFF_CODING_OPTION_VPS);

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

void QSV_VPL_Encoder_Internal::GetVpsSpsPps(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
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

	for (int i = 0; i < mU16_TaskPool; i++) {
		t_TaskPool[i].mfxBS.MaxLength =
			mfx_EncParams.mfx.BufferSizeInKB * 1000 * mfx_EncParams.mfx.BRCParamMultiplier * 8;
		t_TaskPool[i].mfxBS.Data =
			new mfxU8[t_TaskPool[i].mfxBS.MaxLength];
		t_TaskPool[i].mfxBS.DataOffset = 0;
		t_TaskPool[i].mfxBS.DataLength = 0;

		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS.Data,
				   MFX_ERR_MEMORY_ALLOC);
	}

	mfx_Bitstream.MaxLength = mfx_EncParams.mfx.BufferSizeInKB * 1000 * mfx_EncParams.mfx.BRCParamMultiplier * 8;
	mfx_Bitstream.Data = new mfxU8[mfx_Bitstream.MaxLength];
	mfx_Bitstream.DataOffset = 0;
	mfx_Bitstream.DataLength = 0;
	
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

int QSV_VPL_Encoder_Internal::GetFreeTaskIndex(Task *t_TaskPool,
					       mfxU16 nPoolSize)
{
	if (t_TaskPool)
		for (int i = 0; i < nPoolSize; i++)
			if (!t_TaskPool[i].syncp)
				return i;
	return MFX_ERR_NOT_FOUND;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(uint64_t ts, uint8_t *pDataY,
					   uint8_t *pDataUV, uint32_t strideY,
					   uint32_t strideUV,
					   mfxBitstream **pBS)
{
	mfxStatus sts = MFX_ERR_NONE;
	*pBS = NULL;
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mU16_TaskPool);

#if 0
	info("MSDK Encode:\n"
		"\tTaskIndex: %d",
		nTaskIdx);
#endif

	int nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mU16_SurfNum);
#if 0
	info("MSDK Encode:\n"
		"\tnSurfIdx: %d",
		nSurfIdx);
#endif

	while (MFX_ERR_NOT_FOUND == nTaskIdx || MFX_ERR_NOT_FOUND == nSurfIdx) {
		// No more free tasks or surfaces, need to sync
		sts = MFXVideoCORE_SyncOperation(
			mfx_session, t_TaskPool[n_FirstSyncTask].syncp, 1500000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = NULL;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mU16_TaskPool;
		*pBS = &mfx_Bitstream;

#if 0
		info("VPL Encode:\n"
			"\tnew FirstSyncTask: %d\n"
			"\tTaskIndex:         %d",
			n_FirstSyncTask,
			nTaskIdx);
#endif

		nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mU16_SurfNum);
#if 0
		info("VPL Encode:\n"
			"\tnSurfIdx: %d",
			nSurfIdx);
#endif
	}

	mfxFrameSurface1 *pSurface = mfx_FrameSurf[nSurfIdx];
	sts = mfx_FrameAllocator.Lock(mfx_FrameAllocator.pthis,
				      pSurface->Data.MemId, &(pSurface->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = (pSurface->Info.FourCC == MFX_FOURCC_P010)
		      ? LoadP010(pSurface, pDataY, pDataUV, strideY, strideUV)
		      : LoadNV12(pSurface, pDataY, pDataUV, strideY, strideUV);

	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	pSurface->Data.TimeStamp = ts;

	sts = mfx_FrameAllocator.Unlock(mfx_FrameAllocator.pthis,
					pSurface->Data.MemId,
					&(pSurface->Data));
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	/*MFXVideoENCODE_Init(mfx_session, &mfx_EncParams);*/
	for (;;) {
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoEnc->EncodeFrameAsync(
			&mfx_EncControl, pSurface, &t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			//blog(LOG_INFO, "Encode error: Trying repeat without output");

			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts) {
				MSDK_SLEEP(
					10); // Wait if device is busy, then repeat the same call
				//MFXVideoCORE_SyncOperation(
				//	mfx_session,
				//	t_TaskPool[n_FirstSyncTask].syncp,
				//	1500000);
			}
		} else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			//blog(LOG_INFO, "Encode error: Trying repeat with output");
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
	*pBS = NULL;
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mU16_TaskPool);
	int nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mU16_SurfNum);

	while (MFX_ERR_NOT_FOUND == nTaskIdx || MFX_ERR_NOT_FOUND == nSurfIdx) {
		// No more free tasks or surfaces, need to sync
		sts = MFXVideoCORE_SyncOperation(
			mfx_session, t_TaskPool[n_FirstSyncTask].syncp,
			1500000);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = NULL;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mU16_TaskPool;
		*pBS = &mfx_Bitstream;

		nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mU16_SurfNum);
	}

	mfxFrameSurface1 *pSurface = mfx_FrameSurf[nSurfIdx];
	//copy to default surface directly
	pSurface->Data.TimeStamp = ts;
	sts = simple_copytex(mfx_FrameAllocator.pthis, pSurface->Data.MemId,
			     tex_handle, lock_key, next_key);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	/*MFXVideoENCODE_Init(mfx_session, &mfx_EncParams);*/
	for (;;) {
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoEnc->EncodeFrameAsync(
			&mfx_EncControl, pSurface, &t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		/*pSurface->FrameInterface->Release(pSurface);*/

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(
					10); // Wait if device is busy, then repeat the same call
			//MFXVideoCORE_SyncOperation(
			//	mfx_session, t_TaskPool[n_FirstSyncTask].syncp,
			//	1500000);
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
			mfx_session, t_TaskPool[n_FirstSyncTask].syncp, 15000000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		t_TaskPool[n_FirstSyncTask].syncp = NULL;
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
		mfx_VideoEnc = NULL;
	}

	mfx_FrameAllocator.Free(mfx_FrameAllocator.pthis, &mfx_FrameAllocResponse);

	if (mfx_FrameSurf) {
		for (int i = 0; i < mU16_SurfNum; i++) {
			delete mfx_FrameSurf[i]->Data.Y;
			delete mfx_FrameSurf[i];
		}
		MSDK_SAFE_DELETE_ARRAY(mfx_FrameSurf);
	}

	if (t_TaskPool) {
		for (int i = 0; i < mU16_TaskPool; i++)
			delete t_TaskPool[i].mfxBS.Data;
		MSDK_SAFE_DELETE_ARRAY(t_TaskPool);
	}

	if (mfx_Bitstream.Data) {
		delete[] mfx_Bitstream.Data;
		mfx_Bitstream.Data = NULL;
	}

	if (sts >= MFX_ERR_NONE) {
		g_numEncodersOpen--;
	}

	if (g_numEncodersOpen <= 0) {
		Release();
		g_DX_Handle = NULL;
	}

	if (mfx_session) {
		MFXClose(mfx_session);
		MFXDispReleaseImplDescription(mfx_loader, NULL);
		MFXUnload(mfx_loader);
		mfx_session = NULL;
		mfx_loader = NULL;
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
