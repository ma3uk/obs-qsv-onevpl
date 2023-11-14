#pragma once

#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#define __QSV_VPL_ENCODER_PARAMS_H__
#endif

#ifndef __MFX_H__
#include <vpl/mfx.h>
#endif

#ifndef _STRING_
#include <string>
#endif

#ifndef _OPTIONAL_
#include <optional>
#endif

static const char *const qsv_ratecontrols_h264[] = {"CBR", "VBR", "CQP", "ICQ",
                                                    0};

static const char *const qsv_ratecontrols_vp9[] = {"CBR", "VBR", "CQP", "ICQ",
                                                   0};

static const char *const qsv_ratecontrols_hevc[] = {"CBR", "VBR", "CQP", "ICQ",
                                                    0};

static const char *const qsv_ratecontrols_av1[] = {"CBR", "VBR", "CQP", "ICQ",
                                                   0};

static const char *const qsv_profile_names_h264[] = {
    "high", "main", "baseline", "extended", "high10", "high422", 0};

static const char *const qsv_profile_names_av1[] = {"main", 0};

static const char *const qsv_profile_names_hevc[] = {"main", "main10", "rext",
                                                     0};

static const char *const qsv_profile_tiers_hevc[] = {"main", "high", 0};

static const char *const qsv_usage_names[] = {
    "TU1 (Veryslow)", "TU2 (Slower)", "TU3 (Slow)",     "TU4 (Balanced)",
    "TU5 (Fast)",     "TU6 (Faster)", "TU7 (Veryfast)", 0};

static const char *const qsv_latency_names[] = {"ultra-low", "low", "normal",
                                                0};

static const char *const qsv_params_condition[] = {"ON", "OFF", 0};

static const char *const qsv_params_condition_tristate[] = {"ON", "OFF", "AUTO",
                                                            0};

static const char *const qsv_params_condition_p_pyramid[] = {"SIMPLE",
                                                             "PYRAMID", 0};

static const char *const qsv_params_condition_extbrc[] = {"ON", "OFF", 0};

static const char *const qsv_params_condition_intra_ref_encoding[] = {
    "VERTICAL", "HORIZONTAL", 0};

static const char *const qsv_params_condition_mv_cost_scaling[] = {
    "DEFAULT", "1/2", "1/4", "1/8", "AUTO", 0};

static const char *const qsv_params_condition_lookahead_mode[] = {
    "LOW POWER", "HIGH QUALITY", "OFF", 0};

static const char *const qsv_params_condition_lookahead_latency[] = {
    "NORMAL", "HIGH", "LOW", "VERYLOW", 0};

static const char *const qsv_params_condition_lookahead_ds[] = {
    "SLOW", "MEDIUM", "FAST", "AUTO", 0};

static const char *const qsv_params_condition_trellis[] = {
    "OFF", "I", "IP", "IPB", "IB", "P", "PB", "B", "AUTO", 0};

static const char *const qsv_params_condition_hevc_sao[] = {
    "AUTO", "DISABLE", "LUMA", "CHROMA", "ALL", 0};

static const char *const qsv_params_condition_tune_quality[] = {
    "DEFAULT", "PSNR", "SSIM", "MS SSIM", "VMAF", "PERCEPTUAL", "OFF", 0};

static const char *const qsv_params_condition_denoise_mode[] = {
    "DEFAULT",
    "AUTO | BDRATE | PRE ENCODE",
    "AUTO | ADJUST | POST ENCODE",
    "AUTO | SUBJECTIVE | PRE ENCODE",
    "MANUAL | PRE ENCODE",
    "MANUAL | POST ENCODE",
    "OFF",
    0};

