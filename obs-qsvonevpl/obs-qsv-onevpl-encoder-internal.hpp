#pragma once

#ifndef __QSV_VPL_ENCODER_INTERNAL_H__
#define __QSV_VPL_ENCODER_INTERNAL_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "helpers/hw_d3d11.hpp"
#elif defined(__linux__)
#include <obs-nix-platform.h>
#include <va/va_wayland.h>
#include <va/va_x11.h>
#endif

#include <chrono>
#include <thread>

#include "obs-qsv-onevpl-encoder.hpp"
// #include "helpers/bitstream_manager.hpp"
#include "helpers/common_utils.hpp"
#include "helpers/ext_buf_manager.hpp"
// #include "helpers/task_manager.hpp"
#include "helpers/qsv_params.hpp"

class QSV_VPL_Encoder_Internal {
public:
  QSV_VPL_Encoder_Internal(mfxVersion &version, bool isDGPU);
  ~QSV_VPL_Encoder_Internal();

  mfxStatus Open(struct qsv_param_t *pParams, enum qsv_codec codec);
  void GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
                    mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf);
  mfxStatus Encode(mfxU64 ts, uint8_t **frame_data, uint32_t *frame_linesize,
                   mfxBitstream **pBS);
  mfxStatus Encode_tex(mfxU64 ts, uint32_t tex_handle, uint64_t lock_key,
                       uint64_t *next_key, mfxBitstream **pBS);
  mfxStatus ClearData();
  mfxStatus Reset(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus GetCurrentFourCC(mfxU32 &fourCC);
  mfxStatus ReconfigureEncoder();
  void AddROI(mfxU32 left, mfxU32 top, mfxU32 right, mfxU32 bottom,
              mfxI16 delta);
  void ClearROI();
  bool UpdateParams(struct qsv_param_t *pParams);

  bool IsDGPU() const { return b_isDGPU; }

  // mfxStatus AllocateVPPSurfaces();

protected:
  typedef struct {
    mfxBitstream mfxBS;
    mfxSyncPoint syncp;
  } Task;

  mfxStatus Initialize(int deviceNum, [[maybe_unused]] enum qsv_codec codec,
                       [[maybe_unused]] void **data);

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
  mfxStatus GetFreeSurfaceIndex(int *SurfaceID);

  void LoadFrameData(mfxFrameSurface1 *&surface, uint8_t **frame_data,
                     uint32_t *frame_linesize);

  mfxStatus Drain();

private:
  mfxIMPL mfx_Impl;
  mfxPlatform mfx_Platform;
  mfxVersion mfx_Version;
  mfxLoader mfx_Loader;
  mfxConfig mfx_LoaderConfig[7];
  mfxVariant mfx_LoaderVariant[7];
  mfxSession mfx_Session;
  void *mfx_SessionData;

  mfxFrameSurface1 *mfx_EncSurface;
  mfxU32 mfx_EncSurfaceRefCount;

  mfxFrameSurface1 *mfx_VPPSurface;

  MFXVideoENCODE *mfx_VideoEnc;
  MFXVideoVPP *mfx_VideoVPP;

  mfxU8 VPS_Buffer[1024];
  mfxU8 SPS_Buffer[1024];
  mfxU8 PPS_Buffer[1024];
  mfxU16 VPS_BufferSize;
  mfxU16 SPS_BufferSize;
  mfxU16 PPS_BufferSize;

  mfxBitstream mfx_Bitstream;
  mfxU16 mfx_TaskPoolSize;
  Task *mfx_TaskPool;
  int mfx_SyncTaskID;

  mfxVideoParam mfx_ResetParams;
  bool ResetParamChanged;

  mfx_VideoParam mfx_EncParams;
  mfx_VideoParam mfx_VPPParams;
  mfx_EncodeCtrl mfx_EncCtrlParams;

  std::vector<mfxFrameSurface1 *> mfx_SurfacePool;
  mfxFrameAllocRequest mfx_AllocRequest;
  mfxFrameAllocResponse mfx_AllocResponse;

  bool mfx_UseD3D11;
  bool mfx_UseTexAlloc;
  mfxMemoryInterface *mfx_MemoryInterface;
#if defined(_WIN32) || defined(_WIN64)
  mfxSurfaceD3D11Tex2D mfx_TextureInfo;
#else
  mfxSurfaceVAAPI mfx_TextureInfo;
#endif
  int mfx_TextureCounter;
  bool b_isDGPU;

  std::shared_ptr<hw_handle> hw;

  bool mfx_VPP;
};