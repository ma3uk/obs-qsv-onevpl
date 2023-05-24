#pragma once
#include "obs-qsv-onevpl-encoder.h"
#include "helpers/common_utils.h"
#include "mfx.h"

#include <vector>

class QSV_VPL_Encoder_Internal {
public:
	QSV_VPL_Encoder_Internal(mfxIMPL &impl, mfxVersion &version,
				 bool isDGPU);
	~QSV_VPL_Encoder_Internal();

	mfxStatus Open(qsv_param_t *pParams, enum qsv_codec codec);
	void GetSPSPPS(mfxU8 **pSPSBuf, mfxU8 **pPPSBuf, mfxU16 *pnSPSBuf,
		       mfxU16 *pnPPSBuf);
	void GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
			   mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf,
			   mfxU16 *pnPPSBuf);
	mfxStatus Encode(uint64_t ts, uint8_t *pDataY, uint8_t *pDataUV,
			 uint32_t strideY, uint32_t strideUV,
			 mfxBitstream **pBS);
	mfxStatus ClearData();
	mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, mfxSession *pSession,
			     mfxFrameAllocator *mfx_FrameAllocator,
			     mfxHDL *deviceHandle, int deviceNum);
	mfxStatus Reset(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus ReconfigureEncoder();
	bool UpdateParams(qsv_param_t *pParams);

	bool IsDGPU() const { return b_isDGPU; }

protected:
	mfxStatus InitEncParams(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus InitEncCtrlParams(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus AllocateSurfaces();
	mfxStatus GetVideoParam(enum qsv_codec codec);
	mfxStatus InitBitstream();
	mfxStatus LoadNV12(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
			   uint8_t *pDataUV, uint32_t strideY,
			   uint32_t strideUV);
	mfxStatus LoadP010(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
			   uint8_t *pDataUV, uint32_t strideY,
			   uint32_t strideUV);
	mfxStatus Drain();
	int GetFreeTaskIndex(Task *pTaskPool, mfxU16 nPoolSize);

private:
	mfxIMPL mfx_impl;
	mfxVersion mfx_version;
	mfxSession mfx_session;
	mfxLoader mfx_loader;
	mfxExtTuneEncodeQuality mfx_Tune;
	mfxEncodeCtrl mfx_EncCtrl;
	mfxBRCFrameCtrl mfx_BRCFrameCtrl;
	mfxFrameAllocator mfx_FrameAllocator;
	mfxFrameAllocRequest mfx_FrameAllocRequest;
	mfxVideoParam mfx_EncParams;
	mfxInitializationParam mfx_InitParams;
	mfxFrameAllocResponse mfx_FrameAllocResponse;
	mfxFrameSurface1 *mfx_FrameSurf;
	mfxU16 mU16_SurfNum;
	MFXVideoENCODE *mfx_VideoEnc;
	mfxExtEncToolsConfig mfx_EncToolsConf;
	mfxExtAV1BitstreamParam mfx_ExtAV1BitstreamParam;
	mfxExtAV1ResolutionParam mfx_ExtAV1ResolutionParam;
	mfxExtAV1TileParam mfx_ExtAV1TileParam;
	mfxExtCodingOptionVPS mfx_ExtCOVPS;
	mfxExtCodingOptionSPSPPS mfx_ExtCOSPSPPS;
	mfxU8 mU8_VPSBuffer[1024];
	mfxU8 mU8_SPSBuffer[1024];
	mfxU8 mU8_PPSBuffer[1024];
	mfxU16 mU16_VPSBufSize;
	mfxU16 mU16_SPSBufSize;
	mfxU16 mU16_PPSBufSize;
	mfxVideoParam mfx_VideoParams;
	mfxExtMVOverPicBoundaries mfx_ExtMVOverPicBoundaries;
	std::vector<mfxExtBuffer *> mfx_ExtendedBuffers;
	std::vector<mfxExtBuffer *> mfx_CtrlExtendedBuffers;
	mfxExtCodingOption3 mfx_co3;
	mfxExtCodingOption2 mfx_co2;
	mfxExtCodingOption mfx_co;
	mfxExtAvcTemporalLayers mfx_tl;
	mfxExtHEVCParam mfx_ExtHEVCParam{};
	mfxExtVideoSignalInfo mfx_ExtVideoSignalInfo{};
	mfxExtChromaLocInfo mfx_ExtChromaLocInfo{};
	mfxExtMasteringDisplayColourVolume mfx_ExtMasteringDisplayColourVolume{};
	mfxExtContentLightLevelInfo mfx_ExtContentLightLevelInfo{};
	mfxExtVPPDoUse mfx_VPPDoUse;
	mfxExtVPPDenoise2 mfx_VPPDenoise;
	mfxExtVPPScaling mfx_VPPScaling;
	mfxExtAVCRoundingOffset mfx_AVCRoundingOffset;
	mfxU16 mU16_TaskPool;
	Task *t_TaskPool;
	int n_TaskIdx;
	int n_FirstSyncTask;
	mfxBitstream mfx_Bitstream;
	bool b_isDGPU;
	mfxPayload mfx_PayLoad;
	mfxExtEncoderResetOption mfx_ResetOption;
	int g_numEncodersOpen;
	static mfxHDL
		g_DX_Handle; // we only want one handle for all instances to use;
};
