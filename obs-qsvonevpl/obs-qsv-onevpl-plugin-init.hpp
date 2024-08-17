#pragma once

#ifndef __QSV_VPL_PLUGIN_INIT_H__
#define __QSV_VPL_PLUGIN_INIT_H__
#endif

#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif

struct plugin_context {
  obs_encoder_t *EncoderData;

  enum codec_enum Codec;

  struct encoder_params EncoderParams;

  std::unique_ptr<class QSVEncoder> EncoderPTR;

  std::vector<uint8_t> PacketData;

  std::pair<uint8_t*, size_t> ExtraData;
  std::pair<uint8_t*, size_t> SEI;

  os_performance_token_t *PerformanceToken;

  uint32_t roi_increment;
};

static inline const char *TEXT_SPEED = obs_module_text("TargetUsage");
static inline const char *TEXT_TARGET_BITRATE = obs_module_text("Bitrate");
static inline const char *TEXT_CUSTOM_BUFFER_SIZE =
    obs_module_text("CustomBufferSize");
static inline const char *TEXT_BUFFER_SIZE = obs_module_text("BufferSize");
static inline const char *TEXT_MAX_BITRATE = obs_module_text("MaxBitrate");
static inline const char *TEXT_PROFILE = obs_module_text("Profile");
static inline const char *TEXT_HEVC_TIER = obs_module_text("Tier");
static inline const char *TEXT_RATE_CONTROL = obs_module_text("RateControl");
static inline const char *TEXT_ICQ_QUALITY = obs_module_text("ICQQuality");
static inline const char *TEXT_KEYINT_SEC =
    obs_module_text("KeyframeIntervalSec");
static inline const char *TEXT_GOP_REF_DIST = obs_module_text("GOPRefDist");
static inline const char *TEXT_MBBRC = obs_module_text("MBBRC");
static inline const char *TEXT_NUM_REF_FRAME = obs_module_text("NumRefFrame");
static inline const char *TEXT_NUM_REF_ACTIVE_P =
    obs_module_text("NumRefActiveP");
static inline const char *TEXT_NUM_REF_ACTIVE_BL0 =
    obs_module_text("NumRefActiveBL0");
static inline const char *TEXT_NUM_REF_ACTIVE_BL1 =
    obs_module_text("NumRefActiveBL1");
static inline const char *TEXT_LA_DS = obs_module_text("LookaheadDownSampling");
static inline const char *TEXT_GLOBAL_MOTION_BIAS_ADJUSTMENT =
    obs_module_text("GlobalMotionBiasAdjustment");
static inline const char *TEXT_DIRECT_BIAS_ADJUSTMENT =
    obs_module_text("DirectBiasAdjusment");
static inline const char *TEXT_ADAPTIVE_I = obs_module_text("AdaptiveI");
static inline const char *TEXT_ADAPTIVE_B = obs_module_text("AdaptiveB");
static inline const char *TEXT_ADAPTIVE_REF = obs_module_text("AdaptiveRef");
static inline const char *TEXT_ADAPTIVE_CQM = obs_module_text("AdaptiveCQM");
static inline const char *TEXT_P_PYRAMID = obs_module_text("PPyramid");
static inline const char *TEXT_TRELLIS = obs_module_text("Trellis");
static inline const char *TEXT_LA = obs_module_text("Lookahead");
static inline const char *TEXT_LA_DEPTH = obs_module_text("LookaheadDepth");
static inline const char *TEXT_LA_LATENCY =
    obs_module_text("Lookahead latency");
static inline const char *TEXT_MV_OVER_PIC_BOUNDARIES =
    obs_module_text("MotionVectorsOverpicBoundaries");
static inline const char *TEXT_USE_RAW_REF = obs_module_text("UseRawRef");
static inline const char *TEXT_MV_COST_SCALING_FACTOR =
    obs_module_text("MVCostScalingFactor");
static inline const char *TEXT_RDO = obs_module_text("RDO");
static inline const char *TEXT_HRD_CONFORMANCE =
    obs_module_text("HRDConformance");
static inline const char *TEXT_LOW_DELAY_BRC = obs_module_text("LowDelayBRC");
static inline const char *TEXT_LOW_DELAY_HRD = obs_module_text("LowDelayHRD");
static inline const char *TEXT_ASYNC_DEPTH = obs_module_text("AsyncDepth");
static inline const char *TEXT_WINBRC_MAX_AVG_SIZE =
    obs_module_text("WinBRCMaxAvgSize");
static inline const char *TEXT_WINBRC_SIZE = obs_module_text("WinBRCSize");
static inline const char *TEXT_ADAPTIVE_LTR = obs_module_text("AdaptiveLTR");
static inline const char *TEXT_HEVC_SAO =
    obs_module_text("SampleAdaptiveOffset");
