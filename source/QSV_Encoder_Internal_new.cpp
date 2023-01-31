#define MFX_DEPRECATED_OFF
#include "QSV_Encoder_Internal_new.h"
#include "QSV_encoder.h"
#include "mfx.h"
#ifdef DX9_D3D
#include "common_directx.h"
#elif DX11_D3D
#include "common_directx11.h"
#include "common_directx9.h"
#endif
#include <obs-module.h>

#define do_log(level, format, ...) \
	blog(level, "[qsv encoder: '%s'] " format, "vpl_impl", ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

mfxHDL QSV_VPL_Encoder_Internal::g_DX_Handle = NULL;
mfxU16 QSV_VPL_Encoder_Internal::g_numEncodersOpen = 0;
mfxLoader mfx_loader = nullptr;

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxIMPL& impl, mfxVersion& version, bool isDGPU)
	: mfx_FrameSurf(NULL),
	mfx_VidEnc(NULL),
	mU16_SPSBufSize(1024),
	mU16_PPSBufSize(1024),
	mU16_TaskPool(0),
	t_TaskPool(NULL),
	n_TaskIdx(0),
	n_FirstSyncTask(0),
	mfx_Bitstream(),
	b_isDGPU(isDGPU)
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
				mfx_impl = tempImpl;
				blog(LOG_INFO, "\tImplementation:           D3D11\n"
					"\tsurf:           D3D11");
			}
			MFXVideoENCODE_Close(mfx_session);
			MFXClose(mfx_session);
			MFXUnload(mfx_loader);

			return;
		}
		else {
			blog(LOG_INFO, "\tImplementation: [ERROR]\n");
			MFXVideoENCODE_Close(mfx_session);
			MFXClose(mfx_session);
			MFXUnload(mfx_loader);
		}
	}
	else {
		blog(LOG_INFO, "\tImplementation: [ERROR]\n");
		MFXVideoENCODE_Close(mfx_session);
		MFXClose(mfx_session);
		MFXUnload(mfx_loader);
		return;
	}
}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal()
{
	ClearData();
}

