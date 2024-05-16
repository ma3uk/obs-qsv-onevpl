
#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif

#ifndef _STDINT_H_INCLUDED
#define _STDINT_H_INCLUDED
#endif

auto TEXT_SPEED = obs_module_text("TargetUsage");
auto TEXT_TARGET_BITRATE = obs_module_text("Bitrate");
auto TEXT_CUSTOM_BUFFER_SIZE = obs_module_text("CustomBufferSize");
auto TEXT_BUFFER_SIZE = obs_module_text("BufferSize");
auto TEXT_MAX_BITRATE = obs_module_text("MaxBitrate");
auto TEXT_PROFILE = obs_module_text("Profile");
auto TEXT_HEVC_TIER = obs_module_text("Tier");
auto TEXT_RATE_CONTROL = obs_module_text("RateControl");
auto TEXT_ICQ_QUALITY = obs_module_text("ICQQuality");
auto TEXT_KEYINT_SEC = obs_module_text("KeyframeIntervalSec");
auto TEXT_GOP_REF_DIST = obs_module_text("GOPRefDist");
auto TEXT_MBBRC = obs_module_text("MBBRC");
auto TEXT_NUM_REF_FRAME = obs_module_text("NumRefFrame");
auto TEXT_NUM_REF_ACTIVE_P = obs_module_text("NumRefActiveP");
auto TEXT_NUM_REF_ACTIVE_BL0 = obs_module_text("NumRefActiveBL0");
auto TEXT_NUM_REF_ACTIVE_BL1 = obs_module_text("NumRefActiveBL1");
auto TEXT_LA_DS = obs_module_text("LookaheadDownSampling");
auto TEXT_GLOBAL_MOTION_BIAS_ADJUSTMENT =
    obs_module_text("GlobalMotionBiasAdjustment");
auto TEXT_DIRECT_BIAS_ADJUSTMENT = obs_module_text("DirectBiasAdjusment");
auto TEXT_ADAPTIVE_I = obs_module_text("AdaptiveI");
auto TEXT_ADAPTIVE_B = obs_module_text("AdaptiveB");
auto TEXT_ADAPTIVE_REF = obs_module_text("AdaptiveRef");
auto TEXT_ADAPTIVE_CQM = obs_module_text("AdaptiveCQM");
auto TEXT_P_PYRAMID = obs_module_text("PPyramid");
auto TEXT_TRELLIS = obs_module_text("Trellis");
auto TEXT_LA = obs_module_text("Lookahead");
auto TEXT_LA_DEPTH = obs_module_text("LookaheadDepth");
auto TEXT_LA_LATENCY = obs_module_text("Lookahead latency");
auto TEXT_MV_OVER_PIC_BOUNDARIES =
    obs_module_text("MotionVectorsOverpicBoundaries");
auto TEXT_USE_RAW_REF = obs_module_text("UseRawRef");
auto TEXT_MV_COST_SCALING_FACTOR = obs_module_text("MVCostScalingFactor");
auto TEXT_RDO = obs_module_text("RDO");
auto TEXT_HRD_CONFORMANCE = obs_module_text("HRDConformance");
auto TEXT_LOW_DELAY_BRC = obs_module_text("LowDelayBRC");
auto TEXT_LOW_DELAY_HRD = obs_module_text("LowDelayHRD");
auto TEXT_ASYNC_DEPTH = obs_module_text("AsyncDepth");
auto TEXT_WINBRC_MAX_AVG_SIZE = obs_module_text("WinBRCMaxAvgSize");
auto TEXT_WINBRC_SIZE = obs_module_text("WinBRCSize");
auto TEXT_ADAPTIVE_LTR = obs_module_text("AdaptiveLTR");
auto TEXT_HEVC_SAO = obs_module_text("SampleAdaptiveOffset");
auto TEXT_HEVC_GPB = obs_module_text("GPB");
auto TEXT_TUNE_QUALITY_MODE = obs_module_text("TuneQualityMode");
auto TEXT_EXT_BRC = obs_module_text("ExtBRC");
auto TEXT_ENC_TOOLS = obs_module_text("EncTools");
auto TEXT_LOW_POWER = obs_module_text("LowPower mode");

auto TEXT_VPP = obs_module_text("Video processing filters");
auto TEXT_VPP_MODE = obs_module_text("Video processing mode");
auto TEXT_DENOISE_STRENGTH = obs_module_text("Denoise strength");
auto TEXT_DENOISE_MODE = obs_module_text("Denoise mode");
auto TEXT_SCALING_MODE = obs_module_text("Scaling mode");
auto TEXT_IMAGE_STAB_MODE = obs_module_text("ImageStab mode");
auto TEXT_DETAIL = obs_module_text("Detail");
auto TEXT_DETAIL_FACTOR = obs_module_text("Detail factor");
auto TEXT_PERC_ENC_PREFILTER = obs_module_text("PercEncPrefilter");

auto TEXT_INTRA_REF_ENCODING = obs_module_text("IntraRefEncoding");
auto TEXT_INTRA_REF_CYCLE_SIZE = obs_module_text("IntraRefCycleSize");
auto TEXT_INTRA_REF_QP_DELTA = obs_module_text("IntraRefQPDelta");

auto TEXT_GPU_NUMBER = obs_module_text("Select GPU");

static void obs_qsv_defaults(obs_data_t *settings, int ver,
                             enum qsv_codec codec) {
  obs_data_set_default_string(settings, "target_usage", "TU4 (Balanced)");
  obs_data_set_default_int(settings, "bitrate", 6000);
  obs_data_set_default_int(settings, "max_bitrate", 6000);
  obs_data_set_default_bool(settings, "custom_buffer_size", false);
  obs_data_set_default_int(settings, "buffer_size", 0);
  obs_data_set_default_string(settings, "profile",
                              codec == QSV_CODEC_AVC ? "high" : "main");
  obs_data_set_default_string(settings, "hevc_tier", "main");
  obs_data_set_default_string(settings, "rate_control", "CBR");

  obs_data_set_default_int(settings, "__ver", ver);

  obs_data_set_default_int(settings, "cqp", 23);
  obs_data_set_default_int(settings, "qpi", 23);
  obs_data_set_default_int(settings, "qpp", 23);
  obs_data_set_default_int(settings, "qpb", 23);
  obs_data_set_default_int(settings, "icq_quality", 23);

  obs_data_set_default_int(settings, "keyint_sec", 4);
  obs_data_set_default_int(settings, "gop_ref_dist", 4);
  obs_data_set_default_int(settings, "async_depth", 4);

  obs_data_set_default_string(settings, "intra_ref_encoding", "OFF");
  obs_data_set_default_string(settings, "low_delay_brc", "OFF");
  obs_data_set_default_string(settings, "low_delay_hrd", "OFF");

  obs_data_set_default_string(settings, "tune_quality", "OFF");
  obs_data_set_default_string(settings, "adaptive_i", "AUTO");
  obs_data_set_default_string(settings, "adaptive_b", "AUTO");
  obs_data_set_default_string(settings, "adaptive_ref", "AUTO");
  obs_data_set_default_string(settings, "adaptive_cqm", "ON");
  obs_data_set_default_string(settings, "adaptive_ltr", "OFF");
  obs_data_set_default_string(settings, "use_raw_ref", "AUTO");
  obs_data_set_default_string(settings, "rdo", "AUTO");
  obs_data_set_default_string(settings, "hrd_conformance", "AUTO");
  obs_data_set_default_string(settings, "mbbrc", "AUTO");
  obs_data_set_default_string(settings, "trellis", "AUTO");
  obs_data_set_default_int(settings, "num_ref_frame", 0);
  obs_data_set_default_string(settings, "globalmotionbias_adjustment", "AUTO");
  obs_data_set_default_string(settings, "mv_costscaling_factor", "AUTO");
  obs_data_set_default_string(settings, "directbias_adjustment", "AUTO");
  obs_data_set_default_string(settings, "mv_overpic_boundaries", "AUTO");
  obs_data_set_default_int(settings, "la_depth", 0);

  obs_data_set_default_string(settings, "lookahead", "OFF");
  obs_data_set_default_string(settings, "lookahead_latency", "NORMAL");
  obs_data_set_default_string(settings, "lookahead_ds", "MEDIUM");
  obs_data_set_default_string(settings, "extbrc", "OFF");
  obs_data_set_default_string(settings, "enctools", "OFF");
  obs_data_set_default_string(settings, "hevc_sao", "AUTO");
  obs_data_set_default_string(settings, "hevc_gpb", "AUTO");

  obs_data_set_default_string(settings, "intra_ref_encoding", "OFF");
  obs_data_set_default_string(settings, "intra_ref_type", "VERTICAL");
  obs_data_set_default_int(settings, "intra_ref_cycle_size", 2);
  obs_data_set_default_int(settings, "intra_ref_qp_delta", 0);

  obs_data_set_default_string(settings, "vpp", "OFF");
  obs_data_set_default_string(settings, "vpp_mode", "PRE ENC");
  obs_data_set_default_string(settings, "denoise_mode", "OFF");
  obs_data_set_default_int(settings, "denoise_strength", 50);
  obs_data_set_default_string(settings, "detail", "OFF");
  obs_data_set_default_int(settings, "detail_factor", 50);
  obs_data_set_default_string(settings, "image_stab_mode", "OFF");
  obs_data_set_default_string(settings, "scaling_mode", "OFF");
  obs_data_set_default_string(settings, "perc_enc_prefilter", "OFF");

  obs_data_set_default_int(settings, "gpu_number", 0);
}