static inline const char *TEXT_HEVC_GPB = obs_module_text("GPB");
static inline const char *TEXT_TUNE_QUALITY_MODE =
    obs_module_text("TuneQualityMode");
static inline const char *TEXT_EXT_BRC = obs_module_text("ExtBRC");
static inline const char *TEXT_ENC_TOOLS = obs_module_text("EncTools");
static inline const char *TEXT_LOW_POWER = obs_module_text("LowPower mode");

static inline const char *TEXT_VPP =
    obs_module_text("Video processing filters");
static inline const char *TEXT_VPP_MODE =
    obs_module_text("Video processing mode");
static inline const char *TEXT_DENOISE_STRENGTH =
    obs_module_text("Denoise strength");
static inline const char *TEXT_DENOISE_MODE = obs_module_text("Denoise mode");
static inline const char *TEXT_SCALING_MODE = obs_module_text("Scaling mode");
static inline const char *TEXT_IMAGE_STAB_MODE =
    obs_module_text("ImageStab mode");
static inline const char *TEXT_DETAIL = obs_module_text("Detail");
static inline const char *TEXT_DETAIL_FACTOR = obs_module_text("Detail factor");
static inline const char *TEXT_PERC_ENC_PREFILTER =
    obs_module_text("PercEncPrefilter");

static inline const char *TEXT_INTRA_REF_ENCODING =
    obs_module_text("IntraRefEncoding");
static inline const char *TEXT_INTRA_REF_CYCLE_SIZE =
    obs_module_text("IntraRefCycleSize");
static inline const char *TEXT_INTRA_REF_QP_DELTA =
    obs_module_text("IntraRefQPDelta");

static inline const char *TEXT_GPU_NUMBER = obs_module_text("Select GPU");

static const char *const qsv_ratecontrols_h264[] = {"CBR", "VBR", "CQP", "ICQ",
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

static const char *const qsv_params_condition_vpp[] = {"PRE ENC", "POST ENC",
                                                       "PRE ENC | POST ENC", 0};

static const char *const qsv_params_condition_scaling_mode[] = {
    "OFF",
    "QUALITY | ADVANCED",
    "VEBOX | ADVANCED",
    "LOWPOWER | NEAREST NEIGHBOR",
    "LOWPOWER | ADVANCED",
    "AUTO",
    0};

static const char *const qsv_params_condition_image_stab_mode[] = {
    "OFF", "UPSCALE", "BOXING", "AUTO", 0};

static const char *const qsv_params_condition_extbrc[] = {"ON", "OFF", 0};

static const char *const qsv_params_condition_intra_ref_encoding[] = {
    "VERTICAL", "HORIZONTAL", 0};

static const char *const qsv_params_condition_mv_cost_scaling[] = {
    "DEFAULT", "1/2", "1/4", "1/8", "AUTO", 0};

static const char *const qsv_params_condition_lookahead_mode[] = {"HQ", "LP",
                                                                  "OFF", 0};

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

static void SetDefaultEncoderParams(obs_data_t *, enum codec_enum);

static bool ParamsVisibilityModifier(obs_properties_t *, obs_property_t *,
                             obs_data_t *);

static obs_properties_t *GetParamProps(enum codec_enum Codec);

static void GetEncoderParams(plugin_context *Context, obs_data_t *Settings);

static obs_properties_t *GetH264ParamProps(void *);

static obs_properties_t *GetAV1ParamProps(void *);

static obs_properties_t *GetHEVCParamProps(void *);

plugin_context *InitPluginContext(enum codec_enum Codec, obs_data_t *Settings,
                            obs_encoder_t *EncoderData, bool IsTextureEncoder);

static void *InitTextureEncoder(enum codec_enum Codec, obs_data_t *Settings,
                                obs_encoder_t *EncoderData,
                                const char *FallbackID);

static void *InitH264FrameEncoder(obs_data_t *Settings,
                                  obs_encoder_t *EncoderData);

static void *InitAV1FrameEncoder(obs_data_t *Settings,
                                 obs_encoder_t *EncoderData);

static void *InitHEVCFrameEncoder(obs_data_t *Settings,
                                  obs_encoder_t *EncoderData);

static void *InitH264TextureEncoder(obs_data_t *Settings,
                                    obs_encoder_t *EncoderData);

static void *InitAV1TextureEncoder(obs_data_t *Settings,
                                   obs_encoder_t *EncoderData);

static void *InitHEVCTextureEncoder(obs_data_t *Settings,
                                    obs_encoder_t *EncoderData);

static const char *GetH264EncoderName(void *);

static const char *GetAV1EncoderName(void *);

static const char *GetHEVCEncoderName(void *);

static void SetH264DefaultParams(obs_data_t *Settings);

static void SetAV1DefaultParams(obs_data_t *Settings);

static void SetHEVCDefaultParams(obs_data_t *Settings);
