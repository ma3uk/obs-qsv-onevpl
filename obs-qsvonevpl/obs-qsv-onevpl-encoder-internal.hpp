#pragma once

#ifndef __QSV_VPL_ENCODER_INTERNAL_H__
#define __QSV_VPL_ENCODER_INTERNAL_H__
#endif

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif
#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif
#ifndef __QSV_VPL_EXT_BUF_MANAGER_H__
#include "helpers/ext_buf_manager.hpp"
#endif
#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#include "helpers/qsv_params.hpp"
#endif

class QSV_VPL_Encoder_Internal {
public:
  QSV_VPL_Encoder_Internal();
  ~QSV_VPL_Encoder_Internal();

  mfxStatus GetVPLVersion(mfxVersion &mfx_Version);
  mfxStatus Open(struct qsv_param_t *pParams, enum qsv_codec codec,
                 bool useTexAlloc);
  void GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
                    mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf);
  mfxStatus Encode(mfxU64 ts, uint8_t **frame_data, uint32_t *frame_linesize,
                   mfxBitstream **pBS);
  mfxStatus Encode_tex(mfxU64 ts, void *tex_handle, uint64_t lock_key,
                       uint64_t *next_key, mfxBitstream **pBS);
  mfxStatus ClearData();
  mfxStatus ReconfigureEncoder();
  void AddROI(mfxU32 left, mfxU32 top, mfxU32 right, mfxU32 bottom,
              mfxI16 delta);
  void ClearROI();
  bool UpdateParams(struct qsv_param_t *pParams);

protected:
  typedef struct Task {
    mfxBitstream mfxBS;
    mfxSyncPoint syncp;
  } Task;

  mfxStatus Initialize(enum qsv_codec codec, [[maybe_unused]] void **data, int GPUNum);

  mfxStatus InitVPPParams(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus InitEncParams(struct qsv_param_t *pParams, enum qsv_codec codec);

  mfxStatus GetVideoParam(enum qsv_codec codec);
  mfxStatus AllocateTextures();
  mfxStatus InitBitstream(enum qsv_codec codec);
  void ReleaseBitstream();
  mfxStatus InitTaskPool(enum qsv_codec codec);
  void ReleaseTask(int TaskID);
  void ReleaseTaskPool();
  mfxStatus ChangeBitstreamSize(mfxU32 NewSize);
  mfxStatus GetFreeTaskIndex(int *TaskID);

  void LoadFrameData(mfxFrameSurface1 *&surface, uint8_t **frame_data,
                     uint32_t *frame_linesize);

  mfxStatus Drain();

private:
  mfxPlatform mfx_Platform;
  mfxVersion mfx_Version;
  mfxLoader mfx_Loader;
  mfxConfig mfx_LoaderConfig[8];
  mfxVariant mfx_LoaderVariant[8];
  mfxSession mfx_Session;
#if defined(__linux__)
  void *mfx_SessionData;
#endif

  mfxFrameSurface1 *mfx_EncSurface;

  mfxFrameSurface1 *mfx_VPPSurface;

  MFXVideoENCODE* mfx_VideoEnc;
  MFXVideoVPP* mfx_VideoVPP;

  mfxU8 VPS_Buffer[1024];
  mfxU8 SPS_Buffer[1024];
  mfxU8 PPS_Buffer[1024];
  mfxU16 VPS_BufferSize;
  mfxU16 SPS_BufferSize;
  mfxU16 PPS_BufferSize;

  mfxBitstream mfx_Bitstream;
  //mfxU16 mfx_TaskPoolSize;
  std::vector<struct Task> mfx_TaskPool;
  int mfx_SyncTaskID;

  mfxVideoParam mfx_ResetParams;
  bool ResetParamChanged;

  mfx_VideoParam mfx_EncParams;
  mfx_VideoParam mfx_VPPParams;
  mfx_EncodeCtrl mfx_EncCtrlParams;

  mfxFrameAllocRequest mfx_AllocRequest;

  bool mfx_UseTexAlloc;
  mfxMemoryInterface *mfx_MemoryInterface;

  hw_handle* hw;

  bool mfx_VPP;
};