static inline void add_strings(obs_property_t *list,
                               const char *const *strings) {
  while (*strings) {
    obs_property_list_add_string(list, *strings, *strings);
    strings++;
  }
}

static bool rate_control_modified(obs_properties_t *ppts, obs_property_t *p,
                                  obs_data_t *settings) {
  const char *rate_control = obs_data_get_string(settings, "rate_control");
  bool bVisible = astrcmpi(rate_control, "VBR") == 0;
  p = obs_properties_get(ppts, "max_bitrate");
  obs_property_set_visible(p, bVisible);

  bVisible =
      astrcmpi(rate_control, "CQP") == 0 || astrcmpi(rate_control, "ICQ") == 0;
  p = obs_properties_get(ppts, "bitrate");
  obs_property_set_visible(p, !bVisible);

  bVisible = astrcmpi(rate_control, "CQP") == 0;
  p = obs_properties_get(ppts, "qpi");
  if (p)
    obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "qpb");
  if (p)
    obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "qpp");
  if (p)
    obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "cqp");
  if (p)
    obs_property_set_visible(p, bVisible);

  bVisible = astrcmpi(rate_control, "ICQ") == 0;
  p = obs_properties_get(ppts, "icq_quality");
  obs_property_set_visible(p, bVisible);

  bVisible =
      astrcmpi(rate_control, "CBR") == 0 || astrcmpi(rate_control, "VBR") == 0;
  p = obs_properties_get(ppts, "enctools");
  obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "extbrc");
  obs_property_set_visible(p, bVisible);

  const char *lookahead = obs_data_get_string(settings, "lookahead");

  bool bLAVisible = (astrcmpi(rate_control, "CBR") == 0 ||
                     astrcmpi(rate_control, "VBR") == 0);
  p = obs_properties_get(ppts, "lookahead");
  obs_property_set_visible(p, bLAVisible);
  if (bLAVisible) {

    bool bLAOptVisibleHQ = astrcmpi(lookahead, "HQ") == 0;
    bool bLAOptVisibleLP = astrcmpi(lookahead, "LP") == 0;
    p = obs_properties_get(ppts, "lookahead_ds");
    obs_property_set_visible(
        p, ((bLAOptVisibleHQ || bLAOptVisibleLP) && bLAVisible));

    if ((bLAOptVisibleHQ /* || bLAOptVisibleLP*/)) {
      obs_data_set_string(settings, "extbrc", "OFF");
    }

    p = obs_properties_get(ppts, "lookahead_latency");
    obs_property_set_visible(p, (bLAOptVisibleHQ && bLAVisible));

    // if ((bLAOptVisibleHQ && bLAVisible)) {
    //   obs_data_set_string(settings, "hrd_conformance", "OFF");
    // }

    if (bLAOptVisibleLP) {
      obs_data_set_string(settings, "enctools", "OFF");
    }
  }
  bVisible =
      astrcmpi(rate_control, "VBR") == 0 || astrcmpi(rate_control, "CBR") == 0;
  p = obs_properties_get(ppts, "winbrc_max_avg_size");
  obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "winbrc_size");
  obs_property_set_visible(p, bVisible);

  if (!bVisible) {
    obs_data_erase(settings, "winbrc_max_avg_size");
    obs_data_erase(settings, "winbrc_size");
  }

  bVisible = astrcmpi(rate_control, "CBR") == 0 ||
             astrcmpi(rate_control, "VBR") == 0 ||
             astrcmpi(rate_control, "ICQ") == 0;
  p = obs_properties_get(ppts, "mbbrc");
  obs_property_set_visible(p, bVisible);
  if (!bVisible) {
    obs_data_set_string(settings, "mbbrc", "OFF");
  }

  bool use_bufsize = obs_data_get_bool(settings, "custom_buffer_size");
  p = obs_properties_get(ppts, "buffer_size");
  obs_property_set_visible(p, use_bufsize);

  const char *hrd_conformance =
      obs_data_get_string(settings, "hrd_conformance");
  bVisible = astrcmpi(hrd_conformance, "ON") == 0 ||
             astrcmpi(hrd_conformance, "AUTO") == 0;
  p = obs_properties_get(ppts, "low_delay_hrd");
  obs_property_set_visible(p, bVisible);

  return true;
}

static bool visible_modified(obs_properties_t *ppts, obs_property_t *p,
                             obs_data_t *settings) {

  const char *global_motion_bias_adjustment_enable =
      obs_data_get_string(settings, "globalmotionbias_adjustment");
  bool bVisible = ((astrcmpi(global_motion_bias_adjustment_enable, "ON") == 0));
  p = obs_properties_get(ppts, "mv_costscaling_factor");
  obs_property_set_visible(p, bVisible);
  if (!bVisible) {
    obs_data_erase(settings, "mv_costscaling_factor");
  }

  const char *vpp = obs_data_get_string(settings, "vpp");
  bool bVisibleVPP = astrcmpi(vpp, "ON") == 0;
  p = obs_properties_get(ppts, "vpp_mode");
  obs_property_set_visible(p, bVisibleVPP);
  p = obs_properties_get(ppts, "detail");
  obs_property_set_visible(p, bVisibleVPP);
  p = obs_properties_get(ppts, "image_stab_mode");
  obs_property_set_visible(p, bVisibleVPP);
  p = obs_properties_get(ppts, "perc_enc_prefilter");
  obs_property_set_visible(p, bVisibleVPP);
  p = obs_properties_get(ppts, "denoise_mode");
  obs_property_set_visible(p, bVisibleVPP);
  p = obs_properties_get(ppts, "scaling_mode");
  obs_property_set_visible(p, bVisibleVPP);

  const char *denoise_mode = obs_data_get_string(settings, "denoise_mode");
  bVisible = astrcmpi(denoise_mode, "MANUAL | PRE ENCODE") == 0 ||
             astrcmpi(denoise_mode, "MANUAL | POST ENCODE") == 0;
  p = obs_properties_get(ppts, "denoise_strength");
  obs_property_set_visible(p, bVisible && bVisibleVPP);

  const char *detail = obs_data_get_string(settings, "detail");
  bVisible = astrcmpi(detail, "ON") == 0;
  p = obs_properties_get(ppts, "detail_factor");
  obs_property_set_visible(p, bVisible && bVisibleVPP);

  const char *intra_ref_encoding =
      obs_data_get_string(settings, "intra_ref_encoding");
  bVisible = astrcmpi(intra_ref_encoding, "ON") == 0;
  p = obs_properties_get(ppts, "intra_ref_type");
  obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "intra_ref_cycle_size");
  obs_property_set_visible(p, bVisible);
  p = obs_properties_get(ppts, "intra_ref_qp_delta");
  obs_property_set_visible(p, bVisible);

  return true;
}

