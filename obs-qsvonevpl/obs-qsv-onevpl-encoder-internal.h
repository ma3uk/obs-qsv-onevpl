#include "obs-qsv-onevpl-encoder.h"
#include "helpers/common_utils.h"
#include "helpers/ext_buf_manager.h"
#ifndef __MFX_H__
#include "mfx.h"
#endif

#include <vector>

class QSV_VPL_Encoder_Internal {
public:
	QSV_VPL_Encoder_Internal(mfxVersion &version, bool isDGPU);
	~QSV_VPL_Encoder_Internal();

	mfxStatus Open(qsv_param_t *pParams, enum qsv_codec codec);
	void GetSPSPPS(mfxU8 **pSPSBuf, mfxU8 **pPPSBuf, mfxU16 *pnSPSBuf,
		       mfxU16 *pnPPSBuf);
	void GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
			  mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf);
	mfxStatus Encode(uint64_t ts, uint8_t *pDataY, uint8_t *pDataUV,
			 uint32_t strideY, uint32_t strideUV,
			 mfxBitstream **pBS);
	mfxStatus Encode_tex(uint64_t ts, uint32_t tex_handle,
			     uint64_t lock_key, uint64_t *next_key,
			     mfxBitstream **pBS);
	mfxStatus ClearData();
	mfxStatus Initialize(mfxFrameAllocator *FrameAllocator,
			     mfxU32 deviceNum);
	mfxStatus Reset(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus GetCurrentFourCC(mfxU32 &fourCC);
	mfxStatus ReconfigureEncoder();
	bool UpdateParams(qsv_param_t *pParams);

	bool IsDGPU() const { return b_isDGPU; }

protected:

	struct Task {
		mfxBitstream mfxBS;
		mfxSyncPoint syncp;
	};

	mfxStatus InitENCParams(qsv_param_t *pParams, enum qsv_codec codec);
	mfxStatus AllocateSurfaces();
	mfxStatus GetVideoParam(enum qsv_codec codec);
	mfxStatus InitBitstream();
	mfxStatus LoadNV12(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
			   uint8_t *pDataUV, uint32_t strideY,
			   uint32_t strideUV);
	mfxStatus LoadP010(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
			   uint8_t *pDataUV, uint32_t strideY,
			   uint32_t strideUV);
	mfxStatus LoadBGRA(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
			   uint32_t strideY);
	mfxStatus Drain();
	int GetFreeTaskIndex(Task *pTaskPool, mfxU16 nPoolSize);
	int GetFreeSurfaceIndex(mfxFrameSurface1 **pSurfacesPool,
				mfxU16 nPoolSize);

	mfxU16 AVCGetMaxNumRefActivePL0(mfxU16 targetUsage, mfxU16 isLowPower,
					const mfxFrameInfo &info)
	{

		constexpr mfxU16 DEFAULT_BY_TU[][8] = {
			{0, 8, 6, 3, 3, 3, 1,
			 1}, // VME progressive < 4k or interlaced
			{0, 4, 4, 3, 3, 3, 1, 1}, // VME progressive >= 4k
			{0, 2, 2, 2, 2, 2, 1, 1}  // VDEnc
		};

		if (isLowPower == MFX_CODINGOPTION_OFF) {
			if ((info.Width < 3840 && info.Height < 2160) ||
			    (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)) {
				return DEFAULT_BY_TU[0][targetUsage];
			} else //progressive >= 4K
			{
				return DEFAULT_BY_TU[1][targetUsage];
			}
		} else {
			return DEFAULT_BY_TU[2][targetUsage];
		}
	}

	mfxU16 AVCGetMaxNumRefActiveBL0(mfxU16 targetUsage, mfxU16 isLowPower)
	{
		if (isLowPower == MFX_CODINGOPTION_OFF) {
			constexpr mfxU16 DEFAULT_BY_TU[][8] = {
				{0, 4, 4, 2, 2, 2, 1, 1}};
			return DEFAULT_BY_TU[0][targetUsage];
		} else {
			return 1;
		}
	}

	mfxU16 AVCGetMaxNumRefActiveBL1(mfxU16 targetUsage, mfxU16 isLowPower,
					const mfxFrameInfo &info)
	{
		if (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
		    isLowPower == MFX_CODINGOPTION_OFF) {
			constexpr mfxU16 DEFAULT_BY_TU[] = {0, 2, 2, 2,
							    2, 2, 1, 1};
			return DEFAULT_BY_TU[targetUsage];
		} else {
			return 1;
		}
	}

	mfxU16 HEVCGetMaxNumRefActivePL0(mfxU16 targetUsage, mfxU16 isLowPower,
					 const mfxFrameInfo &info)
	{

		constexpr mfxU16 DEFAULT_BY_TU[][8] = {
			{0, 4, 4, 3, 3, 1, 1,
			 1}, // VME progressive < 4k or interlaced
			{0, 4, 4, 3, 3, 1, 1, 1}, // VME progressive >= 4k
			{0, 3, 3, 3, 3, 3, 3, 3}  // VDEnc
		};

		if (isLowPower == MFX_CODINGOPTION_OFF) {
			if ((info.Width < 3840 && info.Height < 2160) ||
			    (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)) {
				return DEFAULT_BY_TU[0][targetUsage];
			} else //progressive >= 4K
			{
				return DEFAULT_BY_TU[1][targetUsage];
			}
		} else {
			return DEFAULT_BY_TU[2][targetUsage];
		}
	}

	mfxU16 HEVCGetMaxNumRefActiveBL0(mfxU16 targetUsage, mfxU16 isLowPower)
	{
		if (isLowPower == MFX_CODINGOPTION_OFF) {
			constexpr mfxU16 DEFAULT_BY_TU[][8] = {
				{0, 4, 4, 3, 3, 3, 1, 1}};
			return DEFAULT_BY_TU[0][targetUsage];
		} else {
			return 2;
		}
	}

	mfxU16 HEVCGetMaxNumRefActiveBL1(mfxU16 targetUsage, mfxU16 isLowPower,
					 const mfxFrameInfo &info)
	{
		if (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
		    isLowPower == MFX_CODINGOPTION_OFF) {
			constexpr mfxU16 DEFAULT_BY_TU[] = {0, 2, 2, 1,
							    1, 1, 1, 1};
			return DEFAULT_BY_TU[targetUsage];
		} else {
			return 1;
		}
	}


private:
	mfxIMPL mfx_Impl;
	mfxPlatform mfx_Platform;
	mfxVersion mfx_Version;
	mfxLoader mfx_Loader;
	mfxConfig mfx_LoaderConfig[10];
	mfxVariant mfx_LoaderVariant[10];
	mfxSession mfx_Session;
	mfxFrameAllocator mfx_FrameAllocator;
	mfxFrameAllocRequest mfx_FrameAllocRequest;
	//mfxVideoParam mfx_ENC_Params;
	mfxFrameAllocResponse mfx_FrameAllocResponse;
	mfxFrameSurface1 **mfx_SurfacePool;
	mfxU16 n_SurfaceNum;
	MFXVideoENCODE *mfx_VideoENC;
	mfxU8 VPS_Buffer[1024];
	mfxU8 SPS_Buffer[1024];
	mfxU8 PPS_Buffer[1024];
	mfxU16 VPS_BufferSize;
	mfxU16 SPS_BufferSize;
	mfxU16 PPS_BufferSize;
	mfxU16 n_TaskNum;
	Task *t_TaskPool;
	//int n_TaskIdx;
	int n_FirstSyncTask;
	mfxBitstream mfx_Bitstream;
	bool b_isDGPU;
	mfx_VideoParam m_mfxEncParams;
};
