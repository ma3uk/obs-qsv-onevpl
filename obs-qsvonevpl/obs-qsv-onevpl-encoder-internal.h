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
			  mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf);
	mfxStatus Encode(uint64_t ts, uint8_t *pDataY, uint8_t *pDataUV,
			 uint32_t strideY, uint32_t strideUV,
			 mfxBitstream **pBS);
	mfxStatus ClearData();
	mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, int deviceNum);
	mfxStatus Reset(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus ReconfigureEncoder();
	bool UpdateParams(qsv_param_t *pParams);

	bool IsDGPU() const { return b_isDGPU; }

protected:
	mfxStatus InitVPPParams(qsv_param_t *pParams, qsv_codec codec);
	mfxStatus InitENCParams(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus InitENCCtrlParams(qsv_param_t *pParams, enum qsv_codec codec);
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
	mfxIMPL mfx_Impl;
	mfxVersion mfx_Version;
	mfxSession mfx_Session;
	mfxLoader mfx_Loader;
	/*mfxExtTuneEncodeQuality mfx_Tune;*/
	mfxBRCFrameCtrl mfx_BRCFrameCtrl;
	mfxVideoParam mfx_ENC_Params;
	mfxVideoParam mfx_VPP_Params;
	mfxInitializationParam mfx_InitParams;
	MFXVideoENCODE *mfx_VideoENC;
	mfxEncodeCtrl mfx_ENCCtrl_Params;
	MFXVideoVPP *mfx_VideoVPP;
	mfxExtEncToolsConfig mfx_EncToolsConf;
	mfxExtAV1BitstreamParam mfx_Ext_AV1BitstreamParam;
	mfxExtAV1ResolutionParam mfx_Ext_AV1ResolutionParam;
	mfxExtAV1TileParam mfx_Ext_AV1TileParam;
	mfxExtCodingOptionVPS mfx_Ext_CO_VPS;
	mfxExtCodingOptionSPSPPS mfx_Ext_CO_SPSPPS;
	mfxU8 VPS_Buffer[1024];
	mfxU8 SPS_Buffer[1024];
	mfxU8 PPS_Buffer[1024];
	mfxU16 VPS_BufSize;
	mfxU16 SPS_BufSize;
	mfxU16 PPS_BufSize;
	mfxExtMVOverPicBoundaries mfx_Ext_MVOverPicBoundaries;
	std::vector<mfxExtBuffer *> mfx_ENC_ExtendedBuffers;
	std::vector<mfxExtBuffer *> mfx_ENCCtrl_ExtendedBuffers;
	std::vector<mfxExtBuffer *> mfx_VPP_ExtendedBuffers;
	mfxExtCodingOption3 mfx_Ext_CO3;
	mfxExtCodingOption2 mfx_Ext_CO2;
	mfxExtCodingOption mfx_Ext_CO;
	mfxExtAVCRefListCtrl mfx_Ext_AVCRefListCtrl;
	mfxExtAVCRefLists mfx_Ext_AVCRefLists;
	mfxExtHEVCParam mfx_Ext_HEVCParam{};
	mfxExtVideoSignalInfo mfx_Ext_VideoSignalInfo{};
	mfxExtChromaLocInfo mfx_Ext_ChromaLocInfo{};
	mfxExtMasteringDisplayColourVolume mfx_Ext_MasteringDisplayColourVolume{};
	mfxExtContentLightLevelInfo mfx_Ext_ContentLightLevelInfo{};
	mfxExtVPPDoUse mfx_Ext_VPPDoUse;
	mfxExtVPPDenoise2 mfx_Ext_VPPDenoise;
	mfxExtVPPScaling mfx_Ext_VPPScaling;
	mfxExtAVCRoundingOffset mfx_Ext_AVCRoundingOffset;
	mfxU16 mfx_TaskPool;
	Task *t_TaskPool;
	int n_TaskIdx;
	int n_FirstSyncTask;
	mfxBitstream mfx_Bitstream;
	bool b_isDGPU;
	mfxPayload mfx_PayLoad;
	mfxExtEncoderResetOption mfx_Ext_ResetOption;
	int g_numEncodersOpen;
	static mfxHDL
		g_DX_Handle; // we only want one handle for all instances to use;
	mfxPlatform mfx_Platform;
	mfxExtHyperModeParam mfx_HyperModeParam;
};