static obs_properties_t *obs_qsv_props(enum qsv_codec codec) {

  obs_properties_t *props = obs_properties_create();
  obs_property_t *prop;

  prop = obs_properties_add_list(props, "rate_control", TEXT_RATE_CONTROL,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

  if (codec == QSV_CODEC_AVC) {
    add_strings(prop, qsv_ratecontrols_h264);
  } else if (codec == QSV_CODEC_HEVC) {
    add_strings(prop, qsv_ratecontrols_hevc);
  } else if (codec == QSV_CODEC_AV1) {
    add_strings(prop, qsv_ratecontrols_av1);
  } else if (codec == QSV_CODEC_VP9) {
    add_strings(prop, qsv_ratecontrols_vp9);
  }

  obs_property_set_modified_callback(prop, rate_control_modified);

  prop = obs_properties_add_list(props, "target_usage", TEXT_SPEED,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_usage_names);

  if (codec != QSV_CODEC_VP9) {
    prop =
        obs_properties_add_list(props, "profile", TEXT_PROFILE,
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    if (codec == QSV_CODEC_AVC) {
      add_strings(prop, qsv_profile_names_h264);
    } else if (codec == QSV_CODEC_AV1) {
      add_strings(prop, qsv_profile_names_av1);
    } else if (codec == QSV_CODEC_HEVC) {
      add_strings(prop, qsv_profile_names_hevc);
    }

    if (codec == QSV_CODEC_HEVC) {
      prop =
          obs_properties_add_list(props, "hevc_tier", TEXT_HEVC_TIER,
                                  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
      add_strings(prop, qsv_profile_tiers_hevc);
    }
  }

  prop = obs_properties_add_list(props, "extbrc", TEXT_EXT_BRC,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_set_long_description(prop, obs_module_text("ExtBRC.ToolTip"));
  add_strings(prop, qsv_params_condition_extbrc);
  obs_property_set_modified_callback(prop, rate_control_modified);

  prop = obs_properties_add_list(props, "enctools", TEXT_ENC_TOOLS,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_set_long_description(prop, obs_module_text("EncTools.ToolTip"));
  add_strings(prop, qsv_params_condition);
  obs_property_set_modified_callback(prop, rate_control_modified);

  if (codec == QSV_CODEC_AV1) {
    prop =
        obs_properties_add_list(props, "tune_quality", TEXT_TUNE_QUALITY_MODE,
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    add_strings(prop, qsv_params_condition_tune_quality);
    obs_property_set_long_description(
        prop, obs_module_text("TuneQualityMode.ToolTip"));
  }

  prop = obs_properties_add_int(props, "bitrate", TEXT_TARGET_BITRATE, 50,
                                10000000, 50);
  obs_property_int_set_suffix(prop, " Kbps");

  prop = obs_properties_add_bool(props, "custom_buffer_size",
                                 TEXT_CUSTOM_BUFFER_SIZE);
  obs_property_set_long_description(
      prop, obs_module_text("CustomBufferSize.ToolTip"));
  obs_property_set_modified_callback(prop, rate_control_modified);
  prop = obs_properties_add_int(props, "buffer_size", TEXT_BUFFER_SIZE, 0,
                                10000000, 10);
  obs_property_int_set_suffix(prop, " KB");
  obs_property_set_long_description(prop,
                                    obs_module_text("BufferSize.ToolTip"));

  prop = obs_properties_add_int(props, "max_bitrate", TEXT_MAX_BITRATE, 50,
                                10000000, 50);
  obs_property_int_set_suffix(prop, " Kbps");

  prop = obs_properties_add_int(props, "winbrc_max_avg_size",
                                TEXT_WINBRC_MAX_AVG_SIZE, 0, 10000000, 50);
  obs_property_set_long_description(
      prop, obs_module_text("WinBrcMaxAvgSize.ToolTip"));
  obs_property_int_set_suffix(prop, " Kbps");

  prop = obs_properties_add_int(props, "winbrc_size", TEXT_WINBRC_SIZE, 0,
                                10000, 1);
  obs_property_set_long_description(prop,
                                    obs_module_text("WinBrcSize.ToolTip"));

  obs_properties_add_int(props, "cqp", "CQP", 1,
                         codec == QSV_CODEC_AV1 ? 63 : 51, 1);

  obs_properties_add_int(props, "icq_quality", TEXT_ICQ_QUALITY, 1, 51, 1);

  prop = obs_properties_add_int(props, "keyint_sec", TEXT_KEYINT_SEC, 0, 20, 1);
  obs_property_int_set_suffix(prop, " s");

  obs_properties_add_int_slider(props, "num_ref_frame", TEXT_NUM_REF_FRAME, 0,
                                (codec == QSV_CODEC_AV1 ? 7 : 16), 1);

  if (codec == QSV_CODEC_HEVC) {
    prop =
        obs_properties_add_list(props, "hevc_gpb", TEXT_HEVC_GPB,
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    add_strings(prop, qsv_params_condition_tristate);
    obs_property_set_long_description(prop, obs_module_text("GPB.ToolTip"));
  }

  prop =
      obs_properties_add_int_slider(props, "gop_ref_dist", TEXT_GOP_REF_DIST, 1,
                                    (codec == QSV_CODEC_AV1) ? 32 : 16, 1);
  obs_property_set_long_description(prop,
                                    obs_module_text("GOPRefDist.Tooltip"));
  obs_property_long_description(prop);

  obs_properties_add_int(props, "async_depth", TEXT_ASYNC_DEPTH, 1, 1000, 1);

  prop = obs_properties_add_list(props, "hrd_conformance", TEXT_HRD_CONFORMANCE,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop,
                                    obs_module_text("HRDConformance.ToolTip"));
  obs_property_set_modified_callback(prop, rate_control_modified);

  prop = obs_properties_add_list(props, "low_delay_hrd", TEXT_LOW_DELAY_HRD,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop,
                                    obs_module_text("LowDelayHRD.ToolTip"));
  obs_property_set_modified_callback(prop, rate_control_modified);

  if (codec != QSV_CODEC_VP9) {
    prop =
        obs_properties_add_list(props, "mbbrc", TEXT_MBBRC, OBS_COMBO_TYPE_LIST,
                                OBS_COMBO_FORMAT_STRING);
    add_strings(prop, qsv_params_condition_tristate);
    obs_property_set_modified_callback(prop, rate_control_modified);
    obs_property_set_long_description(prop, obs_module_text("MBBRC.ToolTip"));
  }

  prop = obs_properties_add_list(props, "rdo", TEXT_RDO, OBS_COMBO_TYPE_LIST,
                                 OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop, obs_module_text("RDO.ToolTip"));

  prop = obs_properties_add_list(props, "adaptive_i", TEXT_ADAPTIVE_I,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop, obs_module_text("AdaptiveI.ToolTip"));

  prop = obs_properties_add_list(props, "adaptive_b", TEXT_ADAPTIVE_B,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop, obs_module_text("AdaptiveB.ToolTip"));

  prop = obs_properties_add_list(props, "adaptive_ref", TEXT_ADAPTIVE_REF,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop,
                                    obs_module_text("AdaptiveRef.ToolTip"));

  prop = obs_properties_add_list(props, "adaptive_ltr", TEXT_ADAPTIVE_LTR,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop,
                                    obs_module_text("AdaptiveLTR.ToolTip"));

  prop = obs_properties_add_list(props, "adaptive_cqm", TEXT_ADAPTIVE_CQM,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop,
                                    obs_module_text("AdaptiveCQM.ToolTip"));

  prop = obs_properties_add_list(props, "p_pyramid", TEXT_P_PYRAMID,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_p_pyramid);
  obs_property_set_long_description(prop, obs_module_text("PPyramid.ToolTip"));

  prop = obs_properties_add_list(props, "use_raw_ref", TEXT_USE_RAW_REF,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(prop, obs_module_text("UseRawRef.ToolTip"));

  prop = obs_properties_add_list(props, "globalmotionbias_adjustment",
                                 TEXT_GLOBAL_MOTION_BIAS_ADJUSTMENT,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_modified_callback(prop, visible_modified);
  obs_property_set_long_description(
      prop, obs_module_text("GlobalMotionBiasAdjustment.ToolTip"));

  prop = obs_properties_add_list(props, "mv_costscaling_factor",
                                 TEXT_MV_COST_SCALING_FACTOR,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_mv_cost_scaling);
  obs_property_set_long_description(
      prop, obs_module_text("MVCostScalingFactor.ToolTip"));

  prop = obs_properties_add_list(props, "directbias_adjustment",
                                 TEXT_DIRECT_BIAS_ADJUSTMENT,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(
      prop, obs_module_text("DirectBiasAdjusment.ToolTip"));

  prop = obs_properties_add_list(props, "mv_overpic_boundaries",
                                 TEXT_MV_OVER_PIC_BOUNDARIES,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_long_description(
      prop, obs_module_text("MVOverpicBoundaries.ToolTip"));

  prop = obs_properties_add_list(props, "trellis", TEXT_TRELLIS,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_trellis);
  obs_property_set_long_description(prop, obs_module_text("Trellis.ToolTip"));

  prop = obs_properties_add_list(props, "lookahead", TEXT_LA,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_lookahead_mode);
  obs_property_set_modified_callback(prop, rate_control_modified);
  obs_property_set_long_description(prop,
                                    obs_module_text("LookaheadDS.ToolTip"));

  prop = obs_properties_add_list(props, "lookahead_ds", TEXT_LA_DS,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_lookahead_ds);
  obs_property_set_long_description(prop,
                                    obs_module_text("LookaheadDS.ToolTip"));

  prop = obs_properties_add_list(props, "lookahead_latency", TEXT_LA_LATENCY,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_lookahead_latency);
  obs_property_set_long_description(
      prop, obs_module_text("LookaheadLatency.ToolTip"));

  prop = obs_properties_add_list(props, "vpp", TEXT_VPP, OBS_COMBO_TYPE_LIST,
                                 OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition);
  obs_property_set_long_description(prop, obs_module_text("VPP.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  prop = obs_properties_add_list(props, "denoise_mode", TEXT_DENOISE_MODE,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_denoise_mode);
  obs_property_set_long_description(prop,
                                    obs_module_text("DenoiseMode.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  obs_properties_add_int_slider(props, "denoise_strength",
                                TEXT_DENOISE_STRENGTH, 1, 100, 1);

  prop = obs_properties_add_list(props, "scaling_mode", TEXT_SCALING_MODE,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_scaling_mode);
  obs_property_set_long_description(prop,
                                    obs_module_text("ScalingMode.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  prop = obs_properties_add_list(props, "detail", TEXT_DETAIL,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition);
  obs_property_set_long_description(prop, obs_module_text("Detail.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  obs_properties_add_int_slider(props, "detail_factor", TEXT_DETAIL_FACTOR, 1,
                                100, 1);

  prop = obs_properties_add_list(props, "image_stab_mode", TEXT_IMAGE_STAB_MODE,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_image_stab_mode);
  obs_property_set_long_description(prop, obs_module_text("ImageStab.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  prop = obs_properties_add_list(props, "perc_enc_prefilter",
                                 TEXT_PERC_ENC_PREFILTER, OBS_COMBO_TYPE_LIST,
                                 OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition);
  obs_property_set_long_description(
      prop, obs_module_text("PercPreEncFilter.ToolTip"));
  obs_property_set_modified_callback(prop, visible_modified);

  prop = obs_properties_add_list(props, "low_power", TEXT_LOW_POWER,
                                 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  add_strings(prop, qsv_params_condition_tristate);
  obs_property_set_modified_callback(prop, rate_control_modified);
  obs_property_set_long_description(
      prop, obs_module_text(
                "Hint to enable low power consumption mode for encoders. \n "
                "Intel Arc graphics adapters only work with the LowPower \n"
                "parameter enabled, changing the state of the parameter will \n"
                "not affect operation. The integrated Intel UHD graphics can \n"
                "work in both modes, the available functionality may vary \n"
                "depending on the state of the parameter."));

  if (codec != QSV_CODEC_AV1) {
    prop = obs_properties_add_list(props, "intra_ref_encoding",
                                   TEXT_INTRA_REF_ENCODING, OBS_COMBO_TYPE_LIST,
                                   OBS_COMBO_FORMAT_STRING);
    add_strings(prop, qsv_params_condition);
    obs_property_set_modified_callback(prop, visible_modified);
    obs_property_set_long_description(
        prop, obs_module_text("IntraRefEncoding.ToolTip"));

    obs_properties_add_int(props, "intra_ref_cycle_size",
                           TEXT_INTRA_REF_CYCLE_SIZE, 2, 1000, 1);
    obs_property_set_long_description(
        prop, obs_module_text("IntraRefCycleSize.ToolTip"));

    obs_properties_add_int(props, "intra_ref_qp_delta", TEXT_INTRA_REF_QP_DELTA,
                           -51, 51, 1);
    obs_property_set_long_description(
        prop, obs_module_text("IntraRefQPDelta.ToolTip"));
  }

  if (codec == QSV_CODEC_HEVC) {
    prop =
        obs_properties_add_list(props, "hevc_sao", TEXT_HEVC_SAO,
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    add_strings(prop, qsv_params_condition_hevc_sao);
    obs_property_set_long_description(prop, obs_module_text("SAO.ToolTip"));
  }

  prop = obs_properties_add_int_slider(props, "num_ref_active_p",
                                TEXT_NUM_REF_ACTIVE_P, 0,
                                (codec == QSV_CODEC_AV1 ? 7 : 16), 1);
  obs_property_set_long_description(
      prop,
      obs_module_text(
          "Max number of active references for P-frames. \nThe set values "
          "can cause frame drops. Do not touch the values \n"
          "of this parameter if you are not sure what you are doing."));

  prop = obs_properties_add_int_slider(props, "num_ref_active_bl0",
                                TEXT_NUM_REF_ACTIVE_BL0, 0,
                                (codec == QSV_CODEC_AV1 ? 7 : 16), 1);
  obs_property_set_long_description(
      prop,
      obs_module_text(
          "Max number of active references for B-frames in reference \n"
          "picture list 0. The set values can cause frame drops. Do not \n"
          "touch the values \n"
          "of this parameter if you are not sure what you are doing."));

  prop = obs_properties_add_int_slider(props, "num_ref_active_bl1",
                                TEXT_NUM_REF_ACTIVE_BL1, 0,
                                (codec == QSV_CODEC_AV1 ? 7 : 16), 1);
  obs_property_set_long_description(
      prop,
      obs_module_text(
          "Max number of active references for B-frames in reference "
          "picture list 1. \n The set values can cause frame drops. Do not \n"
          "touch the values \n"
          "of this parameter if you are not sure what you are doing."));
  obs_property_long_description(prop);
  prop = obs_properties_add_int(props, "gpu_number", TEXT_GPU_NUMBER, 0, 4, 1);
  obs_property_set_long_description(
      prop, obs_module_text(
                "Choosing a graphics adapter for multi-GPU systems. \n The "
                "number of the adapter you need may vary depending on \n"
                "the priority set by the system"));
  obs_property_long_description(prop);

  return props;
}

static void update_params(obs_qsv *obsqsv, obs_data_t *settings) {
  video_t *video = obs_encoder_video(obsqsv->encoder);
  const video_output_info *voi = video_output_get_info(video);
  const char *target_usage = obs_data_get_string(settings, "target_usage");
  const char *profile = obs_data_get_string(settings, "profile");
  const char *hevc_tier = obs_data_get_string(settings, "hevc_tier");
  const char *rate_control = obs_data_get_string(settings, "rate_control");
  int target_bitrate = static_cast<int>(obs_data_get_int(settings, "bitrate"));
  bool custom_buffer_size = obs_data_get_bool(settings, "custom_buffer_size");
  int buffer_size = static_cast<int>(obs_data_get_int(settings, "buffer_size"));
  int max_bitrate = static_cast<int>(obs_data_get_int(settings, "max_bitrate"));
  int cqp = static_cast<int>(obs_data_get_int(settings, "cqp"));
  // int ver = (int)obs_data_get_int(settings, "__ver");
  int icq_quality = static_cast<int>(obs_data_get_int(settings, "icq_quality"));
  int keyint_sec = static_cast<int>(obs_data_get_int(settings, "keyint_sec"));
  bool cbr_override = obs_data_get_bool(settings, "cbr");
  int gop_ref_dist =
      static_cast<int>(obs_data_get_int(settings, "gop_ref_dist"));

  const char *hrd_conformance =
      obs_data_get_string(settings, "hrd_conformance");

  const char *low_delay_hrd = obs_data_get_string(settings, "low_delay_hrd");

  const char *mbbrc = obs_data_get_string(settings, "mbbrc");
  const char *codec = "";
  const char *adaptive_i = obs_data_get_string(settings, "adaptive_i");
  const char *adaptive_b = obs_data_get_string(settings, "adaptive_b");
  const char *adaptive_ref = obs_data_get_string(settings, "adaptive_ref");
  const char *adaptive_cqm = obs_data_get_string(settings, "adaptive_cqm");
  const char *adaptive_ltr = obs_data_get_string(settings, "adaptive_ltr");
  const char *low_power = obs_data_get_string(settings, "low_power");
  const char *use_raw_ref = obs_data_get_string(settings, "use_raw_ref");
  int winbrc_max_avg_size =
      static_cast<int>(obs_data_get_int(settings, "winbrc_max_avg_size"));
  int winbrc_size = static_cast<int>(obs_data_get_int(settings, "winbrc_size"));
  const char *rdo = obs_data_get_string(settings, "rdo");
  const char *trellis = obs_data_get_string(settings, "trellis");
  int num_ref_frame =
      static_cast<int>(obs_data_get_int(settings, "num_ref_frame"));
  int num_ref_active_p =
      static_cast<int>(obs_data_get_int(settings, "num_ref_active_p"));
  int num_ref_active_bl0 =
      static_cast<int>(obs_data_get_int(settings, "num_ref_active_bl0"));
  int num_ref_active_bl1 =
      static_cast<int>(obs_data_get_int(settings, "num_ref_active_bl1"));
  const char *globalmotionbias_adjustment =
      obs_data_get_string(settings, "globalmotionbias_adjustment");
  const char *mv_costscaling_factor =
      obs_data_get_string(settings, "mv_costscaling_factor");
  const char *lookahead = obs_data_get_string(settings, "lookahead");
  const char *lookahead_ds = obs_data_get_string(settings, "lookahead_ds");
  const char *lookahead_latency =
      obs_data_get_string(settings, "lookahead_latency");
  const char *directbias_adjustment =
      obs_data_get_string(settings, "directbias_adjustment");
  const char *mv_overpic_boundaries =
      obs_data_get_string(settings, "mv_overpic_boundaries");
  const char *hevc_sao = obs_data_get_string(settings, "hevc_sao");
  const char *hevc_gpb = obs_data_get_string(settings, "hevc_gpb");
  const char *tune_quality = obs_data_get_string(settings, "tune_quality");
  const char *p_pyramid = obs_data_get_string(settings, "p_pyramid");
  const char *extbrc = obs_data_get_string(settings, "extbrc");
  const char *enctools = obs_data_get_string(settings, "enctools");
  const char *intra_ref_encoding =
      obs_data_get_string(settings, "intra_ref_encoding");
  int intra_ref_cycle_size =
      static_cast<int>(obs_data_get_int(settings, "intra_ref_cycle_size"));
  int intra_ref_qp_delta =
      static_cast<int>(obs_data_get_int(settings, "intra_ref_qp_delta"));

  int width = static_cast<int>(obs_encoder_get_width(obsqsv->encoder));
  int height = static_cast<int>(obs_encoder_get_height(obsqsv->encoder));

  const char *vpp = obs_data_get_string(settings, "vpp");
  int denoise_strength =
      static_cast<int>(obs_data_get_int(settings, "denoise_strength"));
  const char *denoise_mode = obs_data_get_string(settings, "denoise_mode");
  const char *detail = obs_data_get_string(settings, "detail");
  int detail_factor =
      static_cast<int>(obs_data_get_int(settings, "detail_factor"));
  const char *scaling_mode = obs_data_get_string(settings, "scaling_mode");
  const char *image_stab_mode =
      obs_data_get_string(settings, "image_stab_mode");
  const char *perc_enc_prefilter =
      obs_data_get_string(settings, "perc_enc_prefilter");

  int gpu_number = static_cast<int>(obs_data_get_int(settings, "gpu_number"));

  obsqsv->params.nGPUNum = gpu_number;

  if (astrcmpi(target_usage, "TU1 (Veryslow)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_1;
    info("\tTarget usage set: TU1 (Veryslow)");
  } else if (astrcmpi(target_usage, "TU2 (Slower)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_2;
    info("\tTarget usage set: TU2 (Slower)");
  } else if (astrcmpi(target_usage, "TU3 (Slow)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_3;
    info("\tTarget usage set: TU3 (Slow)");
  } else if (astrcmpi(target_usage, "TU4 (Balanced)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_4;
    info("\tTarget usage set: TU4 (Balanced)");
  } else if (astrcmpi(target_usage, "TU5 (Fast)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_5;
    info("\tTarget usage set: TU5 (Fast)");
  } else if (astrcmpi(target_usage, "TU6 (Faster)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_6;
    info("\tTarget usage set: TU6 (Faster)");
  } else if (astrcmpi(target_usage, "TU7 (Veryfast)") == 0) {
    obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_7;
    info("\tTarget usage set: TU7 (Veryfast)");
  }

  if (astrcmpi(tune_quality, "PSNR") == 0) {
    obsqsv->params.nTuneQualityMode = 1;
  } else if (astrcmpi(tune_quality, "SSIM") == 0) {
    obsqsv->params.nTuneQualityMode = 2;
  } else if (astrcmpi(tune_quality, "MS SSIM") == 0) {
    obsqsv->params.nTuneQualityMode = 3;
  } else if (astrcmpi(tune_quality, "VMAF") == 0) {
    obsqsv->params.nTuneQualityMode = 4;
  } else if (astrcmpi(tune_quality, "PERCEPTUAL") == 0) {
    obsqsv->params.nTuneQualityMode = 5;
  } else if (astrcmpi(tune_quality, "DEFAULT") == 0) {
    obsqsv->params.nTuneQualityMode = 0;
  }

  switch (obsqsv->codec) {
  case QSV_CODEC_AVC:
    codec = "H.264";
    if (astrcmpi(profile, "baseline") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_BASELINE;
    } else if (astrcmpi(profile, "main") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_MAIN;
    } else if (astrcmpi(profile, "high") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_HIGH;
    } else if (astrcmpi(profile, "extended") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_EXTENDED;
    } else if (astrcmpi(profile, "high10") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_HIGH10;
    } else if (astrcmpi(profile, "high422") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_HIGH_422;
    } else if (astrcmpi(profile, "progressive high") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AVC_PROGRESSIVE_HIGH;
    }
    break;
  case QSV_CODEC_HEVC:
    codec = "HEVC";
    if (astrcmpi(profile, "main") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_MAIN;

      if (obs_p010_tex_active()) {
        blog(LOG_WARNING, "[qsv encoder] Forcing main10 for P010");
        obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
      }

    } else if (astrcmpi(profile, "main10") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
    } else if (astrcmpi(profile, "rext") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_REXT;
    }

    if (astrcmpi(hevc_tier, "main") == 0) {
      obsqsv->params.HEVCTier = MFX_TIER_HEVC_MAIN;
    } else {
      obsqsv->params.HEVCTier = MFX_TIER_HEVC_HIGH;
    }
    break;
  case QSV_CODEC_AV1:
    codec = "AV1";
    if (astrcmpi(profile, "main") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AV1_MAIN;
    } else if (astrcmpi(profile, "high") == 0) {
      obsqsv->params.CodecProfile = MFX_PROFILE_AV1_HIGH;
    }
    break;
  case QSV_CODEC_VP9:
    codec = "VP9";
    obsqsv->params.CodecProfile = obsqsv->params.video_fmt_10bit
                                      ? static_cast<mfxU16>(MFX_PROFILE_VP9_2)
                                      : static_cast<mfxU16>(MFX_PROFILE_VP9_0);
    break;
  }
  obsqsv->params.VideoFormat = 5;
  obsqsv->params.VideoFullRange = voi->range == VIDEO_RANGE_FULL;

  switch (voi->colorspace) {
  case VIDEO_CS_601:
    obsqsv->params.ColourPrimaries = 6;
    obsqsv->params.TransferCharacteristics = 6;
    obsqsv->params.MatrixCoefficients = 6;
    obsqsv->params.ChromaSampleLocTypeTopField = 0;
    obsqsv->params.ChromaSampleLocTypeBottomField = 0;
    break;
  case VIDEO_CS_DEFAULT:
  case VIDEO_CS_709:
    obsqsv->params.ColourPrimaries = 1;
    obsqsv->params.TransferCharacteristics = 1;
    obsqsv->params.MatrixCoefficients = 1;
    obsqsv->params.ChromaSampleLocTypeTopField = 0;
    obsqsv->params.ChromaSampleLocTypeBottomField = 0;
    break;
  case VIDEO_CS_SRGB:
    obsqsv->params.ColourPrimaries = 1;
    obsqsv->params.TransferCharacteristics = 13;
    obsqsv->params.MatrixCoefficients = 1;
    obsqsv->params.ChromaSampleLocTypeTopField = 0;
    obsqsv->params.ChromaSampleLocTypeBottomField = 0;
    break;
  case VIDEO_CS_2100_PQ:
    obsqsv->params.ColourPrimaries = 9;
    obsqsv->params.TransferCharacteristics = 16;
    obsqsv->params.MatrixCoefficients = 9;
    obsqsv->params.ChromaSampleLocTypeTopField = 2;
    obsqsv->params.ChromaSampleLocTypeBottomField = 2;
    break;
  case VIDEO_CS_2100_HLG:
    obsqsv->params.ColourPrimaries = 9;
    obsqsv->params.TransferCharacteristics = 18;
    obsqsv->params.MatrixCoefficients = 9;
    obsqsv->params.ChromaSampleLocTypeTopField = 2;
    obsqsv->params.ChromaSampleLocTypeBottomField = 2;
  }

  const auto pq = voi->colorspace == VIDEO_CS_2100_PQ;
  const auto hlg = voi->colorspace == VIDEO_CS_2100_HLG;
  if (pq || hlg) {
    const int hdr_nominal_peak_level =
        pq ? static_cast<int>(obs_get_video_hdr_nominal_peak_level())
           : (hlg ? 1000 : 0);

    obsqsv->params.DisplayPrimariesX[0] = 13250;
    obsqsv->params.DisplayPrimariesX[1] = 7500;
    obsqsv->params.DisplayPrimariesX[2] = 34000;
    obsqsv->params.DisplayPrimariesY[0] = 34500;
    obsqsv->params.DisplayPrimariesY[1] = 3000;
    obsqsv->params.DisplayPrimariesY[2] = 16000;
    obsqsv->params.WhitePointX = 15635;
    obsqsv->params.WhitePointY = 16450;
    obsqsv->params.MaxDisplayMasteringLuminance =
        static_cast<mfxU32>(hdr_nominal_peak_level * 10000);
    obsqsv->params.MinDisplayMasteringLuminance = 0;

    obsqsv->params.MaxContentLightLevel =
        static_cast<mfxU16>(hdr_nominal_peak_level);
    obsqsv->params.MaxPicAverageLightLevel =
        static_cast<mfxU16>(hdr_nominal_peak_level);
  }

  /* internal convenience parameter, overrides rate control param
   * XXX: Deprecated */
  if (cbr_override) {
    blog(LOG_WARNING, "\"cbr\" setting has been deprecated for all encoders!  "
                      "Please set \"rate_control\" to \"CBR\" instead.  "
                      "Forcing CBR mode.  "
                      "(Note to all: this is why you shouldn't use strings for "
                      "common settings)");
    rate_control = "CBR";
  }

  if (astrcmpi(low_delay_hrd, "ON") == 0) {
    obsqsv->params.bLowDelayHRD = true;
  } else if (astrcmpi(low_delay_hrd, "OFF") == 0) {
    obsqsv->params.bLowDelayHRD = false;
  }

  if (astrcmpi(mv_overpic_boundaries, "ON") == 0) {
    obsqsv->params.nMotionVectorsOverPicBoundaries = true;
  } else if (astrcmpi(mv_overpic_boundaries, "OFF") == 0) {
    obsqsv->params.nMotionVectorsOverPicBoundaries = false;
  }

  if (astrcmpi(hrd_conformance, "ON") == 0) {
    obsqsv->params.bHRDConformance = 1;
  } else if (astrcmpi(hrd_conformance, "OFF") == 0) {
    obsqsv->params.bHRDConformance = 0;
  }

  if (astrcmpi(mbbrc, "ON") == 0) {
    obsqsv->params.bMBBRC = 1;
  } else if (astrcmpi(mbbrc, "OFF") == 0) {
    obsqsv->params.bMBBRC = 0;
  }

  if (astrcmpi(extbrc, "ON") == 0) {
    obsqsv->params.bExtBRC = 1;
  }

  if (astrcmpi(enctools, "ON") == 0) {
    obsqsv->params.bEncTools = true;
  } else {
    obsqsv->params.bEncTools = false;
  }

  if (astrcmpi(directbias_adjustment, "ON") == 0) {
    obsqsv->params.bDirectBiasAdjustment = 1;
  } else if (astrcmpi(directbias_adjustment, "OFF") == 0) {
    obsqsv->params.bDirectBiasAdjustment = 0;
  }

  if (astrcmpi(mv_costscaling_factor, "OFF") == 0) {
    obsqsv->params.nMVCostScalingFactor = 0;
  } else if (astrcmpi(mv_costscaling_factor, "1/2") == 0) {
    obsqsv->params.nMVCostScalingFactor = 1;
  } else if (astrcmpi(mv_costscaling_factor, "1/4") == 0) {
    obsqsv->params.nMVCostScalingFactor = 2;
  } else if (astrcmpi(mv_costscaling_factor, "1/8") == 0) {
    obsqsv->params.nMVCostScalingFactor = 3;
  }

  if (astrcmpi(use_raw_ref, "ON") == 0) {
    obsqsv->params.bRawRef = 1;
  } else if (astrcmpi(use_raw_ref, "OFF") == 0) {
    obsqsv->params.bRawRef = 0;
  }

  if (astrcmpi(p_pyramid, "PYRAMID") == 0) {
    obsqsv->params.bPPyramid = 1;
  } else if (astrcmpi(p_pyramid, "SIMPLE") == 0) {
    obsqsv->params.bPPyramid = 0;
  }

  if (astrcmpi(globalmotionbias_adjustment, "ON") == 0) {
    obsqsv->params.bGlobalMotionBiasAdjustment = 1;
  } else if (astrcmpi(globalmotionbias_adjustment, "OFF") == 0) {
    obsqsv->params.bGlobalMotionBiasAdjustment = 0;
  }

  if (astrcmpi(lookahead, "HQ") == 0) {
    obsqsv->params.bLookahead = true;

    obsqsv->params.nLADepth = 0;
    if (astrcmpi(lookahead_latency, "HIGH") == 0) {
      obsqsv->params.nLADepth = 100;
    } else if (astrcmpi(lookahead_latency, "NORMAL") == 0) {
      obsqsv->params.nLADepth = 60;
    } else if (astrcmpi(lookahead_latency, "LOW") == 0) {
      obsqsv->params.nLADepth = 40;
    } else if (astrcmpi(lookahead_latency, "VERYLOW") == 0) {
      obsqsv->params.nLADepth = 20;
    }

    if (astrcmpi(lookahead_ds, "SLOW") == 0) {
      obsqsv->params.nLookAheadDS = 0;
    } else if (astrcmpi(lookahead_ds, "MEDIUM") == 0) {
      obsqsv->params.nLookAheadDS = 1;
    } else if (astrcmpi(lookahead_ds, "FAST") == 0) {
      obsqsv->params.nLookAheadDS = 2;
    }
  } else if (astrcmpi(lookahead, "LP") == 0) {
    if (gop_ref_dist > 0 && gop_ref_dist < 17) {
      obsqsv->params.bLookahead = true;
      obsqsv->params.bLookaheadLP = true;
      if (gop_ref_dist > 8) {
        obsqsv->params.nLADepth = 8;
      } else {
        obsqsv->params.nLADepth = static_cast<mfxU16>(gop_ref_dist);
      }
    }
  } else {
    obsqsv->params.bLookahead = false;
  }

  if (astrcmpi(intra_ref_encoding, "ON") == 0) {
    obsqsv->params.bIntraRefEncoding = 1;
  } else if (astrcmpi(intra_ref_encoding, "OFF") == 0) {
    obsqsv->params.bIntraRefEncoding = 0;
  }

  if (astrcmpi(adaptive_cqm, "ON") == 0) {
    obsqsv->params.bAdaptiveCQM = 1;
  } else if (astrcmpi(adaptive_cqm, "OFF") == 0) {
    obsqsv->params.bAdaptiveCQM = 0;
  }

  if (astrcmpi(adaptive_ltr, "ON") == 0) {
    obsqsv->params.bAdaptiveLTR = 1;
  } else if (astrcmpi(adaptive_ltr, "OFF") == 0) {
    obsqsv->params.bAdaptiveLTR = 0;
  }

  if (astrcmpi(adaptive_i, "ON") == 0) {
    obsqsv->params.bAdaptiveI = 1;
  } else if (astrcmpi(adaptive_i, "OFF") == 0) {
    obsqsv->params.bAdaptiveI = 0;
  }

  if (astrcmpi(adaptive_b, "ON") == 0) {
    obsqsv->params.bAdaptiveB = 1;
  } else if (astrcmpi(adaptive_b, "OFF") == 0) {
    obsqsv->params.bAdaptiveB = 0;
  }

  if (astrcmpi(adaptive_ref, "ON") == 0) {
    obsqsv->params.bAdaptiveRef = 1;
  } else if (astrcmpi(adaptive_ref, "OFF") == 0) {
    obsqsv->params.bAdaptiveRef = 0;
  }

  if (astrcmpi(low_power, "ON") == 0) {
    obsqsv->params.bLowpower = true;
  } else if (astrcmpi(low_power, "OFF") == 0) {
    obsqsv->params.bLowpower = false;
  }

  if (astrcmpi(rdo, "ON") == 0) {
    obsqsv->params.bRDO = 1;
  } else if (astrcmpi(rdo, "OFF") == 0) {
    obsqsv->params.bRDO = 0;
  }

  if (astrcmpi(trellis, "I") == 0) {
    obsqsv->params.nTrellis = 1;
  } else if (astrcmpi(trellis, "IP") == 0) {
    obsqsv->params.nTrellis = 2;
  } else if (astrcmpi(trellis, "IPB") == 0) {
    obsqsv->params.nTrellis = 3;
  } else if (astrcmpi(trellis, "IB") == 0) {
    obsqsv->params.nTrellis = 4;
  } else if (astrcmpi(trellis, "P") == 0) {
    obsqsv->params.nTrellis = 5;
  } else if (astrcmpi(trellis, "PB") == 0) {
    obsqsv->params.nTrellis = 6;
  } else if (astrcmpi(trellis, "B") == 0) {
    obsqsv->params.nTrellis = 7;
  }

  if (astrcmpi(hevc_sao, "DISABLE") == 0) {
    obsqsv->params.nSAO = 0;
  } else if (astrcmpi(hevc_sao, "LUMA") == 0) {
    obsqsv->params.nSAO = 1;
  } else if (astrcmpi(hevc_sao, "CHROMA") == 0) {
    obsqsv->params.nSAO = 2;
  } else if (astrcmpi(hevc_sao, "ALL") == 0) {
    obsqsv->params.nSAO = 3;
  }

  if (astrcmpi(hevc_gpb, "ON") == 0) {
    obsqsv->params.bGPB = 1;
  } else if (astrcmpi(hevc_gpb, "OFF") == 0) {
    obsqsv->params.bGPB = 0;
  }

  if (astrcmpi(rate_control, "CBR") == 0) {
    obsqsv->params.RateControl = MFX_RATECONTROL_CBR;
  } else if (astrcmpi(rate_control, "VBR") == 0) {
    obsqsv->params.RateControl = MFX_RATECONTROL_VBR;
  } else if (astrcmpi(rate_control, "CQP") == 0) {
    obsqsv->params.RateControl = MFX_RATECONTROL_CQP;
  } else if (astrcmpi(rate_control, "ICQ") == 0) {
    obsqsv->params.RateControl = MFX_RATECONTROL_ICQ;
  } else if (astrcmpi(rate_control, "LA_EXT_ICQ") == 0) {
    obsqsv->params.RateControl = MFX_RATECONTROL_ICQ;
  }

  if (astrcmpi(denoise_mode, "DEFAULT") == 0) {
    obsqsv->params.nVPPDenoiseMode = 0;
  } else if (astrcmpi(denoise_mode, "AUTO | BDRATE | PRE ENCODE") == 0) {
    obsqsv->params.nVPPDenoiseMode = 1;
  } else if (astrcmpi(denoise_mode, "AUTO | ADJUST | POST ENCODE") == 0) {
    obsqsv->params.nVPPDenoiseMode = 2;
  } else if (astrcmpi(denoise_mode, "AUTO | SUBJECTIVE | PRE ENCODE") == 0) {
    obsqsv->params.nVPPDenoiseMode = 3;
  } else if (astrcmpi(denoise_mode, "MANUAL | PRE ENCODE") == 0) {
    obsqsv->params.nVPPDenoiseMode = 4;
    obsqsv->params.nDenoiseStrength = static_cast<mfxU16>(denoise_strength);
  } else if (astrcmpi(denoise_mode, "MANUAL | POST ENCODE") == 0) {
    obsqsv->params.nVPPDenoiseMode = 5;
    obsqsv->params.nDenoiseStrength = static_cast<mfxU16>(denoise_strength);
  }

  if (astrcmpi(scaling_mode, "QUALITY | ADVANCED") == 0) {
    obsqsv->params.nVPPScalingMode = 1;
  } else if (astrcmpi(scaling_mode, "VEBOX | ADVANCED") == 0) {
    obsqsv->params.nVPPScalingMode = 2;
  } else if (astrcmpi(scaling_mode, "LOWPOWER | NEAREST NEIGHBOR") == 0) {
    obsqsv->params.nVPPScalingMode = 3;
  } else if (astrcmpi(scaling_mode, "LOWPOWER | ADVANCED") == 0) {
    obsqsv->params.nVPPScalingMode = 4;
  } else if (astrcmpi(scaling_mode, "AUTO") == 0) {
    obsqsv->params.nVPPScalingMode = 0;
  }

  if (astrcmpi(image_stab_mode, "UPSCALE") == 0) {
    obsqsv->params.nVPPImageStabMode = 1;
  } else if (astrcmpi(image_stab_mode, "BOXING") == 0) {
    obsqsv->params.nVPPImageStabMode = 2;
  } else if (astrcmpi(image_stab_mode, "AUTO") == 0) {
    obsqsv->params.nVPPScalingMode = 0;
  }

  if (astrcmpi(detail, "ON") == 0) {
    obsqsv->params.nVPPDetail = detail_factor;
  } else if (astrcmpi(detail, "OFF") == 0) {
    obsqsv->params.nVPPDetail = 0;
  }

  if (astrcmpi(perc_enc_prefilter, "ON") == 0) {
    obsqsv->params.bPercEncPrefilter = 1;
  } else if (astrcmpi(perc_enc_prefilter, "OFF") == 0) {
    obsqsv->params.bPercEncPrefilter = 0;
  }

  obsqsv->params.nAsyncDepth =
      static_cast<mfxU16>(obs_data_get_int(settings, "async_depth"));

  auto actual_cqp = cqp;
  if (obsqsv->codec == QSV_CODEC_AV1) {
    actual_cqp *= 4;
  }
  obsqsv->params.nQPI = static_cast<mfxU16>(actual_cqp);
  obsqsv->params.nQPP = static_cast<mfxU16>(actual_cqp);
  obsqsv->params.nQPB = static_cast<mfxU16>(actual_cqp);

  obsqsv->params.nTargetBitRate = static_cast<mfxU16>(target_bitrate / 100);
  obsqsv->params.bCustomBufferSize = (bool)custom_buffer_size;
  obsqsv->params.nBufferSize = static_cast<mfxU16>(buffer_size / 100);
  obsqsv->params.nMaxBitRate = static_cast<mfxU16>(max_bitrate / 100);
  obsqsv->params.nWidth = static_cast<mfxU16>(width);
  obsqsv->params.nHeight = static_cast<mfxU16>(height);
  obsqsv->params.nFpsNum = static_cast<mfxU16>(voi->fps_num);
  obsqsv->params.nFpsDen = static_cast<mfxU16>(voi->fps_den);

  obsqsv->params.nGOPRefDist = static_cast<mfxU16>(gop_ref_dist);
  obsqsv->params.nKeyIntSec = static_cast<mfxU16>(keyint_sec);
  obsqsv->params.nICQQuality = static_cast<mfxU16>(icq_quality);
  obsqsv->params.nNumRefFrame = static_cast<mfxU16>(num_ref_frame);
  obsqsv->params.nNumRefActiveP = static_cast<mfxU16>(num_ref_active_p);
  obsqsv->params.nNumRefActiveBL0 = static_cast<mfxU16>(num_ref_active_bl0);
  obsqsv->params.nNumRefActiveBL1 = static_cast<mfxU16>(num_ref_active_bl1);
  obsqsv->params.nWinBRCMaxAvgSize =
      static_cast<mfxU16>(winbrc_max_avg_size / 100);
  obsqsv->params.nWinBRCSize = static_cast<mfxU16>(winbrc_size);

  obsqsv->params.nIntraRefCycleSize = static_cast<mfxU16>(intra_ref_cycle_size);
  obsqsv->params.nIntraRefQPDelta = static_cast<mfxU16>(intra_ref_qp_delta);

  obsqsv->params.bVPPEnable = false;
  if ((obsqsv->params.nVPPDenoiseMode.has_value() ||
       obsqsv->params.nVPPDetail.has_value() ||
       obsqsv->params.nVPPScalingMode.has_value() ||
       obsqsv->params.nVPPImageStabMode.has_value() ||
       obsqsv->params.bPercEncPrefilter == true) &&
      astrcmpi(vpp, "ON") == 0) {
    if (voi->format == VIDEO_FORMAT_NV12) {
      obsqsv->params.bVPPEnable = true;
    } else {
      warn("VPP is only available with NV12 color format");
    }
  }

  switch (voi->format) {
  default:
  case VIDEO_FORMAT_NV12:
  case VIDEO_FORMAT_I420:
    obsqsv->params.nFourCC = MFX_FOURCC_NV12;
    obsqsv->params.nChromaFormat = MFX_CHROMAFORMAT_YUV420;
    break;
  case VIDEO_FORMAT_P010:
    obsqsv->params.nFourCC = MFX_FOURCC_P010;
    obsqsv->params.nChromaFormat = MFX_CHROMAFORMAT_YUV420;
    break;
  }
  info("\tDebug info:");
  info("\tCodec: %s", codec);
  info("\tRate control: %s\n", rate_control);

  if (obsqsv->params.RateControl != MFX_RATECONTROL_ICQ &&
      obsqsv->params.RateControl != MFX_RATECONTROL_CQP)
    info("\tTarget bitrate: %d", obsqsv->params.nTargetBitRate * 100);

  if (obsqsv->params.RateControl == MFX_RATECONTROL_VBR)
    info("\tMax bitrate: %d", obsqsv->params.nMaxBitRate * 100);

  if (obsqsv->params.RateControl == MFX_RATECONTROL_ICQ)
    info("\tICQ Quality: %d", obsqsv->params.nICQQuality);

  if (obsqsv->params.RateControl == MFX_RATECONTROL_CQP)
    info("\tCQP: %d", actual_cqp);

  info("\tFPS numerator: %d", voi->fps_num);
  info("\tFPS denominator: %d", voi->fps_den);
  info("\tOutput width: %d", width);
  info("\tOutput height: %d", height);
}

static bool update_settings(obs_qsv *obsqsv, obs_data_t *settings) {
  update_params(obsqsv, settings);
  return true;
}

static obs_properties_t *obs_qsv_props_h264([[maybe_unused]] void *) {
  return obs_qsv_props(QSV_CODEC_AVC);
}

static obs_properties_t *obs_qsv_props_av1([[maybe_unused]] void *) {

  return obs_qsv_props(QSV_CODEC_AV1);
}

static obs_properties_t *obs_qsv_props_hevc([[maybe_unused]] void *) {
  return obs_qsv_props(QSV_CODEC_HEVC);
}

static obs_properties_t *obs_qsv_props_vp9([[maybe_unused]] void *) {
  return obs_qsv_props(QSV_CODEC_VP9);
}

static obs_qsv *obs_qsv_create(enum qsv_codec codec, obs_data_t *settings,
                               obs_encoder_t *encoder, bool useTexAlloc) {

  obs_qsv *obsqsv = static_cast<obs_qsv *>(bzalloc(sizeof(obs_qsv)));

  if (!obsqsv) {
    bfree(obsqsv);
    return nullptr;
  }

  obsqsv->encoder = std::move(encoder);
  obsqsv->codec = std::move(codec);

  auto video = std::move(obs_encoder_video(encoder));
  auto voi = std::move(video_output_get_info(video));
  switch (voi->format) {
  case VIDEO_FORMAT_I010:
  case VIDEO_FORMAT_P010:
    if (codec == QSV_CODEC_AVC) {
      auto text = obs_module_text("10bitUnsupportedAvc");
      obs_encoder_set_last_error(encoder, text);
      blog(LOG_ERROR, "%s", text);
      bfree(obsqsv);
      return nullptr;
    }
    obsqsv->params.video_fmt_10bit = true;
    break;
  default:
    if (voi->colorspace == VIDEO_CS_2100_PQ ||
        voi->colorspace == VIDEO_CS_2100_HLG) {
      auto text = obs_module_text("8bitUnsupportedHdr");
      obs_encoder_set_last_error(encoder, text);
      blog(LOG_ERROR, "%s", text);
      bfree(obsqsv);
      return nullptr;
    }
  }

  update_settings(obsqsv, settings);

  pthread_mutex_lock(&g_QsvLock);
  obsqsv->context = qsv_encoder_open(&obsqsv->params, codec, useTexAlloc);
  pthread_mutex_unlock(&g_QsvLock);

  if (obsqsv->context == nullptr) {
    blog(LOG_WARNING, "qsv failed to load");
    bfree(obsqsv);
    return nullptr;
  } else {
    load_headers(obsqsv);
  }

  qsv_encoder_version(&g_verMajor, &g_verMinor);

  info("\tLibVPL version: %d.%d", g_verMajor, g_verMinor);

  if (!obsqsv->context) {
    bfree(obsqsv);
    return nullptr;
  }

  obsqsv->performance_token = os_request_high_performance("qsv encoding");

  return obsqsv;
}

static void *obs_qsv_create_h264(obs_data_t *settings, obs_encoder_t *encoder) {
  return obs_qsv_create(QSV_CODEC_AVC, settings, encoder, false);
}

static void *obs_qsv_create_av1(obs_data_t *settings, obs_encoder_t *encoder) {
  return obs_qsv_create(QSV_CODEC_AV1, settings, encoder, false);
}

static void *obs_qsv_create_hevc(obs_data_t *settings, obs_encoder_t *encoder) {
  return obs_qsv_create(QSV_CODEC_HEVC, settings, encoder, false);
}

static void *obs_qsv_create_vp9(obs_data_t *settings, obs_encoder_t *encoder) {
  return obs_qsv_create(QSV_CODEC_VP9, settings, encoder, false);
}

static void *obs_qsv_create_tex(enum qsv_codec codec, obs_data_t *settings,
                                obs_encoder_t *encoder,
                                const char *fallback_id) {
  struct obs_video_info ovi;
  obs_get_video_info(&ovi);

  if (!adapters[ovi.adapter].is_intel) {
    info(">>> app not on intel GPU, fall back to non-texture encoder");
    return obs_encoder_create_rerouted(encoder,
                                       static_cast<const char *>(fallback_id));
  }

  if (static_cast<int>(obs_data_get_int(settings, "gpu_number")) > 0) {
    info(">>> custom GPU is selected. OBS Studio does not support "
         "transferring textures to third-party adapters, fall back to "
         "non-texture encoder");
    return obs_encoder_create_rerouted(encoder,
                                       static_cast<const char *>(fallback_id));
  }

#if !defined(_WIN32) || !defined(_WIN64)
  info(">>> unsupported platform for texture encode");
  return obs_encoder_create_rerouted(encoder,
                                     static_cast<const char *>(fallback_id));
#endif

  if (codec == QSV_CODEC_AV1 && !adapters[ovi.adapter].supports_av1) {
    info(">>> cap on different device, fall back to non-texture "
         "sharing AV1 qsv encoder");
    return obs_encoder_create_rerouted(encoder,
                                       static_cast<const char *>(fallback_id));
  }

  bool gpu_texture_active = obs_nv12_tex_active();

  if (codec != QSV_CODEC_AVC)
    gpu_texture_active = gpu_texture_active || obs_p010_tex_active();

  if (!gpu_texture_active) {
    info(">>> gpu tex not active, fall back to non-texture encoder");
    return obs_encoder_create_rerouted(encoder,
                                       static_cast<const char *>(fallback_id));
  }

  if (obs_encoder_scaling_enabled(encoder)) {
    if (!obs_encoder_gpu_scaling_enabled(encoder)) {
      info(">>> encoder CPU scaling active, fall back to non-texture encoder");
      return obs_encoder_create_rerouted(
          encoder, static_cast<const char *>(fallback_id));
    }
    info(">>> encoder GPU scaling active");
  }

  info(">>> Texture encoder");
  return obs_qsv_create(codec, settings, encoder, true);
}

static void *obs_qsv_create_tex_h264(obs_data_t *settings,
                                     obs_encoder_t *encoder) {
  return obs_qsv_create_tex(QSV_CODEC_AVC, settings, encoder,
                            "obs_qsv_vpl_h264");
}

static void *obs_qsv_create_tex_av1(obs_data_t *settings,
                                    obs_encoder_t *encoder) {
  return obs_qsv_create_tex(QSV_CODEC_AV1, settings, encoder,
                            "obs_qsv_vpl_av1");
}

static void *obs_qsv_create_tex_hevc(obs_data_t *settings,
                                     obs_encoder_t *encoder) {
  return obs_qsv_create_tex(QSV_CODEC_HEVC, settings, encoder,
                            "obs_qsv_vpl_hevc");
}

static void *obs_qsv_create_tex_vp9(obs_data_t *settings,
                                    obs_encoder_t *encoder) {
  return obs_qsv_create_tex(QSV_CODEC_HEVC, settings, encoder,
                            "obs_qsv_vpl_vp9");
}

static const char *obs_qsv_getname_h264([[maybe_unused]] void *) {
  return "QuickSync oneVPL H.264";
}

static const char *obs_qsv_getname_av1([[maybe_unused]] void *) {
  return "QuickSync oneVPL AV1";
}

static const char *obs_qsv_getname_hevc([[maybe_unused]] void *) {
  return "QuickSync oneVPL HEVC";
}

static const char *obs_qsv_getname_vp9([[maybe_unused]] void *) {
  return "QuickSync oneVPL VP9";
}

static void obs_qsv_defaults_h264(obs_data_t *settings) {
  obs_qsv_defaults(settings, 2, QSV_CODEC_AVC);
}

static void obs_qsv_defaults_av1(obs_data_t *settings) {
  obs_qsv_defaults(settings, 2, QSV_CODEC_AV1);
}

static void obs_qsv_defaults_hevc(obs_data_t *settings) {
  obs_qsv_defaults(settings, 2, QSV_CODEC_HEVC);
}

static void obs_qsv_defaults_vp9(obs_data_t *settings) {
  obs_qsv_defaults(settings, 2, QSV_CODEC_VP9);
}

obs_encoder_info obs_qsv_h264_encoder = {.id = "obs_qsv_vpl_h264",
                                         .type = OBS_ENCODER_VIDEO,
                                         .codec = "h264",
                                         .get_name = obs_qsv_getname_h264,
                                         .create = obs_qsv_create_h264,
                                         .destroy = obs_qsv_destroy,
                                         .encode = obs_qsv_encode,
                                         .get_defaults = obs_qsv_defaults_h264,
                                         .get_properties = obs_qsv_props_h264,
                                         .update = obs_qsv_update,
                                         .get_extra_data = obs_qsv_extra_data,
                                         .get_sei_data = obs_qsv_sei,
                                         .get_video_info = obs_qsv_video_info,
                                         .caps = OBS_ENCODER_CAP_DYN_BITRATE | OBS_ENCODER_CAP_INTERNAL/* |
                                                 OBS_ENCODER_CAP_ROI*/};

obs_encoder_info
    obs_qsv_h264_encoder_tex = {.id = "obs_qsv_vpl_h264_tex",
                                .type = OBS_ENCODER_VIDEO,
                                .codec = "h264",
                                .get_name = obs_qsv_getname_h264,
                                .create = obs_qsv_create_tex_h264,
                                .destroy = obs_qsv_destroy,
                                .get_defaults = obs_qsv_defaults_h264,
                                .get_properties = obs_qsv_props_h264,
                                .update = obs_qsv_update,
                                .get_extra_data = obs_qsv_extra_data,
                                .get_sei_data = obs_qsv_sei,
                                .get_video_info = obs_qsv_video_info,
                                .caps = OBS_ENCODER_CAP_DYN_BITRATE |
                                        OBS_ENCODER_CAP_PASS_TEXTURE /* |
           OBS_ENCODER_CAP_ROI*/,
                                .encode_texture2 = obs_qsv_encode_tex};

obs_encoder_info obs_qsv_av1_encoder = {
    .id = "obs_qsv_vpl_av1",
    .type = OBS_ENCODER_VIDEO,
    .codec = "av1",
    .get_name = obs_qsv_getname_av1,
    .create = obs_qsv_create_av1,
    .destroy = obs_qsv_destroy,
    .encode = obs_qsv_encode,
    .get_defaults = obs_qsv_defaults_av1,
    .get_properties = obs_qsv_props_av1,
    .update = obs_qsv_update,
    .get_extra_data = obs_qsv_extra_data,
    .get_sei_data = obs_qsv_sei,
    .get_video_info = obs_qsv_video_info,
    .caps = OBS_ENCODER_CAP_DYN_BITRATE | OBS_ENCODER_CAP_INTERNAL
    /* | OBS_ENCODER_CAP_ROI*/};

obs_encoder_info
    obs_qsv_av1_encoder_tex = {.id = "obs_qsv_vpl_av1_tex",
                               .type = OBS_ENCODER_VIDEO,
                               .codec = "av1",
                               .get_name = obs_qsv_getname_av1,
                               .create = obs_qsv_create_tex_av1,
                               .destroy = obs_qsv_destroy,
                               .get_defaults = obs_qsv_defaults_av1,
                               .get_properties = obs_qsv_props_av1,
                               .update = obs_qsv_update,
                               .get_extra_data = obs_qsv_extra_data,
                               .get_sei_data = obs_qsv_sei,
                               .get_video_info = obs_qsv_video_info,
                               .caps = OBS_ENCODER_CAP_DYN_BITRATE |
                                       OBS_ENCODER_CAP_PASS_TEXTURE /* |
          OBS_ENCODER_CAP_ROI*/,
                               .encode_texture2 = obs_qsv_encode_tex};

obs_encoder_info obs_qsv_hevc_encoder = {
    .id = "obs_qsv_vpl_hevc",
    .type = OBS_ENCODER_VIDEO,
    .codec = "hevc",
    .get_name = obs_qsv_getname_hevc,
    .create = obs_qsv_create_hevc,
    .destroy = obs_qsv_destroy,
    .encode = obs_qsv_encode,
    .get_defaults = obs_qsv_defaults_hevc,
    .get_properties = obs_qsv_props_hevc,
    .update = obs_qsv_update,
    .get_extra_data = obs_qsv_extra_data,
    .get_sei_data = obs_qsv_sei,
    .get_video_info = obs_qsv_video_info,
    .caps = OBS_ENCODER_CAP_DYN_BITRATE | OBS_ENCODER_CAP_INTERNAL
    /* | OBS_ENCODER_CAP_ROI*/};

obs_encoder_info
    obs_qsv_hevc_encoder_tex = {.id = "obs_qsv_vpl_hevc_tex",
                                .type = OBS_ENCODER_VIDEO,
                                .codec = "hevc",
                                .get_name = obs_qsv_getname_hevc,
                                .create = obs_qsv_create_tex_hevc,
                                .destroy = obs_qsv_destroy,
                                .get_defaults = obs_qsv_defaults_hevc,
                                .get_properties = obs_qsv_props_hevc,
                                .update = obs_qsv_update,
                                .get_extra_data = obs_qsv_extra_data,
                                .get_sei_data = obs_qsv_sei,
                                .get_video_info = obs_qsv_video_info,
                                .caps = OBS_ENCODER_CAP_DYN_BITRATE |
                                        OBS_ENCODER_CAP_PASS_TEXTURE /* |
           OBS_ENCODER_CAP_ROI*/,
                                .encode_texture2 = obs_qsv_encode_tex};

obs_encoder_info obs_qsv_vp9_encoder = {
    .id = "obs_qsv_vpl_vp9",
    .type = OBS_ENCODER_VIDEO,
    .codec = "vp9",
    .get_name = obs_qsv_getname_vp9,
    .create = obs_qsv_create_vp9,
    .destroy = obs_qsv_destroy,
    .encode = obs_qsv_encode,
    .get_defaults = obs_qsv_defaults_vp9,
    .get_properties = obs_qsv_props_vp9,
    .update = obs_qsv_update,
    .get_extra_data = obs_qsv_extra_data,
    .get_sei_data = obs_qsv_sei,
    .get_video_info = obs_qsv_video_info,
    .caps = OBS_ENCODER_CAP_DYN_BITRATE | OBS_ENCODER_CAP_INTERNAL
    /* | OBS_ENCODER_CAP_ROI*/};

obs_encoder_info
    obs_qsv_vp9_encoder_tex = {.id = "obs_qsv_vpl_vp9_tex",
                               .type = OBS_ENCODER_VIDEO,
                               .codec = "vp9",
                               .get_name = obs_qsv_getname_vp9,
                               .create = obs_qsv_create_tex_vp9,
                               .destroy = obs_qsv_destroy,
                               .get_defaults = obs_qsv_defaults_vp9,
                               .get_properties = obs_qsv_props_vp9,
                               .update = obs_qsv_update,
                               .get_extra_data = obs_qsv_extra_data,
                               .get_sei_data = obs_qsv_sei,
                               .get_video_info = obs_qsv_video_info,
                               .caps = OBS_ENCODER_CAP_DYN_BITRATE |
                                       OBS_ENCODER_CAP_PASS_TEXTURE /* |
          OBS_ENCODER_CAP_ROI*/,
                               .encode_texture2 = obs_qsv_encode_tex};