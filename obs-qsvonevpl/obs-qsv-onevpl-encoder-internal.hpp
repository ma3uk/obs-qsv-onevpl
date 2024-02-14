#pragma once

#ifndef __QSV_VPL_ENCODER_INTERNAL_H__
#define __QSV_VPL_ENCODER_INTERNAL_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "helpers/common_directx11.hpp"
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
  mfxStatus InitVPPParams(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus InitEncParams(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus InitDecParams(struct qsv_param_t *pParams, enum qsv_codec codec);

  mfxStatus GetVideoParam(enum qsv_codec codec);
  mfxStatus AllocateTextures();
  mfxStatus InitBitstream();
  void ReleaseBitstream();
  mfxStatus InitTaskPool();
  void ReleaseTask(int TaskID);
  void ReleaseTaskPool();
  mfxStatus ChangeBitstreamSize(mfxU32 NewSize);
  mfxStatus GetFreeTaskIndex(int *TaskID);

  void LoadFrameData(mfxFrameSurface1 *surface, uint8_t **frame_data,
                     uint32_t *frame_linesize);

  mfxStatus Drain();
  // int GetFreeTaskIndex(std::vector<Task> &pTaskPool, mfxU16 &nPoolSize);

  static inline mfxU16 AVCGetMaxNumRefActivePL0(mfxU16 targetUsage,
                                                mfxU16 isLowPower,
                                                bool lookAHead,
                                                const mfxFrameInfo &info) {

    constexpr mfxU16 DEFAULT_BY_TU[][8] = {
        {0, 8, 6, 3, 3, 3, 1, 1}, // VME progressive < 4k or interlaced
        {0, 4, 4, 3, 3, 3, 1, 1}, // VME progressive >= 4k
        {0, 2, 2, 2, 2, 2, 1, 1}, // VDEnc
        {0, 15, 8, 6, 4, 3, 2, 1}};

    if (isLowPower == MFX_CODINGOPTION_OFF) {
      if ((info.Width < 3840 && info.Height < 2160) ||
          (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)) {
        return DEFAULT_BY_TU[0][targetUsage];
      } else // progressive >= 4K
      {
        return DEFAULT_BY_TU[1][targetUsage];
      }
    } else {
      return DEFAULT_BY_TU[2][targetUsage];
    }
  }

  static inline mfxU16 AVCGetMaxNumRefActiveBL0(mfxU16 targetUsage,
                                                mfxU16 isLowPower,
                                                bool lookAHead) {
    constexpr mfxU16 DEFAULT_BY_TU[][8] = {{0, 4, 4, 2, 2, 2, 1, 1},
                                           {0, 2, 2, 2, 2, 2, 1, 1}};
    if (isLowPower == MFX_CODINGOPTION_OFF) {
      return DEFAULT_BY_TU[0][targetUsage];
    } else {
      if (lookAHead == true) {
        return 1;
      } else {
        return DEFAULT_BY_TU[0][targetUsage];
      }
    }
  }

  static inline mfxU16 AVCGetMaxNumRefActiveBL1(mfxU16 targetUsage,
                                                mfxU16 isLowPower,
                                                bool lookAHead,
                                                const mfxFrameInfo &info) {
    constexpr mfxU16 DEFAULT_BY_TU[] = {0, 2, 2, 2, 2, 2, 1, 1};
    if (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        isLowPower == MFX_CODINGOPTION_OFF) {
      return DEFAULT_BY_TU[targetUsage];
    } else {
      if (lookAHead == true) {
        return 1;
      } else {
        return DEFAULT_BY_TU[targetUsage];
      }
    }
  }

  static inline mfxU16 HEVCGetMaxNumRefActivePL0(mfxU16 targetUsage,
                                                 mfxU16 isLowPower,
                                                 const mfxFrameInfo &info) {

    constexpr mfxU16 DEFAULT_BY_TU[][8] = {
        {0, 4, 4, 3, 3, 1, 1, 1}, // VME progressive < 4k or interlaced
        {0, 4, 4, 3, 3, 1, 1, 1}, // VME progressive >= 4k
        {0, 3, 3, 3, 3, 3, 3, 3}  // VDEnc
    };

    if (isLowPower == MFX_CODINGOPTION_OFF) {
      if ((info.Width < 3840 && info.Height < 2160) ||
          (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)) {
        return DEFAULT_BY_TU[0][targetUsage];
      } else // progressive >= 4K
      {
        return DEFAULT_BY_TU[1][targetUsage];
      }
    } else {
      return DEFAULT_BY_TU[2][targetUsage];
    }
  }

  static inline mfxU16 HEVCGetMaxNumRefActiveBL0(mfxU16 targetUsage,
                                                 mfxU16 isLowPower) {
    if (isLowPower == MFX_CODINGOPTION_OFF) {
      constexpr mfxU16 DEFAULT_BY_TU[][8] = {{0, 4, 4, 3, 3, 3, 1, 1}};
      return DEFAULT_BY_TU[0][targetUsage];
    } else {
      return 2;
    }
  }

  static inline mfxU16 HEVCGetMaxNumRefActiveBL1(mfxU16 targetUsage,
                                                 mfxU16 isLowPower,
                                                 const mfxFrameInfo &info) {
    if (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        isLowPower == MFX_CODINGOPTION_OFF) {
      constexpr mfxU16 DEFAULT_BY_TU[] = {0, 2, 2, 1, 1, 1, 1, 1};
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
  mfxConfig mfx_LoaderConfig;
  mfxVariant mfx_LoaderVariant;
  mfxSession mfx_Session;
  void *mfx_SessionData;
  // mfxFrameSurface1 *mfx_DecodeSurface;

  mfxFrameSurface1 *mfx_Surface;
  mfxFrameSurface1 *mfx_VPPInSurface;
  mfxFrameSurface1 *mfx_VPPOutSurface;
  MFXVideoENCODE *mfx_VideoEnc;
  MFXVideoVPP *mfx_VideoVPP;
  // MFXVideoDECODE *mfx_VideoDec;
  mfxU8 VPS_Buffer[2048];
  mfxU8 SPS_Buffer[2048];
  mfxU8 PPS_Buffer[2048];
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

  static mfxU16 mfx_OpenEncodersNum;
  static mfxHDL mfx_GFXHandle;

  //mfxFrameSurface1 **mfx_SurfacePool;
  mfxFrameAllocRequest mfx_AllocRequest;
  mfxFrameAllocResponse mfx_AllocResponse;

  bool mfx_UseD3D11;
  bool mfx_UseTexAlloc;
  mfxMemoryInterface *mfx_MemoryInterface;
  mfxSurfaceD3D11Tex2D mfx_TextureInfo;
  int mfx_TextureCounter;
  bool b_isDGPU;

  bool mfx_VPP;
};