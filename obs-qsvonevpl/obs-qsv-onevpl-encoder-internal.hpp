#pragma once

#ifndef __QSVEncoder_H__
#define __QSVEncoder_H__
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

class QSVEncoder {
public:
  QSVEncoder();
  ~QSVEncoder();

  mfxStatus GetVPLVersion(mfxVersion &);
  mfxStatus Init(struct encoder_params *InputParams, enum codec_enum Codec,
                 bool IsTextureEncoder);
  mfxStatus EncodeFrame(mfxU64 TS, uint8_t **FrameData, uint32_t *FrameLinesize,
                        mfxBitstream **Bitstream);
  mfxStatus EncodeTexture(mfxU64 TS, void *TextureHandle, uint64_t LockKey,
                          uint64_t *NextKey, mfxBitstream **Bitstream);
  mfxStatus ClearData();
  mfxStatus ReconfigureEncoder();
  bool UpdateParams(struct encoder_params *InputParams);

protected:
  typedef struct Task {
    mfxBitstream Bitstream;
    mfxSyncPoint SyncPoint;
  } Task;

  mfxStatus CreateSession(enum codec_enum Codec, [[maybe_unused]] void **Data,
                          int GPUNum);

  mfxStatus SetProcessingParams(struct encoder_params *InputParams,
                                enum codec_enum Codec);
  mfxStatus SetEncoderParams(struct encoder_params *InputParams,
                             enum codec_enum Codec);

  mfxStatus GetVideoParam(enum codec_enum Codec);
  mfxStatus InitTexturePool();
  mfxStatus InitBitstreamBuffer(enum codec_enum Codec);
  void ReleaseBitstream();
  mfxStatus InitTaskPool(enum codec_enum Codec);
  void ReleaseTask(int TaskID);
  void ReleaseTaskPool();
  mfxStatus ChangeBitstreamSize(mfxU32 NewSize);
  mfxStatus GetFreeTaskIndex(int *TaskID);

  void LoadFrameData(mfxFrameSurface1 *&Surface, uint8_t **FrameData,
                     uint32_t *FrameLinesize);

  mfxStatus Drain();

  template <typename T>
  static inline T GetTriState(const std::optional<bool> &Value,
                              const T DefaultValue, const T OnValue,
                              const T OffValue) {
    if (!Value.has_value()) {
      return DefaultValue;
    }
    return Value.value() ? OnValue : OffValue;
  }

  static inline mfxU16 GetCodingOpt(const std::optional<bool> &Value) {
    return static_cast<mfxU16>(GetTriState(Value, MFX_CODINGOPTION_UNKNOWN,
                                           MFX_CODINGOPTION_ON,
                                           MFX_CODINGOPTION_OFF));
  }

  static inline std::string GetCodingOptStatus(const mfxU16 &Value) {
    if (Value == MFX_CODINGOPTION_ON) {
      return "ON";
    } else if (Value == MFX_CODINGOPTION_OFF) {
      return "OFF";
    } else {
      return "AUTO";
    }
  }

private:
  mfxPlatform QSVPlatform;
  mfxVersion QSVVersion;
  mfxLoader QSVLoader;
  mfxConfig QSVLoaderConfig[8];
  mfxVariant QSVLoaderVariant[8];
  mfxSession QSVSession;
  mfxIMPL QSVImpl;
#if defined(__linux__)
  void *QSVSessionData;
#endif

  mfxFrameSurface1 *QSVEncodeSurface;

  mfxFrameSurface1 *QSVProcessingSurface;

  std::unique_ptr<MFXVideoENCODE> QSVEncode;
  std::unique_ptr<MFXVideoVPP> QSVProcessing;

  mfxU8 QSVVPSBuffer[1024];
  mfxU8 QSVSPSBuffer[1024];
  mfxU8 QSVPPSBuffer[1024];
  mfxU16 QSVVPSBufferSize;
  mfxU16 QSVSPSBufferSize;
  mfxU16 QSVPPSBufferSize;

  mfxBitstream QSVBitstream;
  // mfxU16 MFXTaskPoolSize;
  std::vector<struct Task> QSVTaskPool;
  int QSVSyncTaskID;

  mfxVideoParam QSVResetParams;
  bool QSVResetParamsChanged;

  MFXVideoParam QSVEncodeParams;
  MFXVideoParam QSVProcessingParams;
  MFXEncodeCtrl QSVEncodeCtrlParams;

  mfxFrameAllocRequest QSVAllocateRequest;

  bool QSVIsTextureEncoder;
  mfxMemoryInterface *QSVMemoryInterface;

  std::unique_ptr<class HWManager> HWManager;

  bool QSVProcessingEnable;

  enum class AdditionalFourCC {
    MFX_FOURCC_IMC3 = MFX_MAKEFOURCC('I', 'M', 'C', '3'),
    MFX_FOURCC_YUV400 = MFX_MAKEFOURCC('4', '0', '0', 'P'),
    MFX_FOURCC_YUV411 = MFX_MAKEFOURCC('4', '1', '1', 'P'),
    MFX_FOURCC_YUV422H = MFX_MAKEFOURCC('4', '2', '2', 'H'),
    MFX_FOURCC_YUV422V = MFX_MAKEFOURCC('4', '2', '2', 'V'),
    MFX_FOURCC_YUV444 = MFX_MAKEFOURCC('4', '4', '4', 'P'),
    MFX_FOURCC_RGBP24 = MFX_MAKEFOURCC('R', 'G', 'B', 'P'),
  };

  static const inline std::vector<int> ListAllowedGopRefDist{1, 2, 4, 8, 16};
};