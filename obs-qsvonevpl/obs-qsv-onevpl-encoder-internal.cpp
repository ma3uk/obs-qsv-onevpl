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
	: mfx_VideoENC(nullptr),
	  //mfx_VideoVPP(nullptr),
	  SPS_BufSize(1024),
	  PPS_BufSize(1024),
	  VPS_BufSize(),
	  mfx_TaskPool(0),
	  g_numEncodersOpen(0),
	  t_TaskPool(nullptr),
	  n_TaskIdx(0),
	  n_FirstSyncTask(0),
	  mfx_Bitstream(),
	  b_isDGPU(isDGPU),
	  mfx_Ext_CO(),
	  mfx_Ext_CO2(),
	  mfx_Ext_CO3(),
	  mfx_ENC_ExtendedBuffers(),
	  mfx_ENCCtrl_ExtendedBuffers(),
	  //mfx_VPP_ExtendedBuffers(),
	  mfx_ENC_Params(),
	  mfx_ENCCtrl_Params(),
	  mfx_Ext_VPPDenoise(),
	  mfx_Ext_AVCRoundingOffset(),
	  //mfx_Tune(),
	  mfx_EncToolsConf(),
	  mfx_Ext_AV1BitstreamParam(),
	  mfx_Ext_AV1ResolutionParam(),
	  mfx_Ext_AV1TileParam(),
	  mfx_Ext_HEVCParam{},
	  mfx_Ext_VideoSignalInfo{},
	  mfx_Ext_ChromaLocInfo{},
	  mfx_Ext_MasteringDisplayColourVolume{},
	  mfx_Ext_ContentLightLevelInfo{},
	  mfx_Ext_CO_VPS{},
	  mfx_Ext_CO_SPSPPS(),
	  mfx_Ext_ResetOption(),
	  mfx_Platform(),
	  mfx_HyperModeParam(),
	  mfx_CO_DDI()
{
	blog(LOG_INFO, "\tSelected QuickSync oneVPL implementation\n");
	mfxIMPL tempImpl = MFX_IMPL_VIA_D3D9;
	mfx_Loader = MFXLoad();
	mfxStatus sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
	if (sts == MFX_ERR_NONE) {
		sts = MFXQueryVersion(mfx_Session, &version);
		if (sts == MFX_ERR_NONE) {
			mfx_Version = version;
			sts = MFXQueryIMPL(mfx_Session, &tempImpl);
			if (sts == MFX_ERR_NONE) {
				mfx_Impl = MFX_IMPL_VIA_D3D9;
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

mfxStatus QSV_VPL_Encoder_Internal::Initialize(mfxIMPL impl, mfxVersion ver,
					       int deviceNum)
{
	mfxConfig mfx_cfg[10] = {};
	mfxVariant mfx_variant[10] = {};
	mfxStatus sts = MFX_ERR_NONE;

	// Initialize VPL Session
	mfx_Loader = MFXLoad();
	mfx_cfg[0] = MFXCreateConfig(mfx_Loader);
	mfx_variant[0].Type = MFX_VARIANT_TYPE_U32;
	mfx_variant[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
	MFXSetConfigFilterProperty(
		mfx_cfg[0], (mfxU8 *)"mfxImplDescription.Impl.mfxImplType",
		mfx_variant[0]);

	mfx_cfg[1] = MFXCreateConfig(mfx_Loader);
	mfx_variant[1].Type = MFX_VARIANT_TYPE_U32;
	mfx_variant[1].Data.U32 = MFX_ACCEL_MODE_VIA_D3D9;
	MFXSetConfigFilterProperty(
		mfx_cfg[1],
		(mfxU8 *)"mfxImplDescription.mfxAccelerationModeDescription.Mode",
		mfx_variant[1]);

	//mfx_cfg[2] = MFXCreateConfig(mfx_Loader);
	//mfx_variant[2].Type = MFX_VARIANT_TYPE_U16;
	//mfx_variant[2].Data.U16 = MFX_GPUCOPY_ON;
	//MFXSetConfigFilterProperty(mfx_cfg[2],
	//			   (mfxU8 *)"mfxInitializationParam.DeviceCopy",
	//			   mfx_variant[2]);

	sts = MFXCreateSession(mfx_Loader, (deviceNum >= 0) ? deviceNum : 0,
			       &mfx_Session);

	MFXQueryIMPL(mfx_Session, &mfx_Impl);

	MFXSetPriority(mfx_Session, MFX_PRIORITY_NORMAL);
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
	mfxStatus sts = Initialize(mfx_Impl, mfx_Version, pParams->nDeviceNum);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_VideoENC = new MFXVideoENCODE(mfx_Session);

	sts = InitENCParams(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitENCCtrlParams(pParams, codec);

	sts = mfx_VideoENC->Query(&mfx_ENC_Params, &mfx_ENC_Params);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfx_VideoENC->Init(&mfx_ENC_Params);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = GetVideoParam(codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//mfx_VideoVPP = new MFXVideoVPP(mfx_Session);
	//sts = InitVPPParams(pParams, codec);
	//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//sts = mfx_VideoVPP->Init(&mfx_VPP_Params);
	//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitBitstream();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (sts >= MFX_ERR_NONE) {
		g_numEncodersOpen++;
	} else {
		blog(LOG_INFO, "\tOpen encoder: [ERROR]");
		mfx_VideoENC->Close();
		/*mfx_VideoVPP->Close();*/
		MFXClose(mfx_Session);
		MFXUnload(mfx_Loader);
	}

	return sts;
}

//mfxStatus QSV_VPL_Encoder_Internal::InitVPPParams(qsv_param_t *pParams,
//						  enum qsv_codec codec)
//{
//	mfx_VPP_Params.vpp.In.FrameRateExtN = pParams->nFpsNum;
//	mfx_VPP_Params.vpp.In.FrameRateExtD = pParams->nFpsDen;
//
//	if (pParams->video_fmt_10bit) {
//		mfx_VPP_Params.vpp.In.FourCC = MFX_FOURCC_P010;
//		mfx_VPP_Params.vpp.In.BitDepthChroma = 10;
//		mfx_VPP_Params.vpp.In.BitDepthLuma = 10;
//		mfx_VPP_Params.vpp.In.Shift = 1;
//
//	} else {
//		mfx_VPP_Params.vpp.In.FourCC = MFX_FOURCC_NV12;
//		mfx_VPP_Params.vpp.In.Shift = 0;
//		mfx_VPP_Params.vpp.In.BitDepthChroma = 8;
//		mfx_VPP_Params.vpp.In.BitDepthLuma = 8;
//	}
//
//	// Width must be a multiple of 16
//	// Height must be a multiple of 16 in case of frame picture and a
//	// multiple of 32 in case of field picture
//
//	mfx_VPP_Params.vpp.In.Width = MSDK_ALIGN16(pParams->nWidth);
//	mfx_VPP_Params.vpp.In.Height = MSDK_ALIGN16(pParams->nHeight);
//	mfx_VPP_Params.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
//	mfx_VPP_Params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
//	mfx_VPP_Params.vpp.In.CropX = 0;
//	mfx_VPP_Params.vpp.In.CropY = 0;
//	mfx_VPP_Params.vpp.In.CropW = pParams->nWidth;
//	mfx_VPP_Params.vpp.In.CropH = pParams->nHeight;
//
//	if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
//		INIT_MFX_EXT_BUFFER(mfx_Ext_VPPDenoise,
//				    MFX_EXTBUFF_VPP_DENOISE2);
//
//		switch ((int)pParams->nDenoiseMode) {
//		case 1:
//			mfx_Ext_VPPDenoise.Mode =
//				MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
//			blog(LOG_INFO,
//			     "\tDenoise set: AUTO | BDRATE | PRE ENCODE");
//			break;
//		case 2:
//			mfx_Ext_VPPDenoise.Mode =
//				MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
//			blog(LOG_INFO,
//			     "\tDenoise set: AUTO | ADJUST | POST ENCODE");
//			break;
//		case 3:
//			mfx_Ext_VPPDenoise.Mode =
//				MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
//			blog(LOG_INFO,
//			     "\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
//			break;
//		case 4:
//			mfx_Ext_VPPDenoise.Mode =
//				MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
//			mfx_Ext_VPPDenoise.Strength =
//				(mfxU16)pParams->nDenoiseStrength;
//			blog(LOG_INFO,
//			     "\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
//			     mfx_Ext_VPPDenoise.Strength);
//			break;
//		case 5:
//			mfx_Ext_VPPDenoise.Mode =
//				MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
//			mfx_Ext_VPPDenoise.Strength =
//				(mfxU16)pParams->nDenoiseStrength;
//			blog(LOG_INFO,
//			     "\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
//			     mfx_Ext_VPPDenoise.Strength);
//			break;
//		default:
//			mfx_Ext_VPPDenoise.Mode = MFX_DENOISE_MODE_DEFAULT;
//			blog(LOG_INFO, "\tDenoise set: DEFAULT");
//			break;
//		}
//
//		mfx_VPP_ExtendedBuffers.push_back(
//			(mfxExtBuffer *)&mfx_Ext_VPPDenoise);
//	}
// 	//TODO: Add support VPP filters
//	INIT_MFX_EXT_BUFFER(mfx_VPPScaling, MFX_EXTBUFF_VPP_SCALING);

//	mfx_VPPScaling.ScalingMode = MFX_SCALING_MODE_QUALITY;
//	mfx_VPPScaling.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

//	mfx_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_VPPScaling);
//
//	mfx_VPP_Params.ExtParam = mfx_VPP_ExtendedBuffers.data();
//	mfx_VPP_Params.NumExtParam = (mfxU16)mfx_VPP_ExtendedBuffers.size();
//
//	mfx_VPP_Params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY |
//				   MFX_IOPATTERN_OUT_VIDEO_MEMORY;
//	mfxVideoParam validVPP_Params = {0};
//	memcpy(&validVPP_Params, &mfx_VPP_Params, sizeof(validVPP_Params));
//	mfxStatus sts = mfx_VideoVPP->Query(&mfx_VPP_Params, &validVPP_Params);
//	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
//		blog(LOG_INFO, "\tVPP UNSUPPORTED");
//	}
//
//	return MFX_ERR_NONE;
//}

mfxStatus QSV_VPL_Encoder_Internal::InitENCCtrlParams(qsv_param_t *pParams,
						      enum qsv_codec codec)
{
	if (codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_AVCRoundingOffset,
				    MFX_EXTBUFF_AVC_ROUNDING_OFFSET);
		mfx_Ext_AVCRoundingOffset.EnableRoundingIntra =
			MFX_CODINGOPTION_ON;
		mfx_Ext_AVCRoundingOffset.RoundingOffsetIntra = 0;
		mfx_Ext_AVCRoundingOffset.EnableRoundingInter =
			MFX_CODINGOPTION_ON;
		mfx_Ext_AVCRoundingOffset.RoundingOffsetInter = 0;
		mfx_ENCCtrl_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_AVCRoundingOffset);
	}

	//if (codec == QSV_CODEC_AVC) {
	//	INIT_MFX_EXT_BUFFER(mfx_Ext_AVCRefListCtrl,
	//			    MFX_EXTBUFF_AVC_REFLIST_CTRL);
	//	if (pParams->nNumRefFrame > 0) {

	//		mfx_Ext_AVCRefListCtrl.NumRefIdxL0Active =
	//			(mfxU16)(pParams->nNumRefFrame);
	//		mfx_Ext_AVCRefListCtrl.NumRefIdxL1Active =
	//			(mfxU16)(pParams->nNumRefFrame);
	//		mfx_Ext_AVCRefListCtrl.ApplyLongTermIdx = 0;
	//	}
	//	mfx_ENCCtrl_ExtendedBuffers.push_back(
	//		(mfxExtBuffer *)&mfx_Ext_AVCRefListCtrl);
	//}

	//AV1 have bug with EncTools initialization, waiting pending from intel
	if ((mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
	    codec != QSV_CODEC_AV1 && pParams->bCPUEncTools != 0) {
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

		mfx_EncToolsConf.AdaptiveMBQP = MFX_CODINGOPTION_ON;

		mfx_EncToolsConf.AdaptiveLTR =
			pParams->bAdaptiveLTR == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefP =
			pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;
		mfx_EncToolsConf.AdaptiveRefB =
			pParams->bAdaptiveRef == 1 ? MFX_CODINGOPTION_ON
						   : MFX_CODINGOPTION_UNKNOWN;

		mfx_EncToolsConf.BRCBufferHints = MFX_CODINGOPTION_ON;

		mfx_EncToolsConf.BRC = MFX_CODINGOPTION_ON;

		mfx_ENCCtrl_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_EncToolsConf);
	}

	mfx_ENCCtrl_Params.ExtParam = mfx_ENCCtrl_ExtendedBuffers.data();
	mfx_ENCCtrl_Params.NumExtParam =
		(mfxU16)mfx_ENCCtrl_ExtendedBuffers.size();

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::InitENCParams(qsv_param_t *pParams,
						  enum qsv_codec codec)
{
	int mfx_Ext_CO_enable = 1;
	int mfx_Ext_CO2_enable = 1;
	int mfx_Ext_CO3_enable = 1;
	mfx_ENC_Params.mfx.NumThread = 64;
	mfx_ENC_Params.mfx.RestartInterval = 1;

	switch (codec) {
	case QSV_CODEC_AV1:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_AV1;
		break;
	case QSV_CODEC_HEVC:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_HEVC;
		break;
	case QSV_CODEC_AVC:
	default:
		mfx_ENC_Params.mfx.CodecId = MFX_CODEC_AVC;
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

	if (pParams->video_fmt_10bit) {
		mfx_ENC_Params.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
		mfx_ENC_Params.vpp.In.FourCC = MFX_FOURCC_P010;

		mfx_ENC_Params.mfx.FrameInfo.BitDepthChroma = 10;
		mfx_ENC_Params.vpp.In.BitDepthChroma = 10;

		mfx_ENC_Params.mfx.FrameInfo.BitDepthLuma = 10;
		mfx_ENC_Params.vpp.In.BitDepthLuma = 10;

		mfx_ENC_Params.mfx.FrameInfo.Shift = 1;
		mfx_ENC_Params.vpp.In.Shift = 1;

	} else {
		mfx_ENC_Params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
		mfx_ENC_Params.vpp.In.FourCC = MFX_FOURCC_NV12;

		mfx_ENC_Params.mfx.FrameInfo.Shift = 0;
		mfx_ENC_Params.vpp.In.Shift = 0;

		mfx_ENC_Params.mfx.FrameInfo.BitDepthChroma = 8;
		mfx_ENC_Params.vpp.In.BitDepthChroma = 8;

		mfx_ENC_Params.mfx.FrameInfo.BitDepthLuma = 8;
		mfx_ENC_Params.vpp.In.BitDepthLuma = 8;
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture

	mfx_ENC_Params.mfx.FrameInfo.Width = MSDK_ALIGN16(pParams->nWidth);
	mfx_ENC_Params.vpp.In.Width = MSDK_ALIGN16(pParams->nWidth);

	mfx_ENC_Params.mfx.FrameInfo.Height = MSDK_ALIGN16(pParams->nHeight);
	mfx_ENC_Params.vpp.In.Height = MSDK_ALIGN16(pParams->nHeight);

	mfx_ENC_Params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_ENC_Params.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

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
	if ((qsv_platform >= QSV_CPU_PLATFORM_ICL ||
	     qsv_platform == QSV_CPU_PLATFORM_UNKNOWN) &&
	    (pParams->nGOPRefDist == 1) &&
	    (mfx_Version.Major >= 2 && mfx_Version.Minor >= 0)) {
		mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_ON;
		blog(LOG_INFO, "\tLowpower set: ON");
	}

	if (mfx_ENC_Params.mfx.LowPower == MFX_CODINGOPTION_ON) {
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

	mfx_ENC_Params.mfx.RateControlMethod = (mfxU16)pParams->RateControl;

	mfx_ENC_Params.mfx.BRCParamMultiplier = 1;
	switch (pParams->RateControl) {
	case MFX_RATECONTROL_LA_HRD:
	case MFX_RATECONTROL_CBR:
		mfx_ENC_Params.mfx.TargetKbps = (mfxU16)pParams->nTargetBitRate;
		if (pParams->bLAExtBRC == 1) {
			mfx_ENC_Params.mfx.MaxKbps =
				(mfxU16)pParams->nTargetBitRate * 2;
		}
		mfx_ENC_Params.mfx.BufferSizeInKB =
			(mfxU16)(mfx_ENC_Params.mfx.TargetKbps / 8);

		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB);
		break;
	case MFX_RATECONTROL_VBR:
	case MFX_RATECONTROL_VCM:
		mfx_ENC_Params.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_ENC_Params.mfx.MaxKbps = pParams->nMaxBitRate;
		blog(LOG_INFO, "\tBufferAdapt set to: %d",
		     mfx_Platform.MediaAdapterType);
		mfx_ENC_Params.mfx.BufferSizeInKB =
			(mfxU16)(mfx_ENC_Params.mfx.MaxKbps / 8);
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB);
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
			(mfxU16)(mfx_ENC_Params.mfx.TargetKbps / 8);
		if (pParams->bCustomBufferSize == true &&
		    pParams->nBufferSize > 0) {
			mfx_ENC_Params.mfx.BufferSizeInKB =
				(mfxU16)pParams->nBufferSize;
			blog(LOG_INFO, "\tCustomBufferSize set: ON");
		}
		blog(LOG_INFO, "\tBufferSize set to: %d",
		     mfx_ENC_Params.mfx.BufferSizeInKB);
		break;
	}

	mfx_ENC_Params.AsyncDepth = (mfxU16)pParams->nAsyncDepth;

	if (pParams->nKeyIntSec == 0) {
		mfx_ENC_Params.mfx.GopPicSize =
			(mfxU16)((pParams->nFpsNum + pParams->nFpsDen - 1) /
				 pParams->nFpsDen) *
			10;
	} else {
		mfx_ENC_Params.mfx.GopPicSize =
			(mfxU16)(pParams->nKeyIntSec * pParams->nFpsNum /
				 (float)pParams->nFpsDen);
	}
	blog(LOG_INFO, "\tGopPicSize set to: %d",
	     mfx_ENC_Params.mfx.GopPicSize);

	if (pParams->bGopOptFlag == 1) {
		mfx_ENC_Params.mfx.GopOptFlag = 0x00;
		blog(LOG_INFO, "\tGopOptFlag set: OPEN");
		if (mfx_ENC_Params.mfx.CodecId == MFX_CODEC_HEVC) {
			mfx_ENC_Params.mfx.IdrInterval =
				1 + ((pParams->nFpsNum + pParams->nFpsDen - 1) /
				     pParams->nFpsDen) *
					    600 / mfx_ENC_Params.mfx.GopPicSize;
			mfx_ENC_Params.mfx.NumSlice = 0;
		} else if (mfx_ENC_Params.mfx.CodecId == MFX_CODEC_AVC) {
			mfx_ENC_Params.mfx.IdrInterval =
				((pParams->nFpsNum + pParams->nFpsDen - 1) /
				 pParams->nFpsDen) *
				600 / mfx_ENC_Params.mfx.GopPicSize;
			mfx_ENC_Params.mfx.NumSlice = 1;
		} else {
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
		}
	} else if (pParams->bGopOptFlag == 0) {
		mfx_ENC_Params.mfx.GopOptFlag = MFX_GOP_CLOSED;
		blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
		if (mfx_ENC_Params.mfx.CodecId == MFX_CODEC_HEVC) {
			mfx_ENC_Params.mfx.IdrInterval = 1;
			mfx_ENC_Params.mfx.NumSlice = 0;
		} else if (mfx_ENC_Params.mfx.CodecId == MFX_CODEC_AVC) {
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
		} else {
			mfx_ENC_Params.mfx.IdrInterval = 0;
			mfx_ENC_Params.mfx.NumSlice = 1;
		}
	}

	if (codec == QSV_CODEC_AV1) {
		mfx_ENC_Params.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_ENC_Params.mfx.GopRefDist > 33) {
			mfx_ENC_Params.mfx.GopRefDist = 33;
		} else if (mfx_ENC_Params.mfx.GopRefDist > 16 &&
			   pParams->bExtBRC == 1) {
			mfx_ENC_Params.mfx.GopRefDist = 16;
			blog(LOG_INFO,
			     "\tExtBRC does not support GOPRefDist greater than 16.");
		}
		if (pParams->bExtBRC == 1 &&
		    (mfx_ENC_Params.mfx.GopRefDist != 1 &&
		     mfx_ENC_Params.mfx.GopRefDist != 2 &&
		     mfx_ENC_Params.mfx.GopRefDist != 4 &&
		     mfx_ENC_Params.mfx.GopRefDist != 8 &&
		     mfx_ENC_Params.mfx.GopRefDist != 16)) {
			mfx_ENC_Params.mfx.GopRefDist = 4;
			blog(LOG_INFO,
			     "\tExtBRC only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
		} else {
			blog(LOG_INFO, "\tGOPRefDist set to:     %d",
			     pParams->nGOPRefDist);
		}
	} else {
		mfx_ENC_Params.mfx.GopRefDist = pParams->nGOPRefDist;
		if (mfx_ENC_Params.mfx.GopRefDist > 16) {
			mfx_ENC_Params.mfx.GopRefDist = 16;
		}
		if (pParams->bExtBRC == 1 &&
		    (mfx_ENC_Params.mfx.GopRefDist != 1 &&
		     mfx_ENC_Params.mfx.GopRefDist != 2 &&
		     mfx_ENC_Params.mfx.GopRefDist != 4 &&
		     mfx_ENC_Params.mfx.GopRefDist != 8 &&
		     mfx_ENC_Params.mfx.GopRefDist != 16)) {
			mfx_ENC_Params.mfx.GopRefDist = 4;
			blog(LOG_INFO,
			     "\tExtBRC only works with GOPRefDist equal to 1, 2, 4, 8, 16. GOPRefDist is set to 4.");
		}
		blog(LOG_INFO, "\tGOPRefDist set to:     %d",
		     (int)mfx_ENC_Params.mfx.GopRefDist);
	}

	if (mfx_Ext_CO_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO, MFX_EXTBUFF_CODING_OPTION);

		mfx_Ext_CO.CAVLC = MFX_CODINGOPTION_OFF;

		mfx_Ext_CO.RefPicMarkRep = MFX_CODINGOPTION_ON;
		mfx_Ext_CO.ResetRefList = MFX_CODINGOPTION_OFF;
		/*mfx_Ext_CO.FieldOutput = MFX_CODINGOPTION_ON;*/
		mfx_Ext_CO.IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_Ext_CO.InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_Ext_CO.MVPrecision = MFX_MVPRECISION_QUARTERPEL;

		mfx_Ext_CO.MaxDecFrameBuffering =
			max((pParams->nNumRefFrame + 1), pParams->nLADepth);

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

		switch ((int)pParams->bVuiNalHrd) {
		case 0:
			mfx_Ext_CO.VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
			mfx_Ext_CO.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: OFF");
			break;
		case 1:
			mfx_Ext_CO.VuiVclHrdParameters = MFX_CODINGOPTION_ON;
			mfx_Ext_CO.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tVuiNalHrdParameters set: ON");
			break;
		default:
			blog(LOG_INFO, "\tVuiNalHrdParameters set: AUTO");
			break;
		}

		switch ((int)pParams->bNalHrdConformance) {
		case 0:
			mfx_Ext_CO.NalHrdConformance = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tNalHrdConformance set: OFF");
			break;
		case 1:
			mfx_Ext_CO.NalHrdConformance = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tNalHrdConformance set: ON");
			break;
		default:
			blog(LOG_INFO, "\tNalHrdConformance set: AUTO");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO);
	}

	if (1 && (mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
	    pParams->bCPUEncTools == 1) {
		INIT_MFX_EXT_BUFFER(mfx_EncToolsConf,
				    MFX_EXTBUFF_ENCTOOLS_CONFIG);

		mfx_EncToolsConf.AdaptiveI = /*pParams->bAdaptiveI == 1
						     ? */
			MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptiveB = /*pParams->bAdaptiveB == 1
						     ? */
			MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.SceneChange =
			/*(pParams->bAdaptiveB == 1 || pParams->bAdaptiveI == 1)
				? */
			MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptivePyramidQuantP =
			/*pParams->bAdaptiveI == 1 ? */ MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptivePyramidQuantB =
			/*pParams->bAdaptiveB == 1 ? */ MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptiveQuantMatrices =
			/*pParams->bAdaptiveCQM == 1 ? */ MFX_CODINGOPTION_ON
			/* : MFX_CODINGOPTION_UNKNOWN*/;

		mfx_EncToolsConf.AdaptiveMBQP = MFX_CODINGOPTION_ON;

		mfx_EncToolsConf.AdaptiveLTR =
			/*pParams->bAdaptiveLTR == 1 ? */ MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptiveRefP =
			/*pParams->bAdaptiveRef == 1 ? */ MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		mfx_EncToolsConf.AdaptiveRefB =
			/*pParams->bAdaptiveRef == 1 ? */ MFX_CODINGOPTION_ON
			/*: MFX_CODINGOPTION_UNKNOWN*/;
		/*if (pParams->bCPUBufferHints == 1) {*/

		mfx_EncToolsConf.BRCBufferHints = MFX_CODINGOPTION_ON;
		/*}*/
		/*if (pParams->bCPUBRCControl == 1) {*/
		mfx_EncToolsConf.BRC = MFX_CODINGOPTION_ON;
		/*}*/
		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_EncToolsConf);
	}

	if (mfx_Ext_CO2_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO2, MFX_EXTBUFF_CODING_OPTION2);
		/*mfx_Ext_CO2.DisableVUI = MFX_CODINGOPTION_ON;*/
		mfx_Ext_CO2.FixedFrameRate = MFX_CODINGOPTION_ON;
		//mfx_Ext_CO2.SkipFrame = MFX_SKIPFRAME_BRC_ONLY;

		if (pParams->bIntraRefEncoding == 1) {
			mfx_Ext_CO2.EnableMAD = MFX_CODINGOPTION_ON;
			switch ((int)pParams->nIntraRefType) {
			default:
			case 0:
				mfx_Ext_CO2.IntRefType = MFX_REFRESH_VERTICAL;
				blog(LOG_INFO, "\tIntraRefType set: VERTICAL");
				break;
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

		if (pParams->bExtBRC == 1 && pParams->bLAExtBRC == 1) {
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:     %d",
			     pParams->nLADepth);
		} else if (pParams->RateControl == MFX_RATECONTROL_LA_ICQ ||
			   pParams->RateControl == MFX_RATECONTROL_LA ||
			   pParams->RateControl == MFX_RATECONTROL_LA_HRD ||
			   (pParams->bLowPower == 1 ||
			    mfx_ENC_Params.mfx.LowPower ==
				    MFX_CODINGOPTION_ON)) {
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
			blog(LOG_INFO, "\tLookahead set to:%d",
			     pParams->nLADepth);
		}

		if (pParams->bMBBRC == 1) {
			if ((pParams->RateControl == MFX_RATECONTROL_ICQ) ||
			    (pParams->RateControl == MFX_RATECONTROL_CQP) ||
			    (pParams->RateControl == MFX_RATECONTROL_CBR)) {
				mfx_Ext_CO2.MBBRC = MFX_CODINGOPTION_ON;
				blog(LOG_INFO, "\tMBBRC set: ON");
			}
		} else if (pParams->bMBBRC == 0) {
			mfx_Ext_CO2.MBBRC = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tMBBRC set: OFF");
		}

		//switch ((int)pParams->bDeblockingIdc) {
		//case 0:
		//	mfx_Ext_CO2.DisableDeblockingIdc = MFX_CODINGOPTION_ON;
		//	blog(LOG_INFO, "\tDeblockingIdc set: OFF");
		//	break;
		//case 1:
		//	mfx_Ext_CO2.DisableDeblockingIdc = MFX_CODINGOPTION_OFF;
		//	blog(LOG_INFO, "\tDeblockingIdc set: ON");
		//	break;
		//default:
		//	mfx_Ext_CO2.DisableDeblockingIdc = MFX_CODINGOPTION_UNKNOWN;
		//	blog(LOG_INFO, "\tDeblockingIdc set: AUTO");
		//	break;
		//}

		switch ((int)pParams->bBPyramid) {
		case 0:
			mfx_Ext_CO2.BRefType = MFX_B_REF_OFF;
			blog(LOG_INFO, "\tBPyramid set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.BRefType = MFX_B_REF_PYRAMID;
			blog(LOG_INFO, "\tBPyramid set: ON");
			break;
		default:
			if (pParams->nGOPRefDist >= 2) {
				mfx_Ext_CO2.BRefType = MFX_B_REF_PYRAMID;
				blog(LOG_INFO, "\tBPyramid set: ON");
			} else {
				blog(LOG_INFO, "\tBPyramid set: AUTO");
			}
			break;
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
		    ((pParams->bExtBRC == 1 && pParams->bLAExtBRC == 1) &&
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

		switch ((int)pParams->bExtBRC) {
		case 0:
			mfx_Ext_CO2.ExtBRC = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tExtBRC set: OFF");
			break;
		case 1:
			mfx_Ext_CO2.ExtBRC = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tExtBRC set: ON");
			break;
		default:
			blog(LOG_INFO, "\tExtBRC set: AUTO");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO2);
	}

	if (mfx_Ext_CO3_enable == 1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_CO3, MFX_EXTBUFF_CODING_OPTION3);
		mfx_Ext_CO3.RepartitionCheckEnable = MFX_CODINGOPTION_ADAPTIVE;
		mfx_Ext_CO3.FadeDetection = MFX_CODINGOPTION_ADAPTIVE;
		mfx_Ext_CO3.MBDisableSkipMap = MFX_CODINGOPTION_ON;
		mfx_Ext_CO3.EnableQPOffset = MFX_CODINGOPTION_ON;
		/*mfx_Ext_CO3.BitstreamRestriction = MFX_CODINGOPTION_ON;*/
		if (pParams->bIntraRefEncoding == 1) {
			mfx_Ext_CO3.IntRefCycleDist = (mfxU16)0;
		}

		mfx_Ext_CO3.ContentInfo = MFX_CONTENT_FULL_SCREEN_VIDEO;
		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) {
			mfx_Ext_CO3.ContentInfo = MFX_CONTENT_NOISY_VIDEO;
		}

		if (mfx_ENC_Params.mfx.RateControlMethod ==
		    MFX_RATECONTROL_CQP) {
			mfx_Ext_CO3.EnableMBQP = MFX_CODINGOPTION_ON;
		}

		if (pParams->nNumRefFrame > 0 && codec != QSV_CODEC_AV1) {
			if (mfx_ENC_Params.mfx.RateControlMethod !=
				    MFX_RATECONTROL_LA &&
			    mfx_ENC_Params.mfx.RateControlMethod !=
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
							mfx_Ext_CO3
								.NumRefActiveP[i] =
								(mfxU16)pParams
									->nNumRefFrame;
						} else {
							mfx_Ext_CO3
								.NumRefActiveP[i] =
								(mfxU16)2;
						}
						mfx_Ext_CO3.NumRefActiveBL0[i] =
							(mfxU16)pParams
								->nNumRefFrame;
						mfx_Ext_CO3.NumRefActiveBL1[i] =
							(mfxU16)pParams
								->nNumRefFrame;
					}
				} else {
					if (codec == QSV_CODEC_HEVC &&
					    (codec == QSV_CODEC_AVC &&
					     pParams->bLAExtBRC == 0)) {
						mfx_Ext_CO3.NumRefActiveP[0] =
							(mfxU16)pParams
								->nNumRefFrame;
					} else {
						mfx_Ext_CO3.NumRefActiveP[0] =
							(mfxU16)2;
					}
					mfx_Ext_CO3.NumRefActiveBL0[0] =
						(mfxU16)pParams->nNumRefFrame;
					mfx_Ext_CO3.NumRefActiveBL1[0] =
						(mfxU16)pParams->nNumRefFrame;
				}
			} else {
				mfx_Ext_CO3.NumRefActiveP[0] = (mfxU16)2;
				mfx_Ext_CO3.NumRefActiveBL0[0] = (mfxU16)2;
				mfx_Ext_CO3.NumRefActiveBL1[0] = (mfxU16)2;
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

		if ((int)pParams->nMaxFrameSizeType >= -1) {
			switch ((int)pParams->nMaxFrameSizeType) {
			case 0:
				mfx_Ext_CO3.MaxFrameSizeI =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizeIMultiplier);
				mfx_Ext_CO3.MaxFrameSizeP =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizePMultiplier);
				blog(LOG_INFO,
				     "\tMaxFrameSizeType set: MANUAL\n \tMaxFrameSizeI set: %d Kb\n \tMaxFrameSizeP set: %d Kb",
				     (int)(mfx_Ext_CO3.MaxFrameSizeI / 8 /
					   1024),
				     (int)(mfx_Ext_CO3.MaxFrameSizeP / 8 /
					   1024));
				break;
			case 1:
				mfx_Ext_CO3.MaxFrameSizeI =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizeIMultiplier);
				mfx_Ext_CO3.MaxFrameSizeP =
					(mfxU32)(((pParams->nTargetBitRate *
						   1000) /
						  (8 * pParams->nFpsNum /
						   pParams->nFpsDen)) *
						 pParams->nMaxFrameSizePMultiplier);
				mfx_Ext_CO3.AdaptiveMaxFrameSize =
					MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tMaxFrameSizeType set: MANUAL | ADAPTIVE\n \tMaxFrameSizeI set: %d Kb\n \tMaxFrameSize set: %d Kb\n \tAdaptiveMaxFrameSize set: ON",
				     (int)(mfx_Ext_CO3.MaxFrameSizeI / 8 /
					   1024),
				     (int)(mfx_Ext_CO3.MaxFrameSizeP / 8 /
					   1024));
				break;
			default:
				break;
			}
		}

		if ((int)pParams->nPRefType >= -1) {
			switch ((int)pParams->nPRefType) {
			case 0:
				mfx_Ext_CO3.PRefType = MFX_P_REF_SIMPLE;
				blog(LOG_INFO, "\tPRef set: SIMPLE");
				break;
			case 1:
				mfx_Ext_CO3.PRefType = MFX_P_REF_PYRAMID;
				blog(LOG_INFO, "\tPRef set: PYRAMID");
				break;
			default:
				mfx_Ext_CO3.PRefType = MFX_P_REF_DEFAULT;
				blog(LOG_INFO, "\tPRef set: DEFAULT");
				break;
			}
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
				blog(LOG_INFO,
				     "\tAdaptiveRef set: ON\n Warning! If you get video artifacts, please turn off AdaptiveRef");
				break;
			default:
				blog(LOG_INFO, "\tAdaptiveRef set: AUTO");
				break;
			}

			switch ((int)pParams->bAdaptiveLTR) {
			case 0:
				mfx_Ext_CO3.AdaptiveLTR = MFX_CODINGOPTION_OFF;
				blog(LOG_INFO, "\tAdaptiveLTR set: OFF");
				break;
			case 1:
				mfx_Ext_CO3.AdaptiveLTR = MFX_CODINGOPTION_ON;
				blog(LOG_INFO,
				     "\tAdaptiveLTR set: ON\n Warning! If you get an encoder error or video artifacts, please turn off AdaptiveLTR");
				break;
			default:
				blog(LOG_INFO, "\tAdaptiveLTR set: AUTO");
				break;
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
			mfx_Ext_CO3.WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
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
			mfx_Ext_CO3.WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
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
				mfx_Ext_CO3.MVCostScalingFactor = (mfxU16)0;
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

		switch ((int)pParams->nScenario) {
		case 0:
			blog(LOG_INFO, "\tScenario set: OFF");
			break;
		case 1:
			mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_ARCHIVE;
			blog(LOG_INFO, "\tScenario set: ARCHIVE");
			break;
		case 2:
			mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_LIVE_STREAMING;
			blog(LOG_INFO, "\tScenario set: LIVE STREAMING");
			break;
		case 3:
			mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
			blog(LOG_INFO, "\tScenario set: GAME STREAMING");
			break;
		case 4:
			mfx_Ext_CO3.ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
			blog(LOG_INFO, "\tScenario set: REMOTE GAMING");
			break;
		default:
			blog(LOG_INFO, "\tScenario set: AUTO");
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

	if (codec != QSV_CODEC_AV1) {

		INIT_MFX_EXT_BUFFER(mfx_CO_DDI, MFX_EXTBUFF_DDI);
		/*mfx_CO_DDI.IBC = MFX_CODINGOPTION_ON;*/
		mfx_CO_DDI.BRCPrecision = 3;
		mfx_CO_DDI.BiDirSearch = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.GlobalSearch = 2;
		mfx_CO_DDI.IntraPredCostType = 8;
		mfx_CO_DDI.MEFractionalSearchType = 16;
		mfx_CO_DDI.MVPrediction = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.WeightedBiPredIdc = pParams->bWeightedBiPred == 1
						       ? 2
						       : 0;
		mfx_CO_DDI.WeightedPrediction = pParams->bWeightedPred == 1
							? MFX_CODINGOPTION_ON
							: MFX_CODINGOPTION_OFF;
		mfx_CO_DDI.FieldPrediction = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.CabacInitIdcPlus1 = 1;
		mfx_CO_DDI.DirectCheck = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.FractionalQP = 1;
		mfx_CO_DDI.Hme = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.LocalSearch = 5;
		mfx_CO_DDI.MBAFF = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.MEInterpolationMethod = 8;
		mfx_CO_DDI.DDI.InterPredBlockSize = 64;
		mfx_CO_DDI.DDI.IntraPredBlockSize = 1;
		mfx_CO_DDI.RefOppositeField = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.RefRaw = pParams->bRawRef == 1 ? MFX_CODINGOPTION_ON
							  : MFX_CODINGOPTION_OFF;
		mfx_CO_DDI.TMVP = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.LongStartCodes = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.NumActiveRefP =
			pParams->nLADepth > 0 ? 2 : pParams->nNumRefFrame;
		mfx_CO_DDI.DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
		mfx_CO_DDI.DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
		/*mfx_CO_DDI.WriteIVFHeaders = MFX_CODINGOPTION_OFF;*/
		mfx_CO_DDI.QpAdjust = MFX_CODINGOPTION_ON;
		mfx_CO_DDI.Transform8x8Mode = MFX_CODINGOPTION_UNKNOWN;
		if (pParams->nLADepth > 39) {
			/*mfx_CO_DDI.StrengthN = 1000;*/
			mfx_CO_DDI.QpUpdateRange = 50;
			/*mfx_CO_DDI.LookAheadDependency = 10;*/
		}

		mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_CO_DDI);
	}

	INIT_MFX_EXT_BUFFER(mfx_HyperModeParam, MFX_EXTBUFF_HYPER_MODE_PARAM);
	switch ((int)pParams->nHyperMode) {
	case 0:
		mfx_HyperModeParam.Mode = MFX_HYPERMODE_OFF;
		blog(LOG_INFO, "\tHyperMode set: OFF");
		break;
	default:
		mfx_HyperModeParam.Mode = MFX_HYPERMODE_ADAPTIVE;
		blog(LOG_INFO, "\tHyperMode set: ADAPTIVE");
		break;
	}

	mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_HyperModeParam);

	//Wait next VPL release for support this
	//if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9 &&
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

	//	mfx_ENC_ExtendedBuffers.push_back((mfxExtBuffer *)&mfx_Tune);
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

	if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
		INIT_MFX_EXT_BUFFER(mfx_Ext_VPPDenoise,
				    MFX_EXTBUFF_VPP_DENOISE2);

		switch ((int)pParams->nDenoiseMode) {
		case 1:
			mfx_Ext_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | BDRATE | PRE ENCODE");
			break;
		case 2:
			mfx_Ext_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | ADJUST | POST ENCODE");
			break;
		case 3:
			mfx_Ext_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
			blog(LOG_INFO,
			     "\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
			break;
		case 4:
			mfx_Ext_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
			mfx_Ext_VPPDenoise.Strength =
				(mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO,
			     "\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
			     mfx_Ext_VPPDenoise.Strength);
			break;
		case 5:
			mfx_Ext_VPPDenoise.Mode =
				MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
			mfx_Ext_VPPDenoise.Strength =
				(mfxU16)pParams->nDenoiseStrength;
			blog(LOG_INFO,
			     "\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
			     mfx_Ext_VPPDenoise.Strength);
			break;
		default:
			mfx_Ext_VPPDenoise.Mode = MFX_DENOISE_MODE_DEFAULT;
			blog(LOG_INFO, "\tDenoise set: DEFAULT");
			break;
		}

		mfx_ENC_ExtendedBuffers.push_back(
			(mfxExtBuffer *)&mfx_Ext_VPPDenoise);
	}

	if (codec == QSV_CODEC_AV1) {
		if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 5) {
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1BitstreamParam,
					    MFX_EXTBUFF_AV1_BITSTREAM_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1ResolutionParam,
					    MFX_EXTBUFF_AV1_RESOLUTION_PARAM);
			INIT_MFX_EXT_BUFFER(mfx_Ext_AV1TileParam,
					    MFX_EXTBUFF_AV1_TILE_PARAM);

			mfx_Ext_AV1BitstreamParam.WriteIVFHeaders =
				MFX_CODINGOPTION_OFF;

			mfx_Ext_AV1ResolutionParam.FrameHeight =
				mfx_ENC_Params.mfx.FrameInfo.Height;
			mfx_Ext_AV1ResolutionParam.FrameWidth =
				mfx_ENC_Params.mfx.FrameInfo.Width;

			mfx_Ext_AV1TileParam.NumTileGroups = 1;
			mfx_Ext_AV1TileParam.NumTileColumns = 1;
			mfx_Ext_AV1TileParam.NumTileRows = 1;

			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1BitstreamParam);
			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1ResolutionParam);
			mfx_ENC_ExtendedBuffers.push_back(
				(mfxExtBuffer *)&mfx_Ext_AV1TileParam);
		}
	}

	INIT_MFX_EXT_BUFFER(mfx_Ext_VideoSignalInfo,
			    MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

	mfx_Ext_VideoSignalInfo.VideoFormat = pParams->VideoFormat;
	mfx_Ext_VideoSignalInfo.VideoFullRange = pParams->VideoFullRange;
	mfx_Ext_VideoSignalInfo.ColourDescriptionPresent = 1;
	mfx_Ext_VideoSignalInfo.ColourPrimaries = pParams->ColourPrimaries;
	mfx_Ext_VideoSignalInfo.TransferCharacteristics =
		pParams->TransferCharacteristics;
	mfx_Ext_VideoSignalInfo.MatrixCoefficients =
		pParams->MatrixCoefficients;

	mfx_ENC_ExtendedBuffers.push_back(
		(mfxExtBuffer *)&mfx_Ext_VideoSignalInfo);

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
	     (codec == QSV_CODEC_AV1 &&
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
	if (codec == QSV_CODEC_AVC) {
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
	memcpy(&validParams, &mfx_ENC_Params, sizeof(validParams));
	mfxStatus sts = mfx_VideoENC->Query(&mfx_ENC_Params, &validParams);
	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
		if (mfx_ENC_Params.mfx.LowPower == MFX_CODINGOPTION_ON) {
			mfx_ENC_Params.mfx.LowPower = MFX_CODINGOPTION_OFF;
			mfx_Ext_CO2.LookAheadDepth = pParams->nLADepth;
		}
	}

	return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(qsv_param_t *pParams)
{
	if (pParams->bResetAllowed == true) {

		switch (pParams->RateControl) {
		case MFX_RATECONTROL_CBR:
			mfx_ENC_Params.mfx.TargetKbps =
				(mfxU16)pParams->nTargetBitRate;
			break;
		case MFX_RATECONTROL_VBR:
			mfx_ENC_Params.mfx.TargetKbps = pParams->nTargetBitRate;
			mfx_ENC_Params.mfx.MaxKbps =
				pParams->nTargetBitRate > pParams->nMaxBitRate
					? pParams->nTargetBitRate
					: pParams->nMaxBitRate;
			break;
		case MFX_RATECONTROL_CQP:
			mfx_ENC_Params.mfx.QPI = pParams->nQPI;
			mfx_ENC_Params.mfx.QPP = pParams->nQPP;
			mfx_ENC_Params.mfx.QPB = pParams->nQPB;
			break;
		case MFX_RATECONTROL_ICQ:
			mfx_ENC_Params.mfx.ICQQuality = pParams->nICQQuality;
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

	INIT_MFX_EXT_BUFFER(mfx_Ext_ResetOption,
			    MFX_EXTBUFF_ENCODER_RESET_OPTION);
	mfx_Ext_ResetOption.StartNewSequence = MFX_CODINGOPTION_ON;

	extendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_ResetOption);

	mfx_ENC_Params.ExtParam = extendedBuffers.data();
	mfx_ENC_Params.NumExtParam = (mfxU16)extendedBuffers.size();

	mfxStatus sts = mfx_VideoENC->Query(&mfx_ENC_Params, &mfx_ENC_Params);

	if (sts >= MFX_ERR_NONE) {
		sts = mfx_VideoENC->Reset(&mfx_ENC_Params);
	}
	blog(LOG_INFO, "\tReset status:     %d", sts);
	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec)
{
	INIT_MFX_EXT_BUFFER(mfx_Ext_CO_SPSPPS,
			    MFX_EXTBUFF_CODING_OPTION_SPSPPS);

	std::vector<mfxExtBuffer *> extendedBuffers;
	extendedBuffers.reserve(2);

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

	extendedBuffers.push_back((mfxExtBuffer *)&mfx_Ext_CO_SPSPPS);

	mfx_ENC_Params.ExtParam = extendedBuffers.data();
	mfx_ENC_Params.NumExtParam = (mfxU16)extendedBuffers.size();

	blog(LOG_INFO, "Video params extended buffer size: %d",
	     extendedBuffers.size());

	mfxStatus sts = mfx_VideoENC->GetVideoParam(&mfx_ENC_Params);
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
		mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_CQP ||
				mfx_ENC_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_ICQ ||
				mfx_ENC_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_LA_ICQ
			? (mfx_ENC_Params.mfx.BufferSizeInKB * 1000)
		: mfx_ENC_Params.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
				mfx_ENC_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_AVBR ||
				mfx_ENC_Params.mfx.RateControlMethod ==
					MFX_RATECONTROL_VCM
			? (mfx_ENC_Params.mfx.MaxKbps * 1000)
			: (mfx_ENC_Params.mfx.TargetKbps * 1000);
	MaxLength = max((mfx_ENC_Params.mfx.BufferSizeInKB * 1000), MaxLength);
	mfx_TaskPool = mfx_ENC_Params.AsyncDepth;
	n_FirstSyncTask = 0;

	t_TaskPool = new Task[mfx_TaskPool];
	memset(t_TaskPool, 0, sizeof(Task) * mfx_TaskPool);

	for (int i = 0; i < mfx_TaskPool; i++) {
		t_TaskPool[i].mfxBS.MaxLength = (mfxU32)MaxLength;
		t_TaskPool[i].mfxBS.Data =
			new mfxU8[t_TaskPool[i].mfxBS.MaxLength];
		t_TaskPool[i].mfxBS.DataOffset = 0;
		t_TaskPool[i].mfxBS.DataLength = (mfxU32)0;
		mfx_Bitstream.CodecId = mfx_ENC_Params.mfx.CodecId;
		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS.Data,
				   MFX_ERR_MEMORY_ALLOC);
	}

	mfx_Bitstream.MaxLength = (mfxU32)MaxLength;
	mfx_Bitstream.Data = new mfxU8[mfx_Bitstream.MaxLength];
	mfx_Bitstream.DataOffset = 0;
	mfx_Bitstream.DataLength = (mfxU32)0;
	mfx_Bitstream.CodecId = mfx_ENC_Params.mfx.CodecId;

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
	const mfxU16 line_size = w * 2;

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
	if (t_TaskPool) {
		for (int i = 0; i < nPoolSize; i++) {
			if (t_TaskPool[i].syncp == NULL) {
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
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mfx_TaskPool);
	while (nTaskIdx == -1) {
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp, 60000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfxU8 *pTemp = mfx_Bitstream.Data;
		memcpy(&mfx_Bitstream, &t_TaskPool[n_FirstSyncTask].mfxBS,
		       sizeof(mfxBitstream));

		t_TaskPool[n_FirstSyncTask].mfxBS.Data = pTemp;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataLength = 0;
		t_TaskPool[n_FirstSyncTask].mfxBS.DataOffset = 0;
		t_TaskPool[n_FirstSyncTask].syncp = NULL;
		nTaskIdx = n_FirstSyncTask;
		n_FirstSyncTask = (n_FirstSyncTask + 1) % mfx_TaskPool;
		*pBS = &mfx_Bitstream;
	}
	sts = MFXMemory_GetSurfaceForEncode(mfx_Session, &mfx_FrameSurface);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfx_FrameSurface->FrameInterface->AddRef(mfx_FrameSurface);
	sts = mfx_FrameSurface->FrameInterface->Map(mfx_FrameSurface,
						    MFX_MAP_WRITE);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	mfx_FrameSurface->Info = mfx_ENC_Params.mfx.FrameInfo;
	//memcpy(&mfx_FrameSurface->Info, &mfx_ENC_Params.mfx.FrameInfo,
	//       sizeof(mfxFrameInfo));

	if (mfx_FrameSurface->Info.FourCC == MFX_FOURCC_P010) {
		LoadP010(mfx_FrameSurface, pDataY, pDataUV, strideY, strideUV);
	} else {
		LoadNV12(mfx_FrameSurface, pDataY, pDataUV, strideY, strideUV);
	}

	mfx_FrameSurface->Data.TimeStamp = ts;

	sts = mfx_FrameSurface->FrameInterface->Unmap(mfx_FrameSurface);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	for (;;) {
		MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp, 60000);
		mfx_FrameSurface->FrameInterface->AddRef(mfx_FrameSurface);

		mfx_FrameSurface->FrameInterface->Synchronize(mfx_FrameSurface,
							      60000);
		// Encode a frame asynchronously (returns immediately)
		sts = mfx_VideoENC->EncodeFrameAsync(
			&mfx_ENCCtrl_Params, mfx_FrameSurface,
			&t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		mfx_FrameSurface->FrameInterface->Release(mfx_FrameSurface);
		MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp, 60000);
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
			break;
		} else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
			break;
		} else {
			break;
		}
	}

	MFXVideoCORE_SyncOperation(mfx_Session,
				   t_TaskPool[n_FirstSyncTask].syncp, 60000);

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain()
{
	mfxStatus sts = MFX_ERR_NONE;

	while (t_TaskPool && t_TaskPool[n_FirstSyncTask].syncp) {
		sts = MFXVideoCORE_SyncOperation(
			mfx_Session, t_TaskPool[n_FirstSyncTask].syncp, 60000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		t_TaskPool[n_FirstSyncTask].syncp = NULL;
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
		g_numEncodersOpen--;
	}

	if (g_numEncodersOpen <= 0) {
		g_DX_Handle = nullptr;
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
