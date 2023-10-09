#pragma once

#ifndef __QSV_VPL_ENCODER_INTERNAL_H__
#define __QSV_VPL_ENCODER_INTERNAL_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#elif __linux__
#include <obs-nix-platform.h>
#include <va/va_drm.h>
#include <va/va_str.h>
#include <va/va_wayland.h>
#include <va/va_x11.h>
#endif

#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#include "helpers/qsv_params.hpp"
#endif
#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif
#ifndef __QSV_VPL_EXT_BUF_MANAGER_H__
#include "helpers/ext_buf_manager.hpp"
#endif
#ifndef __QSV_VPL_BITSTREAM_H__
#include "helpers/bitstream_manager.hpp"
#endif
#if defined(_WIN32) || defined(_WIN64)
#ifndef __QSV_VPL_D3D_COMMON_H__
#include "helpers/common_directx11.hpp"
#endif
#endif


class QSV_VPL_Encoder_Internal {
public:
  QSV_VPL_Encoder_Internal(mfxVersion &version, bool isDGPU);
  ~QSV_VPL_Encoder_Internal();

  mfxStatus Open(struct qsv_param_t *pParams, enum qsv_codec codec);
  void GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf, mfxU8 **pPPSBuf,
                    mfxU16 *pnVPSBuf, mfxU16 *pnSPSBuf, mfxU16 *pnPPSBuf);
  mfxStatus Encode(uint64_t ts, uint8_t *pDataY, uint8_t *pDataUV,
                   uint32_t strideY, uint32_t strideUV, mfxBitstream **pBS);
  mfxStatus Encode_tex(uint64_t ts, uint32_t tex_handle, uint64_t lock_key,
                       uint64_t *next_key, mfxBitstream **pBS);
  mfxStatus ClearData();
  mfxStatus Initialize(int deviceNum);
  mfxStatus Reset(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus GetCurrentFourCC(mfxU32 &fourCC);
  mfxStatus ReconfigureEncoder();
  bool UpdateParams(struct qsv_param_t *pParams);

  bool IsDGPU() const { return b_isDGPU; }

protected:
  struct Task {
    mfxBitstream *mfxBS;
    mfxSyncPoint syncp;
  };
  mfxStatus InitENCCtrlParams(struct qsv_param_t *pParams,
                              enum qsv_codec codec);
  mfxStatus InitENCParams(struct qsv_param_t *pParams, enum qsv_codec codec);
  mfxStatus GetVideoParam(enum qsv_codec codec);
  mfxStatus LoadNV12(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
                     uint8_t *pDataUV, uint32_t strideY, uint32_t strideUV);
  mfxStatus LoadP010(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
                     uint8_t *pDataUV, uint32_t strideY, uint32_t strideUV);
  mfxStatus LoadBGRA(mfxFrameSurface1 *pSurface, uint8_t *pDataY,
                     uint32_t strideY);
  mfxStatus Drain();
  // int GetFreeTaskIndex(std::vector<Task> &pTaskPool, mfxU16 &nPoolSize);

  static inline mfxU16 AVCGetMaxNumRefActivePL0(mfxU16 targetUsage, mfxU16 isLowPower,
                                  bool lookAHead, const mfxFrameInfo &info) {

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
                                  bool lookAHead, const mfxFrameInfo &info) {
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
#if defined(_WIN32) || defined(_WIN64)
  inline static QSV_VPL_D3D11 *D3D11Device;
#endif

private:
  mfxIMPL mfx_Impl;
  mfxPlatform mfx_Platform;
  mfxVersion mfx_Version;
  mfxLoader mfx_Loader;
  mfxConfig mfx_LoaderConfig;
  mfxVariant mfx_LoaderVariant;
  mfxSession mfx_Session;
  mfxFrameSurface1 *mfx_EncodeSurface;
  MFXVideoENCODE *mfx_VideoENC;
  mfxU8 VPS_Buffer[128];
  mfxU8 SPS_Buffer[512];
  mfxU8 PPS_Buffer[128];
  mfxU16 VPS_BufferSize;
  mfxU16 SPS_BufferSize;
  mfxU16 PPS_BufferSize;
  // mfxBitstream *mfx_Bitstream;

  QSV_VPL_Bitstream mfx_Bitstream;



  mfxVideoParam mfx_ResetParams;
  bool ResetParamChanged;
  mfx_VideoParam mfx_EncParams;
  mfx_EncodeCtrl encCTRL;

  mfxSyncPoint mfx_SyncPoint;
  mfxSyncPoint mfx_BufferedSyncPoint;

  inline static mfxU16 mfx_OpenEncodersNum;
  inline static mfxHDL mfx_DX_Handle;

  bool isD3D11;
  bool b_isDGPU;
};