mfxStatus QSV_VPL_Encoder_Internal::Initialize(mfxIMPL impl, mfxVersion ver, mfxSession* pSession,
	mfxFrameAllocator* mfx_FrameAllocator, mfxHDL* deviceHandle,
	bool bCreateSharedHandles, bool dx9hack)
{
	bCreateSharedHandles; // (Hugh) Currently unused
	mfx_FrameAllocator;       // (Hugh) Currently unused

	mfxStatus sts = MFX_ERR_NONE;

	// If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
	if (mfx_FrameAllocator) {
		// Initialize VPL Session

		mfx_loader = MFXLoad();
		mfxStatus sts = MFXCreateSession(mfx_loader, 0, &mfx_session);

		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		// Create DirectX device context
		if (deviceHandle == NULL || *deviceHandle == NULL) {
			sts = CreateHWDevice(*pSession, deviceHandle, NULL,
				bCreateSharedHandles);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		}

		if (deviceHandle == NULL || *deviceHandle == NULL)
			return MFX_ERR_DEVICE_FAILED;

		// Provide device manager to VPL

		sts = MFXVideoCORE_SetHandle(*pSession, DEVICE_MGR_TYPE, *deviceHandle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfx_FrameAllocator->pthis =
			*pSession; // We use VPL session ID as the allocation identifier
		mfx_FrameAllocator->Alloc = simple_alloc;
		mfx_FrameAllocator->Free = simple_free;
		mfx_FrameAllocator->Lock = simple_lock;
		mfx_FrameAllocator->Unlock = simple_unlock;
		mfx_FrameAllocator->GetHDL = simple_gethdl;

		// Since we are using video memory we must provide VPL with an external allocator
		sts = MFXVideoCORE_SetFrameAllocator(mfx_session, mfx_FrameAllocator);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Open(qsv_param_t* pParams, enum qsv_codec codec) {

	// Use D3D11 surface
	mfxStatus sts = Initialize(mfx_impl, mfx_version, &mfx_session, &mfx_FrameAllocator,
		&g_DX_Handle, false, false);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


	sts = InitParams(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = MFXVideoENCODE_Query(mfx_session, &mfx_EncParams, &mfx_EncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = AllocateSurfaces();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = MFXVideoENCODE_Init(mfx_session, &mfx_EncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = GetVideoParam(codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = InitBitstream();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (sts >= MFX_ERR_NONE) {
		g_numEncodersOpen++;
	}
	else {
		blog(LOG_INFO, "\tOpen encoder: [ERROR]\n");
		MFXVideoENCODE_Close(mfx_session);
		MFXClose(mfx_session);
		MFXUnload(mfx_loader);
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitParams(qsv_param_t* pParams,
	enum qsv_codec codec)
{
	memset(&mfx_EncParams, 0, sizeof(mfx_EncParams));

	if (codec == QSV_CODEC_AVC)
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AVC;
	else if (codec == QSV_CODEC_AV1)
		mfx_EncParams.mfx.CodecId = MFX_CODEC_AV1;
	else if (codec == QSV_CODEC_HEVC)
		mfx_EncParams.mfx.CodecId = MFX_CODEC_HEVC;

	switch (pParams->nGopOptFlag) {
	case 0:
		mfx_EncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
		blog(LOG_INFO, "\tGopOptFlag set to CLOSED:     %d",
			pParams->nGopOptFlag);
		break;
	case 1:
		mfx_EncParams.mfx.GopOptFlag = 0x00;
		blog(LOG_INFO, "\tGopOptFlag set to OPEN:     %d",
			pParams->nGopOptFlag);
		break;
	case 2:
		mfx_EncParams.mfx.GopOptFlag = MFX_GOP_STRICT;
		blog(LOG_INFO, "\tGopOptFlag set to STRICT:     %d",
			pParams->nGopOptFlag);
		break;
	}

	if (((int)pParams->nNumRefFrame >= 0) &&
		((int)pParams->nNumRefFrame < 17)) {
		mfx_EncParams.mfx.NumRefFrame = pParams->nNumRefFrame;
		blog(LOG_INFO, "\tNumRefFrame set to:     %d",
			pParams->nNumRefFrame);
	}

	if (codec == QSV_CODEC_HEVC) {
		mfx_EncParams.mfx.NumSlice = 0;
		mfx_EncParams.mfx.IdrInterval = 1;
	}
	else {
		mfx_EncParams.mfx.NumSlice = 1;
	}
	mfx_EncParams.mfx.TargetUsage = pParams->nTargetUsage;
	mfx_EncParams.mfx.CodecProfile = pParams->nCodecProfile;
	mfx_EncParams.mfx.FrameInfo.FrameRateExtN = pParams->nFpsNum;
	mfx_EncParams.mfx.FrameInfo.FrameRateExtD = pParams->nFpsDen;
	if (pParams->video_fmt_10bit) {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
		mfx_EncParams.mfx.FrameInfo.BitDepthChroma = 10;
		mfx_EncParams.mfx.FrameInfo.BitDepthLuma = 10;
		mfx_EncParams.mfx.FrameInfo.Shift = 1;
	}
	else {
		mfx_EncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	}
	mfx_EncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_EncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_EncParams.mfx.FrameInfo.CropX = 0;
	mfx_EncParams.mfx.FrameInfo.CropY = 0;
	mfx_EncParams.mfx.FrameInfo.CropW = pParams->nWidth;
	mfx_EncParams.mfx.FrameInfo.CropH = pParams->nHeight;
	if (codec == QSV_CODEC_AV1)
		mfx_EncParams.mfx.GopRefDist = 1;
	else
		mfx_EncParams.mfx.GopRefDist = pParams->nbFrames + 1;

	if (codec == QSV_CODEC_HEVC)
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;

	enum qsv_cpu_platform qsv_platform = qsv_get_cpu_platform();
	if ((b_isDGPU || qsv_platform >= QSV_CPU_PLATFORM_ICL ||
		qsv_platform == QSV_CPU_PLATFORM_UNKNOWN) &&
		(pParams->nbFrames == 0) &&
		(mfx_version.Major >= 1 && mfx_version.Minor >= 31)) {
		mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
		if (pParams->nRateControl == MFX_RATECONTROL_LA_ICQ ||
			pParams->nRateControl == MFX_RATECONTROL_LA_HRD ||
			pParams->nRateControl == MFX_RATECONTROL_LA)
			pParams->nRateControl = MFX_RATECONTROL_VBR;
	}

	mfx_EncParams.mfx.RateControlMethod = pParams->nRateControl;

	switch (pParams->nRateControl) {
	case MFX_RATECONTROL_CBR:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = pParams->nTargetBitRate * 2;
		mfx_EncParams.mfx.InitialDelayInKB =
			pParams->nTargetBitRate * 1;
		break;
	case MFX_RATECONTROL_VBR:
	case MFX_RATECONTROL_VCM:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.MaxKbps = pParams->nMaxBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = pParams->nTargetBitRate * 2;
		mfx_EncParams.mfx.InitialDelayInKB =
			pParams->nTargetBitRate * 1;
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
		mfx_EncParams.mfx.ICQQuality = pParams->nICQQuality;
		break;
	case MFX_RATECONTROL_LA:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		break;
	case MFX_RATECONTROL_LA_ICQ:
		mfx_EncParams.mfx.ICQQuality = pParams->nICQQuality;
		break;
	case MFX_RATECONTROL_LA_HRD:
		mfx_EncParams.mfx.TargetKbps = pParams->nTargetBitRate;
		mfx_EncParams.mfx.BufferSizeInKB = pParams->nTargetBitRate * 2;
		mfx_EncParams.mfx.InitialDelayInKB = pParams->nTargetBitRate * 1;
		break;
	}

	mfx_EncParams.AsyncDepth = pParams->nAsyncDepth;

	mfx_EncParams.mfx.GopPicSize =
		(mfxU16)(pParams->nKeyIntSec * pParams->nFpsNum /
			(float)pParams->nFpsDen);

	if (mfx_version.Major >= 1 && mfx_version.Minor >= 8) {
		memset(&mfx_co2, 0, sizeof(mfxExtCodingOption2));
		mfx_co2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
		mfx_co2.Header.BufferSz = sizeof(mfx_co2);
		if (codec != QSV_CODEC_AVC)
			mfx_co2.RepeatPPS = MFX_CODINGOPTION_ON;
		if (pParams->nRateControl == MFX_RATECONTROL_LA_ICQ ||
			pParams->nRateControl == MFX_RATECONTROL_LA ||
			pParams->nRateControl == MFX_RATECONTROL_LA_HRD)
			mfx_co2.LookAheadDepth = pParams->nLADEPTH;
		blog(LOG_INFO, "\tLookahead set to:     %d",
			pParams->nLADEPTH);
		if ((pParams->bMBBRC && pParams->nRateControl != MFX_RATECONTROL_LA_ICQ) || (pParams->bMBBRC && pParams->nRateControl != MFX_RATECONTROL_LA) || (pParams->bMBBRC && pParams->nRateControl != MFX_RATECONTROL_LA_HRD))
			mfx_co2.MBBRC = MFX_CODINGOPTION_ON;
		//mfx_co2.ExtBRC = MFX_CODINGOPTION_ON;
		if (pParams->nbFrames > 1)
			mfx_co2.BRefType = MFX_B_REF_PYRAMID;

		switch (pParams->nTrellis) {
		case 0:
			mfx_co2.Trellis = MFX_TRELLIS_OFF;
			blog(LOG_INFO, "\tTrellis disabled:     %d",
				pParams->nTrellis);
			break;
		case 1:
			mfx_co2.Trellis = MFX_TRELLIS_I;
			blog(LOG_INFO, "\tTrellis set I:     %d",
				pParams->nTrellis);
			break;
		case 2:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set IP:     %d",
				pParams->nTrellis);
			break;
		case 3:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P |
				MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set IPB:     %d",
				pParams->nTrellis);
			break;
		case 4:
			mfx_co2.Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set IB:     %d",
				pParams->nTrellis);
			break;
		case 5:
			mfx_co2.Trellis = MFX_TRELLIS_P;
			blog(LOG_INFO, "\tTrellis set P:     %d",
				pParams->nTrellis);
			break;
		case 6:
			mfx_co2.Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
			blog(LOG_INFO, "\tTrellis set PB:     %d",
				pParams->nTrellis);
			break;
		case 7:
			blog(LOG_INFO, "\tTrellis set B:     %d",
				pParams->nTrellis);
			break;
		default:
			mfx_co2.Trellis = MFX_TRELLIS_UNKNOWN;
			blog(LOG_INFO, "\tTrellis set unknown:     %d",
				pParams->nTrellis);
			break;
		}

		if (pParams->bAdaptiveI) {
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveI enabled:     %d",
				pParams->bAdaptiveI);
		}
		else {
			mfx_co2.AdaptiveI = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveI disabled:     %d",
				pParams->bAdaptiveI);
		}

		if (pParams->bAdaptiveB) {
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tAdaptiveB enabled:     %d",
				pParams->bAdaptiveB);
		}
		else {
			mfx_co2.AdaptiveB = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tAdaptiveB disabled:     %d",
				pParams->bAdaptiveB);
		}

		switch (pParams->nLookAheadDS) {
		case 0:
			mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
			blog(LOG_INFO, "\tLookAheadDS set to off:     %d",
				pParams->nLookAheadDS);
			break;
		case 1:
			mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_2x;
			blog(LOG_INFO, "\tLookAheadDS set to 2x:     %d",
				pParams->nLookAheadDS);
			break;
		case 2:
			mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_4x;
			blog(LOG_INFO, "\tLookAheadDS set to 4x:     %d",
				pParams->nLookAheadDS);
			break;
		default:
			mfx_co2.LookAheadDS = MFX_LOOKAHEAD_DS_UNKNOWN;
			blog(LOG_INFO, "\tLookAheadDS set to unknown:     %d",
				pParams->nLookAheadDS);
			break;
		}

		if (pParams->bUseRawRef) {
			mfx_co2.UseRawRef = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tUseRawRef enabled:     %d",
				pParams->bUseRawRef);
		}
		else {
			mfx_co2.UseRawRef = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tUseRawRef disabled:     %d",
				pParams->bUseRawRef);
		}

		if (mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
			if (codec == QSV_CODEC_AVC)
				mfx_co2.RepeatPPS = MFX_CODINGOPTION_OFF;
			if (pParams->nRateControl == MFX_RATECONTROL_CBR ||
				pParams->nRateControl == MFX_RATECONTROL_VBR) {
				mfx_co2.LookAheadDepth = pParams->nLADEPTH;
			}
		}

		extendedBuffers.push_back((mfxExtBuffer*)&mfx_co2);

	}

	if ((mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) ||
		(mfx_version.Major >= 1 && mfx_version.Minor >= 16)) {
		memset(&mfx_co3, 0, sizeof(mfxExtCodingOption3));
		mfx_co3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
		mfx_co3.Header.BufferSz = sizeof(mfx_co3);
		mfx_co3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;

		if (pParams->nbFrames <= 1) mfx_co3.PRefType = MFX_P_REF_PYRAMID;

		if (pParams->bAdaptiveMaxFrameSize) {
			mfx_co3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_ON;
		}

		if (pParams->bAdaptiveCQM) {
			mfx_co3.AdaptiveCQM = MFX_CODINGOPTION_ON;
		}

		if (pParams->bAdaptiveRef) {
			mfx_co3.AdaptiveRef = MFX_CODINGOPTION_ON;
		}

		//mfx_co3.EnableMBQP = MFX_CODINGOPTION_ON;

		switch (pParams->nMotionVectorsOverPicBoundaries) {
		case 0:
			mfx_co3.MotionVectorsOverPicBoundaries =
				MFX_CODINGOPTION_OFF;
			blog(LOG_INFO,
				"\tMotionVectorsOverPicBoundaries disabled:     %d",
				pParams->nMotionVectorsOverPicBoundaries);
			break;
		case 1:
			mfx_co3.MotionVectorsOverPicBoundaries =
				MFX_CODINGOPTION_ON;
			blog(LOG_INFO,
				"\tMotionVectorsOverPicBoundaries enabled:     %d",
				pParams->nMotionVectorsOverPicBoundaries);
			break;
		case 2:
			mfx_co3.MotionVectorsOverPicBoundaries =
				MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO,
				"\tMotionVectorsOverPicBoundaries set auto:     %d",
				pParams->nMotionVectorsOverPicBoundaries);
			break;
		}


		if (pParams->bGlobalMotionBiasAdjustment) {
			mfx_co3.GlobalMotionBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO,
				"\tGlobalMotionBiasAdjustment enabled:     %d",
				pParams->bGlobalMotionBiasAdjustment);
		}
		else {
			mfx_co3.GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO,
				"\tGlobalMotionBiasAdjustment disabled:     %d",
				pParams->bGlobalMotionBiasAdjustment);
		}

		switch (pParams->nRepartitionCheckEnable) {
		case 0:
			mfx_co3.RepartitionCheckEnable = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO,
				"\tRepartitionCheckEnable set performance:     %d",
				pParams->nRepartitionCheckEnable);
			break;
		case 1:
			mfx_co3.RepartitionCheckEnable = MFX_CODINGOPTION_ON;
			blog(LOG_INFO,
				"\tRepartitionCheckEnable set quality:     %d",
				pParams->nRepartitionCheckEnable);
			break;
		case 2:
			mfx_co3.RepartitionCheckEnable = MFX_CODINGOPTION_UNKNOWN;
			blog(LOG_INFO,
				"\tRepartitionCheckEnable set auto:     %d",
				pParams->nRepartitionCheckEnable);
			break;
		}

		if (pParams->bWeightedPred) {
			mfx_co3.WeightedPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO,
				"\tWeightedPred implicit enabled:     %d",
				pParams->bWeightedPred);
		}
		else {
			mfx_co3.WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
			blog(LOG_INFO, "\tWeightedPred default enabled:     %d",
				pParams->bWeightedPred);
		}

		if (pParams->bWeightedBiPred) {
			mfx_co3.WeightedBiPred = MFX_WEIGHTED_PRED_IMPLICIT;
			blog(LOG_INFO,
				"\tWeightedBiPred implicit enabled:     %d",
				pParams->bWeightedBiPred);
		}
		else {
			mfx_co3.WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
			blog(LOG_INFO,
				"\tWeightedBiPred default enabled:     %d",
				pParams->bWeightedBiPred);
		}

		if (pParams->bGlobalMotionBiasAdjustment) {
			switch (pParams->nMVCostScalingFactor) {
			case 0:
				mfx_co3.MVCostScalingFactor = (mfxU16)0;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set 0:     %d",
					pParams->nMVCostScalingFactor);
				break;
			case 1:
				mfx_co3.MVCostScalingFactor = (mfxU16)1;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set 1/2:     %d",
					pParams->nMVCostScalingFactor);
				break;
			case 2:
				mfx_co3.MVCostScalingFactor = (mfxU16)2;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set 1/4:     %d",
					pParams->nMVCostScalingFactor);
				break;
			case 3:
				mfx_co3.MVCostScalingFactor = (mfxU16)3;
				blog(LOG_INFO,
					"\tMVCostScalingFactor set 1/8:     %d",
					pParams->nMVCostScalingFactor);
				break;
			}
		}

		if (pParams->bDirectBiasAdjustment) {
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tDirectBiasAdjustment enabled:     %d",
				pParams->bDirectBiasAdjustment);
		}
		else {
			mfx_co3.DirectBiasAdjustment = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO,
				"\tDirectBiasAdjustment disabled:     %d",
				pParams->bDirectBiasAdjustment);
		}

		if (pParams->bFadeDetection) {
			mfx_co3.FadeDetection = MFX_CODINGOPTION_ON;
			blog(LOG_INFO, "\tFadeDetection enabled:     %d",
				pParams->bFadeDetection);
		}
		else {
			mfx_co3.FadeDetection = MFX_CODINGOPTION_OFF;
			blog(LOG_INFO, "\tFadeDetection disabled:     %d",
				pParams->bFadeDetection);
		}

		extendedBuffers.push_back((mfxExtBuffer*)&mfx_co3);

		memset(&mfx_co, 0, sizeof(mfxExtCodingOption));
		mfx_co.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
		mfx_co.Header.BufferSz = sizeof(mfx_co);
		mfx_co.IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_co.InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
		mfx_co.MVPrecision = MFX_MVPRECISION_QUARTERPEL;
		//mfx_co.MaxDecFrameBuffering = (int)(pParams->nLADEPTH + pParams->nFpsNum);

		if (pParams->bUseRDO) {
			mfx_co.RateDistortionOpt = MFX_CODINGOPTION_ON;
		}

		mfx_co.VuiNalHrdParameters = MFX_CODINGOPTION_ON;
		mfx_co.NalHrdConformance = MFX_CODINGOPTION_ON;
		mfx_co.ResetRefList = MFX_CODINGOPTION_ON;

		extendedBuffers.push_back((mfxExtBuffer*)&mfx_co3);

		if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_HEVC) && pParams->nTemporalLayers > 0) {
			memset(&mfx_tl, 0, sizeof(mfxExtTemporalLayers));
			mfx_tl.Header.BufferId = MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS;
			mfx_tl.Header.BufferSz = sizeof(mfx_tl);
			mfx_tl.BaseLayerPID = 0;
			int TemporalLayersScalers[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
			for (int i = 0; i <= pParams->nTemporalLayers; i++) {
				mfx_tl.Layers[i].FrameRateScale = TemporalLayersScalers[i];
			}
			extendedBuffers.push_back((mfxExtBuffer*)&mfx_tl);
		}
	}

	if (codec == QSV_CODEC_HEVC) {
		if ((pParams->nWidth & 15) || (pParams->nHeight & 15)) {
			memset(&mfx_ExtHEVCParam, 0, sizeof(mfx_ExtHEVCParam));
			mfx_ExtHEVCParam.Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
			mfx_ExtHEVCParam.Header.BufferSz = sizeof(mfx_ExtHEVCParam);
			mfx_ExtHEVCParam.PicWidthInLumaSamples = pParams->nWidth;
			mfx_ExtHEVCParam.PicHeightInLumaSamples =
				pParams->nHeight;
			extendedBuffers.push_back(
				(mfxExtBuffer*)&mfx_ExtHEVCParam);
		}
	}

	memset(&mfx_ExtVideoSignalInfo, 0, sizeof(mfx_ExtVideoSignalInfo));
	mfx_ExtVideoSignalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
	mfx_ExtVideoSignalInfo.Header.BufferSz = sizeof(mfx_ExtVideoSignalInfo);
	mfx_ExtVideoSignalInfo.VideoFormat = pParams->VideoFormat;
	mfx_ExtVideoSignalInfo.VideoFullRange = pParams->VideoFullRange;
	mfx_ExtVideoSignalInfo.ColourDescriptionPresent = 1;
	mfx_ExtVideoSignalInfo.ColourPrimaries = pParams->ColourPrimaries;
	mfx_ExtVideoSignalInfo.TransferCharacteristics =
		pParams->TransferCharacteristics;
	mfx_ExtVideoSignalInfo.MatrixCoefficients = pParams->MatrixCoefficients;
	extendedBuffers.push_back((mfxExtBuffer*)&mfx_ExtVideoSignalInfo);

	/* TODO: Ask Intel why this is MFX_ERR_UNSUPPORTED */
	//Worked with oneVPL
	memset(&mfx_ExtChromaLocInfo, 0, sizeof(mfx_ExtChromaLocInfo));
	mfx_ExtChromaLocInfo.Header.BufferId = MFX_EXTBUFF_CHROMA_LOC_INFO;
	mfx_ExtChromaLocInfo.Header.BufferSz = sizeof(mfx_ExtChromaLocInfo);
	mfx_ExtChromaLocInfo.ChromaLocInfoPresentFlag = 1;
	mfx_ExtChromaLocInfo.ChromaSampleLocTypeTopField =
		pParams->ChromaSampleLocTypeTopField;
	mfx_ExtChromaLocInfo.ChromaSampleLocTypeBottomField =
		pParams->ChromaSampleLocTypeBottomField;
	extendedBuffers.push_back((mfxExtBuffer*)&mfx_ExtChromaLocInfo);

	if (codec != QSV_CODEC_AV1 && pParams->MaxContentLightLevel > 0) {
		memset(&mfx_ExtMasteringDisplayColourVolume, 0,
			sizeof(mfx_ExtMasteringDisplayColourVolume));
		mfx_ExtMasteringDisplayColourVolume.Header.BufferId =
			MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
		mfx_ExtMasteringDisplayColourVolume.Header.BufferSz =
			sizeof(mfx_ExtMasteringDisplayColourVolume);
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
		mfx_ExtMasteringDisplayColourVolume.MaxDisplayMasteringLuminance =
			pParams->MaxDisplayMasteringLuminance;
		mfx_ExtMasteringDisplayColourVolume.MinDisplayMasteringLuminance =
			pParams->MinDisplayMasteringLuminance;
		extendedBuffers.push_back(
			(mfxExtBuffer*)&mfx_ExtMasteringDisplayColourVolume);

		memset(&mfx_ExtContentLightLevelInfo, 0,
			sizeof(mfx_ExtContentLightLevelInfo));
		mfx_ExtContentLightLevelInfo.Header.BufferId =
			MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
		mfx_ExtContentLightLevelInfo.Header.BufferSz =
			sizeof(mfx_ExtContentLightLevelInfo);
		mfx_ExtContentLightLevelInfo.InsertPayloadToggle =
			MFX_PAYLOAD_IDR;
		mfx_ExtContentLightLevelInfo.MaxContentLightLevel =
			pParams->MaxContentLightLevel;
		mfx_ExtContentLightLevelInfo.MaxPicAverageLightLevel =
			pParams->MaxPicAverageLightLevel;
		extendedBuffers.push_back(
			(mfxExtBuffer*)&mfx_ExtContentLightLevelInfo);
	}

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture
	mfx_EncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(pParams->nWidth);
	mfx_EncParams.mfx.FrameInfo.Height = MSDK_ALIGN16(pParams->nHeight);

	mfx_EncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	mfx_EncParams.ExtParam = extendedBuffers.data();
	mfx_EncParams.NumExtParam = (mfxU16)extendedBuffers.size();

	memset(&mfx_EncControl, 0, sizeof(mfx_EncControl));
	mfx_EncControl.ExtParam = extendedBuffers.data();
	mfx_EncControl.NumExtParam = (mfxU16)extendedBuffers.size();

	// We dont check what was valid or invalid here, just try changing lower power.
	// Ensure set values are not overwritten so in case it wasnt lower power we fail
	// during the parameter check.
	mfxVideoParam validParams = { 0 };
	memcpy(&validParams, &mfx_EncParams, sizeof(validParams));
	mfxStatus sts = MFXVideoENCODE_Query(mfx_session, &mfx_EncParams, &validParams);
	if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
		if (mfx_EncParams.mfx.LowPower == MFX_CODINGOPTION_ON) {
			mfx_EncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
			mfx_co2.LookAheadDepth = pParams->nLADEPTH;
		}
	}

	return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(qsv_param_t* pParams)
{
	switch (pParams->nRateControl) {
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
	return MFXVideoENCODE_Reset(mfx_session, &mfx_EncParams);
}

mfxStatus QSV_VPL_Encoder_Internal::AllocateSurfaces()
{
	// Query number of required surfaces for encoder
	mfxFrameAllocRequest EncRequest;
	memset(&EncRequest, 0, sizeof(EncRequest));
	mfxStatus sts = MFXVideoENCODE_QueryIOSurf(mfx_session, &mfx_EncParams, &EncRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	EncRequest.Type |= WILL_WRITE;

	// SNB hack. On some SNB, it seems to require more surfaces
	EncRequest.NumFrameSuggested += mfx_EncParams.AsyncDepth;

	// Allocate required surfaces
	sts = mfx_FrameAllocator.Alloc(mfx_FrameAllocator.pthis, &EncRequest,
		&mfx_FrameAllocResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mU16_SurfNum = mfx_FrameAllocResponse.NumFrameActual;

	mfx_FrameSurf = new mfxFrameSurface1 * [mU16_SurfNum];
	MSDK_CHECK_POINTER(mfx_FrameSurf, MFX_ERR_MEMORY_ALLOC);

	for (int i = 0; i < mU16_SurfNum; i++) {
		mfx_FrameSurf[i] = new mfxFrameSurface1;
		memset(mfx_FrameSurf[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(mfx_FrameSurf[i]->Info),
			&(mfx_EncParams.mfx.FrameInfo),
			sizeof(mfxFrameInfo));
		mfx_FrameSurf[i]->Data.MemId = mfx_FrameAllocResponse.mids[i];
	}

	blog(LOG_INFO, "\tSurface count:     %d", mU16_SurfNum);

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec)
{
	mfxExtCodingOptionSPSPPS opt;
	memset(&opt, 0, sizeof(mfxExtCodingOptionSPSPPS));

	opt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
	opt.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);

	std::vector<mfxExtBuffer*> extendedBuffers;
	extendedBuffers.reserve(2);

	opt.SPSBuffer = mU8_SPSBuffer;
	opt.PPSBuffer = mU8_PPSBuffer;
	opt.SPSBufSize = 1024; //  m_nSPSBufferSize;
	opt.PPSBufSize = 1024; //  m_nPPSBufferSize;

	mfxExtCodingOptionVPS opt_vps{};
	memset(&opt_vps, 0, sizeof(mfxExtCodingOptionVPS));
	if (codec == QSV_CODEC_HEVC) {
		opt_vps.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_VPS;
		opt_vps.Header.BufferSz = sizeof(mfxExtCodingOptionVPS);
		opt_vps.VPSBuffer = mU8_VPSBuffer;
		opt_vps.VPSBufSize = 1024;

		extendedBuffers.push_back((mfxExtBuffer*)&opt_vps);
	}

	extendedBuffers.push_back((mfxExtBuffer*)&opt);

	mfx_EncParams.ExtParam = extendedBuffers.data();
	mfx_EncParams.NumExtParam = (mfxU16)extendedBuffers.size();

	mfxStatus sts = MFXVideoENCODE_GetVideoParam(mfx_session, &mfx_EncParams);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (codec == QSV_CODEC_HEVC)
		mU16_VPSBufSize = opt_vps.VPSBufSize;
	mU16_SPSBufSize = opt.SPSBufSize;
	mU16_PPSBufSize = opt.PPSBufSize;

	return sts;
}

void QSV_VPL_Encoder_Internal::GetSPSPPS(mfxU8** pSPSBuf, mfxU8** pPPSBuf,
	mfxU16* pnSPSBuf, mfxU16* pnPPSBuf)
{
	*pSPSBuf = mU8_SPSBuffer;
	*pPPSBuf = mU8_PPSBuffer;
	*pnSPSBuf = mU16_SPSBufSize;
	*pnPPSBuf = mU16_PPSBufSize;
}

void QSV_VPL_Encoder_Internal::GetVpsSpsPps(mfxU8** pVPSBuf, mfxU8** pSPSBuf,
	mfxU8** pPPSBuf, mfxU16* pnVPSBuf,
	mfxU16* pnSPSBuf, mfxU16* pnPPSBuf)
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
			mfx_EncParams.mfx.BufferSizeInKB * 1000;
		t_TaskPool[i].mfxBS.Data =
			new mfxU8[t_TaskPool[i].mfxBS.MaxLength];
		t_TaskPool[i].mfxBS.DataOffset = 0;
		t_TaskPool[i].mfxBS.DataLength = 0;

		MSDK_CHECK_POINTER(t_TaskPool[i].mfxBS.Data,
			MFX_ERR_MEMORY_ALLOC);
	}

	memset(&mfx_Bitstream, 0, sizeof(mfxBitstream));
	mfx_Bitstream.MaxLength = mfx_EncParams.mfx.BufferSizeInKB * 1000;
	mfx_Bitstream.Data = new mfxU8[mfx_Bitstream.MaxLength];
	mfx_Bitstream.DataOffset = 0;
	mfx_Bitstream.DataLength = 0;

	blog(LOG_INFO, "\tTaskPool count:    %d", mU16_TaskPool);

	return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadP010(mfxFrameSurface1* pSurface,
	uint8_t* pDataY, uint8_t* pDataUV,
	uint32_t strideY, uint32_t strideUV)
{
	mfxU16 w, h, i, pitch;
	mfxU8* ptr;
	mfxFrameInfo* pInfo = &pSurface->Info;
	mfxFrameData* pData = &pSurface->Data;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	}
	else {
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

mfxStatus QSV_VPL_Encoder_Internal::LoadNV12(mfxFrameSurface1* pSurface,
	uint8_t* pDataY, uint8_t* pDataUV,
	uint32_t strideY, uint32_t strideUV)
{
	mfxU16 w, h, i, pitch;
	mfxU8* ptr;
	mfxFrameInfo* pInfo = &pSurface->Info;
	mfxFrameData* pData = &pSurface->Data;

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	}
	else {
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

int QSV_VPL_Encoder_Internal::GetFreeTaskIndex(Task* t_TaskPool, mfxU16 nPoolSize)
{
	if (t_TaskPool)
		for (int i = 0; i < nPoolSize; i++)
			if (!t_TaskPool[i].syncp)
				return i;
	return MFX_ERR_NOT_FOUND;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(uint64_t ts, uint8_t* pDataY,
	uint8_t* pDataUV, uint32_t strideY,
	uint32_t strideUV, mfxBitstream** pBS)
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
		sts = MFXVideoCORE_SyncOperation(mfx_session,
			t_TaskPool[n_FirstSyncTask].syncp, 60000);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		mfxU8* pTemp = mfx_Bitstream.Data;
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

	mfxFrameSurface1* pSurface = mfx_FrameSurf[nSurfIdx];
	sts = mfx_FrameAllocator.Lock(mfx_FrameAllocator.pthis,
		pSurface->Data.MemId,
		&(pSurface->Data));
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

		mfxEncodeCtrl curentEncControl = mfx_EncControl;
		// Encode a frame asynchronously (returns immediately)
		sts = MFXVideoENCODE_EncodeFrameAsync(mfx_session, &mfx_EncControl, pSurface,
			&t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			//blog(LOG_INFO, "Encode error: Trying repeat without output");

			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts) {
				/*MFXVideoCORE_SyncOperation(mfx_session,
					t_TaskPool[n_FirstSyncTask].syncp, 10);*/
				MSDK_SLEEP(
					10); // Wait if device is busy, then repeat the same call
			}
		}
		else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			//blog(LOG_INFO, "Encode error: Trying repeat with output");
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		}
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
			// Allocate more bitstream buffer memory here if needed...
			break;
		}
		else if (sts == MFX_ERR_DEVICE_LOST) {
			blog(LOG_INFO, "Encode error: Hardware device lost");
			break;
		}
		else if (sts == MFX_ERR_MORE_DATA) {
			//blog(LOG_INFO, "Encode error: More data");
			break;
		}
		else if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) {
			blog(LOG_INFO, "Encode error: Incompability video params");
			break;
		}
		else if (sts == MFX_WRN_IN_EXECUTION) {
			blog(LOG_INFO, "Encode warning: In execution");

			/*MFXVideoCORE_SyncOperation(mfx_session,
				t_TaskPool[n_FirstSyncTask].syncp, 10);*/
		}
		else if (sts == MFX_ERR_REALLOC_SURFACE) {
			blog(LOG_INFO, "Encode error: Need bigger surface");
			break;
		}
		else if (sts == MFX_ERR_MORE_BITSTREAM) {
			blog(LOG_INFO, "Encode error: Need more bitstream");
			break;
		}
		else if (sts == MFX_ERR_GPU_HANG) {
			blog(LOG_INFO, "Encode error: GPU hang");
			break;
		}
		else if (sts == MFX_TASK_BUSY) {
			blog(LOG_INFO, "Encode warn: task busy");
			MSDK_SLEEP(
				100);
			break;
		}
		else if (sts == MFX_ERR_DEVICE_FAILED) {
			blog(LOG_INFO, "Encode error: Device failed");
			break;
		}
		else {
			break;
		}
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode_tex(uint64_t ts, uint32_t tex_handle,
	uint64_t lock_key,
	uint64_t* next_key,
	mfxBitstream** pBS)
{
	mfxStatus sts = MFX_ERR_NONE;
	*pBS = NULL;
	int nTaskIdx = GetFreeTaskIndex(t_TaskPool, mU16_TaskPool);
	int nSurfIdx = GetFreeSurfaceIndex(mfx_FrameSurf, mU16_SurfNum);

	while (MFX_ERR_NOT_FOUND == nTaskIdx || MFX_ERR_NOT_FOUND == nSurfIdx) {
		// No more free tasks or surfaces, need to sync
		sts = MFXVideoCORE_SyncOperation(mfx_session,
			t_TaskPool[n_FirstSyncTask].syncp, 60000);

		mfxU8* pTemp = mfx_Bitstream.Data;
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

	mfxFrameSurface1* pSurface = mfx_FrameSurf[nSurfIdx];
	//copy to default surface directly
	pSurface->Data.TimeStamp = ts;
	sts = simple_copytex(mfx_FrameAllocator.pthis, pSurface->Data.MemId,
		tex_handle, lock_key, next_key);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	/*MFXVideoENCODE_Init(mfx_session, &mfx_EncParams);*/
	for (;;) {
		////Allocate internal memory for Surface
		//sts = MFXMemory_GetSurfaceForEncode(mfx_session, &pSurface);
		//if (sts != MFX_ERR_NONE) {
		//	blog(LOG_INFO, "\tAllocate memory error");
		//	break;
		//}

		// Encode a frame asynchronously (returns immediately)
		sts = MFXVideoENCODE_EncodeFrameAsync(mfx_session, NULL, pSurface,
			&t_TaskPool[nTaskIdx].mfxBS,
			&t_TaskPool[nTaskIdx].syncp);

		/*pSurface->FrameInterface->Release(pSurface);*/

		if (MFX_ERR_NONE < sts && !t_TaskPool[nTaskIdx].syncp) {
			/*MFXVideoCORE_SyncOperation(mfx_session, t_TaskPool[nTaskIdx].syncp, INFINITE);*/
			// Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(
					1); // Wait if device is busy, then repeat the same call
		}
		else if (MFX_ERR_NONE < sts && t_TaskPool[nTaskIdx].syncp) {
			sts = MFX_ERR_NONE; // Ignore warnings if output is available
			break;
		}
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			blog(LOG_INFO, "Encode error: Need more buffer size");
			// Allocate more bitstream buffer memory here if needed...
			break;
		}
		else if (sts == MFX_ERR_DEVICE_LOST) {
			blog(LOG_INFO, "Encode error: Hardware device lost");
			break;
		}
		else if (sts == MFX_ERR_MORE_DATA) {
			//blog(LOG_INFO, "Encode error: More data");
			break;
		}
		else if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) {
			blog(LOG_INFO, "Encode error: Incompability video params");
			break;
		}
		else if (sts == MFX_WRN_IN_EXECUTION) {
			blog(LOG_INFO, "Encode warning: In execution");

			//MFXVideoCORE_SyncOperation(mfx_session,
			//	t_TaskPool[n_FirstSyncTask].syncp, 10);
		}
		else if (sts == MFX_ERR_REALLOC_SURFACE) {
			blog(LOG_INFO, "Encode error: Need bigger surface");
			break;
		}
		else if (sts == MFX_ERR_MORE_BITSTREAM) {
			blog(LOG_INFO, "Encode error: Need more bitstream");
			break;
		}
		else if (sts == MFX_ERR_GPU_HANG) {
			blog(LOG_INFO, "Encode error: GPU hang");
			break;
		}
		else if (sts == MFX_TASK_BUSY) {
			blog(LOG_INFO, "Encode warn: task busy");
			MSDK_SLEEP(
				100);
			break;
		}
		else if (sts == MFX_ERR_DEVICE_FAILED) {
			blog(LOG_INFO, "Encode error: Device failed");
			break;
		}
		else {
			break;
		}
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain()
{
	mfxStatus sts = MFX_ERR_NONE;

	while (t_TaskPool && t_TaskPool[n_FirstSyncTask].syncp) {
		sts = MFXVideoCORE_SyncOperation(mfx_session,
			t_TaskPool[n_FirstSyncTask].syncp, 60000);
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

	mfx_FrameAllocator.Free(&mfx_FrameAllocator.pthis, &mfx_FrameAllocResponse);
	mfx_FrameAllocator.Free(&mfx_FrameAllocator, &mfx_FrameAllocResponse);
	mfx_FrameAllocator.Free(mfx_FrameAllocator.pthis, &mfx_FrameAllocResponse);
	MFXDispReleaseImplDescription(mfx_loader, g_DX_Handle);


	sts = MFXVideoENCODE_Close(mfx_session);
	MFXClose(mfx_session);
	MFXUnload(mfx_loader);

	mfx_session = nullptr;
	mfx_loader = nullptr;

	if (mfx_FrameSurf) {
		for (int i = 0; i < mU16_SurfNum; i++) {
			delete mfx_FrameSurf[i];
		}
		MSDK_SAFE_DELETE_ARRAY(mfx_FrameSurf);
		delete mfx_FrameSurf;
	}

	if (t_TaskPool) {
		for (int i = 0; i < mU16_TaskPool; i++)
			delete t_TaskPool[i].mfxBS.Data;
		MSDK_SAFE_DELETE_ARRAY(t_TaskPool);
		delete t_TaskPool;
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
		mfx_FrameAllocator.Free(g_DX_Handle, &mfx_FrameAllocResponse);
		g_DX_Handle = NULL;
	}

	return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Reset(qsv_param_t* pParams,
	enum qsv_codec codec)
{
	mfxStatus sts = ClearData();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = Open(pParams, codec);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	return sts;
}
