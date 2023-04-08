#pragma once
#include "QSV_Encoder.h"
#include "common_utils.h"

#include <vector>

class QSV_VPL_Encoder_Internal {
public:
	QSV_VPL_Encoder_Internal(mfxIMPL& impl, mfxVersion& version, bool isDGPU);
	~QSV_VPL_Encoder_Internal();

	mfxStatus Open(qsv_param_t* pParams, enum qsv_codec codec);
	void GetSPSPPS(mfxU8** pSPSBuf, mfxU8** pPPSBuf, mfxU16* pnSPSBuf,
		mfxU16* pnPPSBuf);
	void GetVpsSpsPps(mfxU8** pVPSBuf, mfxU8** pSPSBuf, mfxU8** pPPSBuf,
		mfxU16* pnVPSBuf, mfxU16* pnSPSBuf, mfxU16* pnPPSBuf);
	mfxStatus Encode(uint64_t ts, uint8_t* pDataY, uint8_t* pDataUV,
		uint32_t strideY, uint32_t strideUV,
		mfxBitstream** pBS);
	mfxStatus Encode_tex(uint64_t ts, uint32_t tex_handle,
		uint64_t lock_key, uint64_t* next_key,
		mfxBitstream** pBS);
	mfxStatus ClearData();
	mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, mfxSession* pSession,
		mfxFrameAllocator* mfx_FrameAllocator, mfxHDL* deviceHandle);
	mfxStatus Reset(qsv_param_t* pParams, enum qsv_codec codec);
	mfxStatus ReconfigureEncoder();
	bool UpdateParams(qsv_param_t* pParams);

	bool IsDGPU() const { return b_isDGPU; }

protected:
	mfxStatus InitParams(qsv_param_t* pParams, enum qsv_codec codec);
	mfxStatus AllocateSurfaces();
	mfxStatus GetVideoParam(enum qsv_codec codec);
	mfxStatus InitBitstream();
	mfxStatus LoadNV12(mfxFrameSurface1* pSurface, uint8_t* pDataY,
		uint8_t* pDataUV, uint32_t strideY,
		uint32_t strideUV);
	mfxStatus LoadP010(mfxFrameSurface1* pSurface, uint8_t* pDataY,
		uint8_t* pDataUV, uint32_t strideY,
		uint32_t strideUV);
	mfxStatus Drain();
	int GetFreeTaskIndex(Task* pTaskPool, mfxU16 nPoolSize);

private:
	mfxIMPL mfx_impl;
	mfxVersion mfx_version;
	mfxSession mfx_session;
	mfxExtTuneEncodeQuality mfx_Tune;
	mfxEncodeCtrl mfx_EncControl;
	mfxFrameAllocator mfx_FrameAllocator;
	mfxVideoParam mfx_EncParams;
	mfxFrameAllocResponse mfx_FrameAllocResponse;
	mfxFrameSurface1** mfx_FrameSurf;
	mfxU16 mU16_SurfNum;
	MFXVideoENCODE* mfx_VideoEnc;
	mfxU8 mU8_VPSBuffer[1024];
	mfxU8 mU8_SPSBuffer[1024];
	mfxU8 mU8_PPSBuffer[1024];
	mfxU16 mU16_VPSBufSize;
	mfxU16 mU16_SPSBufSize;
	mfxU16 mU16_PPSBufSize;
	mfxVideoParam mfx_VideoParams;
	mfxExtMVOverPicBoundaries mfx_MVOP;
	std::vector<mfxExtBuffer*> extendedBuffers;
	mfxExtCodingOption3 mfx_co3;
	mfxExtCodingOption2 mfx_co2;
	mfxExtCodingOption mfx_co;
	mfxExtAvcTemporalLayers mfx_tl;
	mfxExtHEVCParam mfx_ExtHEVCParam{};
	mfxExtVideoSignalInfo mfx_ExtVideoSignalInfo{};
	mfxExtChromaLocInfo mfx_ExtChromaLocInfo{};
	mfxExtMasteringDisplayColourVolume mfx_ExtMasteringDisplayColourVolume{};
	mfxExtContentLightLevelInfo mfx_ExtContentLightLevelInfo{};
	mfxU16 mU16_TaskPool;
	Task* t_TaskPool;
	int n_TaskIdx;
	int n_FirstSyncTask;
	mfxBitstream mfx_Bitstream;
	bool b_isDGPU;
	static mfxU16 g_numEncodersOpen;
	mfxPayload mfx_PayLoad;
	static mfxHDL
		g_DX_Handle; // we only want one handle for all instances to use;
};