struct qsv_param_t {
  mfxU16 nTargetUsage; /* 1 through 7, 1 being best quality and 7
                                  being the best speed */
  mfxU16 nWidth;       /* source picture width */
  mfxU16 nHeight;      /* source picture height */
  mfxU16 nAsyncDepth;
  mfxU16 nFpsNum;
  mfxU16 nFpsDen;
  mfxU16 nTargetBitRate;
  mfxU16 nMaxBitRate;
  mfxU16 nBufferSize;
  mfxU16 CodecProfile;
  mfxU16 HEVCTier;
  mfxU16 RateControl;
  mfxU16 nQPI;
  mfxU16 nQPP;
  mfxU16 nQPB;
  mfxU16 nLADepth;
  mfxU16 nKeyIntSec;
  mfxU16 nGOPRefDist;
  mfxU16 nICQQuality;
  mfxU16 VideoFormat;
  mfxU16 VideoFullRange;
  mfxU16 ColourPrimaries;
  mfxU16 TransferCharacteristics;
  mfxU16 MatrixCoefficients;
  mfxU16 ChromaSampleLocTypeTopField;
  mfxU16 ChromaSampleLocTypeBottomField;
  mfxU16 DisplayPrimariesX[3];
  mfxU16 DisplayPrimariesY[3];
  mfxU16 WhitePointX;
  mfxU16 WhitePointY;
  mfxU32 MaxDisplayMasteringLuminance;
  mfxU32 MinDisplayMasteringLuminance;
  mfxU16 MaxContentLightLevel;
  mfxU16 MaxPicAverageLightLevel;
  mfxU16 nIntraRefCycleSize;
  mfxU16 nCTU;
  mfxU16 nWinBRCMaxAvgSize;
  mfxU16 nWinBRCSize;
  mfxU16 nNumRefFrame;
  mfxU16 nDenoiseStrength;

  mfxI16 nIntraRefQPDelta;

  std::optional<bool> bQualityEnchance;
  std::optional<bool> bMBBRC;
  std::optional<bool> bAdaptiveI;
  std::optional<bool> bAdaptiveB;
  std::optional<bool> bAdaptiveRef;
  std::optional<bool> bAdaptiveCQM;
  std::optional<bool> bAdaptiveLTR;
  std::optional<bool> bAdaptiveMaxFrameSize;
  std::optional<bool> bRDO;
  std::optional<bool> bRawRef;
  std::optional<bool> bGPB;
  std::optional<bool> bDirectBiasAdjustment;
  std::optional<bool> bGopOptFlag;
  std::optional<bool> bWeightedPred;
  std::optional<bool> bWeightedBiPred;
  std::optional<bool> bGlobalMotionBiasAdjustment;
  std::optional<bool> bLookaheadLP;
  std::optional<bool> bHRDConformance;
  
  bool bPPyramid;
  bool bExtBRC;
  bool bIntraRefEncoding;
  bool bCustomBufferSize;
  bool bEncTools;
  bool video_fmt_10bit;
  bool bResetAllowed;
  bool bLowpower;

  std::optional<int> nTrellis;
  std::optional<int> nDenoiseMode;
  std::optional<int> nMVCostScalingFactor;
  std::optional<int> nLookAheadDS;
  std::optional<int> nMotionVectorsOverPicBoundaries;
  std::optional<int> nTuneQualityMode;
  std::optional<int> nNumRefFrameLayers;
  std::optional<int> nSAO;

  int nDeviceNum;

  mfxU32 nFourCC;
  mfxU16 nChromaFormat;
};

template <typename T>
static inline T GetTriState(const std::optional<bool> &Value,
                            const T DefaultValue, const T OnValue,
                            const T OffValue) {
  if (!Value.has_value()) {
    return DefaultValue;
  }
  return Value.value() ? OnValue : OffValue;
}

static inline mfxU16 GetCodingOpt(const std::optional<bool> &value) {
  return (mfxU16)GetTriState(value, MFX_CODINGOPTION_UNKNOWN,
                             MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF);
}

static inline std::string GetCodingOptStatus(const mfxU16 &value) {
  if (value == MFX_CODINGOPTION_ON) {
    return "ON";
  } else if (value == MFX_CODINGOPTION_OFF) {
    return "OFF";
  } else {
    return "AUTO";
  }
}