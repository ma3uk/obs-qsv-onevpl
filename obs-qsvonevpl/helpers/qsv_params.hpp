#pragma once

#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#define __QSV_VPL_ENCODER_PARAMS_H__
#endif

struct encoder_params {
  mfxU16 TargetUsage; /* 1 through 7, 1 being best quality and 7
                                  being the best speed */
  mfxU16 Width;       /* source picture width */
  mfxU16 Height;      /* source picture height */
  mfxU16 AsyncDepth;
  mfxU16 FpsNum;
  mfxU16 FpsDen;
  mfxU16 TargetBitRate;
  mfxU16 MaxBitRate;
  mfxU16 BufferSize;
  mfxU16 CodecProfile;
  mfxU16 CodecProfileTier;
  mfxU16 RateControl;
  mfxU16 QPI;
  mfxU16 QPP;
  mfxU16 QPB;
  mfxU16 LADepth;
  mfxU16 KeyIntSec;
  mfxU16 GOPRefDist;
  mfxU16 ICQQuality;
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
  mfxU16 IntraRefCycleSize;
  mfxU16 CTU;
  mfxU16 WinBRCMaxAvgSize;
  mfxU16 WinBRCSize;
  mfxU16 NumRefFrame;
  mfxU16 DenoiseStrength;

  mfxI16 IntraRefQPDelta;

  std::optional<bool> QualityEnchance;
  std::optional<bool> MBBRC;
  std::optional<bool> AdaptiveI;
  std::optional<bool> AdaptiveB;
  std::optional<bool> AdaptiveRef;
  std::optional<bool> AdaptiveCQM;
  std::optional<bool> AdaptiveLTR;
  std::optional<bool> AdaptiveMaxFrameSize;
  std::optional<bool> RDO;
  std::optional<bool> RawRef;
  std::optional<bool> GPB;
  std::optional<bool> DirectBiasAdjustment;
  std::optional<bool> GopOptFlag;
  std::optional<bool> WeightedPred;
  std::optional<bool> WeightedBiPred;
  std::optional<bool> GlobalMotionBiasAdjustment;
  std::optional<bool> HRDConformance;
  std::optional<bool> LowDelayHRD;

  bool Lookahead;
  bool LookaheadLP;
  bool PPyramid;
  bool ExtBRC;
  bool IntraRefEncoding;
  bool CustomBufferSize;
  bool EncTools;
  bool VideoFormat10bit;
  bool ResetAllowed;
  bool Lowpower;
  bool PercEncPrefilter;
  bool ProcessingEnable;

  std::optional<int> Trellis;
  std::optional<int> VPPDenoiseMode;
  std::optional<int> VPPScalingMode;
  std::optional<int> VPPImageStabMode;
  std::optional<int> VPPDetail;
  std::optional<int> MVCostScalingFactor;
  std::optional<int> LookAheadDS;
  std::optional<bool> MotionVectorsOverPicBoundaries;
  std::optional<int> TuneQualityMode;
  std::optional<int> NumRefFrameLayers;
  std::optional<int> NumRefActiveP;
  std::optional<int> NumRefActiveBL0;
  std::optional<int> NumRefActiveBL1;
  std::optional<int> SAO;

  mfxU32 FourCC;
  mfxU16 ChromaFormat;

  int GPUNum;
};

