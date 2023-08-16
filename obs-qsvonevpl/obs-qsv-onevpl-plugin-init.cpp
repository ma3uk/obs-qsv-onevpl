/*

This file is provided under a dual BSD/GPLv2 license.  When using or
redistributing this file, you may do so under either license.

GPL LICENSE SUMMARY

Copyright(c) Oct. 2015 Intel Corporation.

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Contact Information:

Seung-Woo Kim, seung-woo.kim@intel.com
705 5th Ave S #500, Seattle, WA 98104

BSD LICENSE

Copyright(c) <date> Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdio>
#include <cinttypes>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>
#include <obs.h>
#include <obs-module.h>
#include <obs-hevc.h>
#include <obs-avc.h>
#include <util/threading.h>
#include "obs-qsv-onevpl-encoder.hpp"

#ifndef _STDINT_H_INCLUDED
#define _STDINT_H_INCLUDED
#endif

#define do_log(level, format, ...)                 \
	blog(level, "[qsv encoder: '%s'] " format, \
	     obs_encoder_get_name(obsqsv->encoder), ##__VA_ARGS__)

#define error(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#ifndef GS_INVALID_HANDLE
#define GS_INVALID_HANDLE (uint32_t) - 1
#endif
/* ------------------------------------------------------------------------- */

struct obs_qsv {
	obs_encoder_t *encoder;

	enum qsv_codec codec;

	qsv_param_t params;
	qsv_t *context;

	DARRAY(uint8_t) packet_data;

	uint8_t *extra_data;
	uint8_t *sei;

	size_t extra_data_size;
	size_t sei_size;

	os_performance_token_t *performance_token;
};

/* ------------------------------------------------------------------------- */

static pthread_mutex_t g_QsvLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned short g_verMajor;
static unsigned short g_verMinor;
static bool g_bFirst;

static const char *obs_qsv_getname_h264(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "QuickSync oneVPL H.264";
}

static const char *obs_qsv_getname_av1(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "QuickSync oneVPL AV1";
}

static const char *obs_qsv_getname_hevc(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "QuickSync oneVPL HEVC";
}

static const char *obs_qsv_getname_vp9(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "QuickSync oneVPL VP9";
}

static void obs_qsv_stop(void *data);

static void clear_data(obs_qsv *obsqsv)
{
	if (obsqsv->context) {
		pthread_mutex_lock(&g_QsvLock);
		qsv_encoder_close(obsqsv->context);
		obsqsv->context = nullptr;
		obsqsv->sei = nullptr;
		obsqsv->sei_size = 0;
		obsqsv->extra_data = nullptr;
		obsqsv->extra_data_size = 0;
		pthread_mutex_unlock(&g_QsvLock);
	}
}

static void obs_qsv_destroy(void *data)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

	if (obsqsv) {
		os_end_high_performance(obsqsv->performance_token);
		clear_data(obsqsv);
		da_free(obsqsv->packet_data);

		delete obsqsv;
	}
}

static void obs_qsv_defaults(obs_data_t *settings, int ver,
			     enum qsv_codec codec)
{
	obs_data_set_default_int(settings, "device_num", 0);
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
	obs_data_set_default_string(settings, "denoise_mode", "OFF");
	obs_data_set_default_int(settings, "denoise_strength", 50);

	obs_data_set_default_string(settings, "tune_quality", "OFF");
	obs_data_set_default_string(settings, "adaptive_i", "AUTO");
	obs_data_set_default_string(settings, "adaptive_b", "AUTO");
	obs_data_set_default_string(settings, "adaptive_ref", "AUTO");
	obs_data_set_default_string(settings, "adaptive_cqm", "ON");
	obs_data_set_default_string(settings, "adaptive_ltr", "AUTO");
	obs_data_set_default_string(settings, "use_raw_ref", "AUTO");
	obs_data_set_default_string(settings, "rdo", "AUTO");
	obs_data_set_default_string(settings, "mbbrc", "AUTO");
	obs_data_set_default_string(settings, "weighted_pred", "AUTO");
	obs_data_set_default_string(settings, "weighted_bi_pred", "AUTO");
	obs_data_set_default_string(settings, "trellis", "AUTO");
	obs_data_set_default_int(settings, "num_ref_frame", 0);
	obs_data_set_default_string(settings, "globalmotionbias_adjustment",
				    "AUTO");
	obs_data_set_default_string(settings, "mv_costscaling_factor",
				    "DEFAULT");
	obs_data_set_default_string(settings, "directbias_adjustment", "AUTO");
	obs_data_set_default_string(settings, "mv_overpic_boundaries", "AUTO");
	obs_data_set_default_int(settings, "la_depth", 0);

	obs_data_set_default_string(settings, "lookahead", "OFF");
	obs_data_set_default_string(settings, "lookahead_latency", "NORMAL");
	obs_data_set_default_string(settings, "lookahead_ds", "MEDIUM");
	obs_data_set_default_string(settings, "low_power", "ON");
	obs_data_set_default_string(settings, "extbrc", "OFF");
	obs_data_set_default_string(settings, "enctools", "OFF");
	obs_data_set_default_string(settings, "hevc_sao", "AUTO");
	obs_data_set_default_string(settings, "hevc_gpb", "AUTO");

	obs_data_set_default_string(settings, "intra_ref_encoding", "OFF");
	obs_data_set_default_string(settings, "intra_ref_type", "VERTICAL");
	obs_data_set_default_int(settings, "intra_ref_cycle_size", 2);
	obs_data_set_default_int(settings, "intra_ref_qp_delta", 0);
}

static void obs_qsv_defaults_h264(obs_data_t *settings)
{
	obs_qsv_defaults(settings, 2, QSV_CODEC_AVC);
}

static void obs_qsv_defaults_av1(obs_data_t *settings)
{
	obs_qsv_defaults(settings, 2, QSV_CODEC_AV1);
}

static void obs_qsv_defaults_hevc(obs_data_t *settings)
{
	obs_qsv_defaults(settings, 2, QSV_CODEC_HEVC);
}

static void obs_qsv_defaults_vp9(obs_data_t *settings)
{
	obs_qsv_defaults(settings, 2, QSV_CODEC_VP9);
}

static inline void add_strings(obs_property_t *list, const char *const *strings)
{
	while (*strings) {
		obs_property_list_add_string(list, *strings, *strings);
		strings++;
	}
}

#define TEXT_SPEED obs_module_text("TargetUsage")
#define TEXT_TARGET_BITRATE obs_module_text("Bitrate")
#define TEXT_CUSTOM_BUFFER_SIZE obs_module_text("CustomBufferSize")
#define TEXT_BUFFER_SIZE obs_module_text("BufferSize")
#define TEXT_MAX_BITRATE obs_module_text("MaxBitrate")
#define TEXT_PROFILE obs_module_text("Profile")
#define TEXT_HEVC_TIER obs_module_text("Tier")
#define TEXT_RATE_CONTROL obs_module_text("RateControl")
#define TEXT_ICQ_QUALITY obs_module_text("ICQQuality")
#define TEXT_KEYINT_SEC obs_module_text("KeyframeIntervalSec")
#define TEXT_GOP_REF_DIST obs_module_text("GOPRefDist")
#define TEXT_MBBRC obs_module_text("MBBRC")
#define TEXT_NUM_REF_FRAME obs_module_text("NumRefFrame")
#define TEXT_NUM_REF_FRAME_LAYERS obs_module_text("NumRefFrameLayers")
#define TEXT_LA_DS obs_module_text("LookaheadDownSampling")
#define TEXT_GLOBAL_MOTION_BIAS_ADJUSTMENT \
	obs_module_text("GlobalMotionBiasAdjustment")
#define TEXT_DIRECT_BIAS_ADJUSTMENT obs_module_text("DirectBiasAdjusment")
#define TEXT_ADAPTIVE_I obs_module_text("AdaptiveI")
#define TEXT_ADAPTIVE_B obs_module_text("AdaptiveB")
#define TEXT_ADAPTIVE_REF obs_module_text("AdaptiveRef")
#define TEXT_ADAPTIVE_CQM obs_module_text("AdaptiveCQM")
#define TEXT_TRELLIS obs_module_text("Trellis")
#define TEXT_LA obs_module_text("Lookahead")
#define TEXT_LA_DEPTH obs_module_text("LookaheadDepth")
#define TEXT_LA_LATENCY obs_module_text("Lookahead latency")
#define TEXT_MV_OVER_PIC_BOUNDARIES \
	obs_module_text("MotionVectorsOverpicBoundaries")
#define TEXT_WEIGHTED_PRED obs_module_text("WeightedPred")
#define TEXT_WEIGHTED_BI_PRED obs_module_text("WeightedBiPred")
#define TEXT_USE_RAW_REF obs_module_text("UseRawRef")
#define TEXT_MV_COST_SCALING_FACTOR obs_module_text("MVCostScalingFactor")
#define TEXT_RDO obs_module_text("RDO")
#define TEXT_ASYNC_DEPTH obs_module_text("AsyncDepth")
#define TEXT_WINBRC_MAX_AVG_SIZE obs_module_text("WinBRCMaxAvgSize")
#define TEXT_WINBRC_SIZE obs_module_text("WinBRCSize")
#define TEXT_ADAPTIVE_LTR obs_module_text("AdaptiveLTR")
#define TEXT_LOW_POWER obs_module_text("LowPower mode")
#define TEXT_HEVC_SAO obs_module_text("SampleAdaptiveOffset")
#define TEXT_HEVC_GPB obs_module_text("GPB")
#define TEXT_TUNE_QUALITY_MODE obs_module_text("TuneQualityMode")
#define TEXT_EXT_BRC obs_module_text("ExtBRC")
#define TEXT_ENC_TOOLS obs_module_text("EncTools")
#define TEXT_DEVICE_NUM obs_module_text("Select GPU")
#define TEXT_DENOISE_STRENGTH obs_module_text("Denoise strength")
#define TEXT_DENOISE_MODE obs_module_text("Denoise mode")

#define TEXT_INTRA_REF_ENCODING obs_module_text("IntraRefEncoding")
#define TEXT_INTRA_REF_TYPE obs_module_text("IntraRefType")
#define TEXT_INTRA_REF_CYCLE_SIZE obs_module_text("IntraRefCycleSize")
#define TEXT_INTRA_REF_QP_DELTA obs_module_text("IntraRefQPDelta")

static bool rate_control_modified(obs_properties_t *ppts, obs_property_t *p,
				  obs_data_t *settings)
{
	const char *rate_control =
		obs_data_get_string(settings, "rate_control");
	bool bVisible = astrcmpi(rate_control, "VBR") == 0;
	p = obs_properties_get(ppts, "max_bitrate");
	obs_property_set_visible(p, bVisible);

	bVisible = astrcmpi(rate_control, "CQP") == 0 ||
		   astrcmpi(rate_control, "ICQ") == 0;
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

	bVisible = astrcmpi(rate_control, "CBR") == 0 ||
		   astrcmpi(rate_control, "VBR") == 0;
	p = obs_properties_get(ppts, "enctools");
	obs_property_set_visible(p, bVisible);
	p = obs_properties_get(ppts, "extbrc");
	obs_property_set_visible(p, bVisible);

	const char *lookahead = obs_data_get_string(settings, "lookahead");

	bVisible = (astrcmpi(rate_control, "CBR") == 0 ||
		    astrcmpi(rate_control, "VBR") == 0);
	p = obs_properties_get(ppts, "lookahead");
	obs_property_set_visible(p, bVisible);
	if (bVisible) {

		bVisible = astrcmpi(lookahead, "ON") == 0 &&
			   (astrcmpi(rate_control, "CBR") == 0 ||
			    astrcmpi(rate_control, "VBR") == 0);
		p = obs_properties_get(ppts, "lookahead_latency");
		obs_property_set_visible(p, bVisible);
		p = obs_properties_get(ppts, "lookahead_ds");
		obs_property_set_visible(p, bVisible);
		if (!bVisible) {
			obs_data_erase(settings, "lookahead_latency");
			obs_data_erase(settings, "lookahead_ds");
		}
	} else {
		obs_data_set_string(settings, "lookahead", "OFF");
		obs_data_erase(settings, "lookahead_latency");
		obs_data_erase(settings, "lookahead_ds");
	}

	bVisible = astrcmpi(rate_control, "VBR") == 0;
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

	return true;
}

static bool visible_modified(obs_properties_t *ppts, obs_property_t *p,
			     obs_data_t *settings)
{

	const char *global_motion_bias_adjustment_enable =
		obs_data_get_string(settings, "globalmotionbias_adjustment");
	bool bVisible =
		((astrcmpi(global_motion_bias_adjustment_enable, "ON") == 0));
	p = obs_properties_get(ppts, "mv_costscaling_factor");
	obs_property_set_visible(p, bVisible);
	if (!bVisible) {
		obs_data_erase(settings, "mv_costscaling_factor");
	}

	const char *denoise_mode =
		obs_data_get_string(settings, "denoise_mode");
	bVisible = astrcmpi(denoise_mode, "MANUAL | PRE ENCODE") == 0 ||
		   astrcmpi(denoise_mode, "MANUAL | POST ENCODE") == 0;
	p = obs_properties_get(ppts, "denoise_strength");
	obs_property_set_visible(p, bVisible);

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

static obs_properties_t *obs_qsv_props(enum qsv_codec codec, void *unused,
				       int ver)
{
	ver;
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *prop;

	prop = obs_properties_add_list(props, "rate_control", TEXT_RATE_CONTROL,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);

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
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_usage_names);

	if (codec != QSV_CODEC_VP9) {
		prop = obs_properties_add_list(props, "profile", TEXT_PROFILE,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);

		if (codec == QSV_CODEC_AVC) {
			add_strings(prop, qsv_profile_names_h264);
		} else if (codec == QSV_CODEC_AV1) {
			add_strings(prop, qsv_profile_names_av1);
		} else if (codec == QSV_CODEC_HEVC) {
			add_strings(prop, qsv_profile_names_hevc);
		}

		if (codec == QSV_CODEC_HEVC) {
			prop = obs_properties_add_list(props, "hevc_tier",
						       TEXT_HEVC_TIER,
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
			add_strings(prop, qsv_profile_tiers_hevc);
		}
	}

	prop = obs_properties_add_list(props, "extbrc", TEXT_EXT_BRC,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(prop,
					  obs_module_text("ExtBRC.ToolTip"));
	add_strings(prop, qsv_params_condition_extbrc);
	obs_property_set_modified_callback(prop, rate_control_modified);

	prop = obs_properties_add_list(props, "enctools", TEXT_ENC_TOOLS,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_set_long_description(prop,
					  obs_module_text("EncTools.ToolTip"));
	add_strings(prop, qsv_params_condition);
	obs_property_set_modified_callback(prop, rate_control_modified);

	//prop = obs_properties_add_list(props, "tune_quality",
	//			       TEXT_TUNE_QUALITY_MODE,
	//			       OBS_COMBO_TYPE_LIST,
	//			       OBS_COMBO_FORMAT_STRING);
	//add_strings(prop, qsv_params_condition_tune_quality);
	//obs_property_set_long_description(
	//	prop, obs_module_text("TuneQualityMode.ToolTip"));

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
	obs_property_set_long_description(
		prop, obs_module_text("BufferSize.ToolTip"));

	prop = obs_properties_add_int(props, "winbrc_max_avg_size",
				      TEXT_WINBRC_MAX_AVG_SIZE, 0, 10000000,
				      50);
	obs_property_set_long_description(
		prop, obs_module_text("WinBrcMaxAvgSize.ToolTip"));
	obs_property_int_set_suffix(prop, " Kbps");

	prop = obs_properties_add_int(props, "winbrc_size", TEXT_WINBRC_SIZE, 0,
				      10000, 1);
	obs_property_set_long_description(
		prop, obs_module_text("WinBrcSize.ToolTip"));

	prop = obs_properties_add_int(props, "max_bitrate", TEXT_MAX_BITRATE,
				      50, 10000000, 50);
	obs_property_int_set_suffix(prop, " Kbps");

	obs_properties_add_int(props, "cqp", "CQP", 1,
			       codec == QSV_CODEC_AV1 ? 63 : 51, 1);

	obs_properties_add_int(props, "icq_quality", TEXT_ICQ_QUALITY, 1, 51,
			       1);

	//prop = obs_properties_add_list(props, "gop_opt_flag", TEXT_GOP_OPT_FLAG,
	//			       OBS_COMBO_TYPE_LIST,
	//			       OBS_COMBO_FORMAT_STRING);
	//add_strings(prop, qsv_params_condition_gop);
	//obs_property_set_long_description(
	//	prop, obs_module_text("GOPOptFlag.ToolTip"));

	prop = obs_properties_add_int(props, "keyint_sec", TEXT_KEYINT_SEC, 0,
				      20, 1);
	obs_property_int_set_suffix(prop, " s");

	obs_properties_add_int_slider(props, "num_ref_frame",
				      TEXT_NUM_REF_FRAME, 0,
				      (codec == QSV_CODEC_AV1 ? 7 : 15), 1);

	if (codec != QSV_CODEC_AVC) {
		obs_properties_add_int_slider(props, "num_ref_frame_layers",
					      TEXT_NUM_REF_FRAME_LAYERS, 1, 9,
					      1);
	}
	if (codec == QSV_CODEC_HEVC) {
		prop = obs_properties_add_list(props, "hevc_gpb", TEXT_HEVC_GPB,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition_tristate);
		obs_property_set_long_description(
			prop, obs_module_text("GPB.ToolTip"));
	}

	prop = obs_properties_add_int_slider(props, "gop_ref_dist",
					     TEXT_GOP_REF_DIST, 1,
					     (codec == QSV_CODEC_AV1) ? 33 : 16,
					     1);
	obs_property_set_long_description(
		prop, obs_module_text("GOPRefDist.Tooltip"));
	obs_property_long_description(prop);

	obs_properties_add_int(props, "async_depth", TEXT_ASYNC_DEPTH, 1, 1000,
			       1);
	if (codec != QSV_CODEC_VP9) {
		prop = obs_properties_add_list(props, "mbbrc", TEXT_MBBRC,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition_tristate);
		obs_property_set_modified_callback(prop, rate_control_modified);
		obs_property_set_long_description(
			prop, obs_module_text("MBBRC.ToolTip"));
	}

	/*Intel broke this parameter, it is responsible for automatically detecting the scene change, but it gives a compatibility error*/
	//if (codec == QSV_CODEC_AVC) {
	//	prop = obs_properties_add_list(props, "repartitioncheck_enable",
	//		TEXT_REPARTITION_CHECK_ENABLE,
	//		OBS_COMBO_TYPE_LIST,
	//		OBS_COMBO_FORMAT_STRING);
	//	add_strings(prop, qsv_repartition_check_condition);
	//	obs_property_set_long_description(prop,
	//		obs_module_text("PreferredMode.ToolTip"));
	//}

	prop = obs_properties_add_list(props, "rdo", TEXT_RDO,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(prop, obs_module_text("RDO.ToolTip"));

	prop = obs_properties_add_list(props, "adaptive_i", TEXT_ADAPTIVE_I,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(prop,
					  obs_module_text("AdaptiveI.ToolTip"));

	prop = obs_properties_add_list(props, "adaptive_b", TEXT_ADAPTIVE_B,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(prop,
					  obs_module_text("AdaptiveB.ToolTip"));

	prop = obs_properties_add_list(props, "adaptive_ref", TEXT_ADAPTIVE_REF,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("AdaptiveRef.ToolTip"));

	prop = obs_properties_add_list(props, "adaptive_ltr", TEXT_ADAPTIVE_LTR,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("AdaptiveLTR.ToolTip"));

	prop = obs_properties_add_list(props, "adaptive_cqm", TEXT_ADAPTIVE_CQM,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("AdaptiveCQM.ToolTip"));

	prop = obs_properties_add_list(props, "use_raw_ref", TEXT_USE_RAW_REF,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(prop,
					  obs_module_text("UseRawRef.ToolTip"));

	/*Intel broke this parameter, turning it on should instruct the encoder to achieve quality, and turning it off for speed, but now it gives a compatibility error*/
	//prop = obs_properties_add_list(props, "fade_detection",
	//	TEXT_FADE_DETECTION, OBS_COMBO_TYPE_LIST,
	//	OBS_COMBO_FORMAT_STRING);
	//add_strings(prop, qsv_params_condition);
	//obs_property_set_long_description(prop,
	//	obs_module_text("FadeDetection.ToolTip"));

	prop = obs_properties_add_list(props, "globalmotionbias_adjustment",
				       TEXT_GLOBAL_MOTION_BIAS_ADJUSTMENT,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_modified_callback(prop, visible_modified);
	obs_property_set_long_description(
		prop, obs_module_text("GlobalMotionBiasAdjustment.ToolTip"));

	prop = obs_properties_add_list(props, "mv_costscaling_factor",
				       TEXT_MV_COST_SCALING_FACTOR,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_mv_cost_scaling);
	obs_property_set_long_description(
		prop, obs_module_text("MVCostScalingFactor.ToolTip"));

	prop = obs_properties_add_list(props, "directbias_adjustment",
				       TEXT_DIRECT_BIAS_ADJUSTMENT,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("DirectBiasAdjusment.ToolTip"));

	prop = obs_properties_add_list(props, "weighted_pred",
				       TEXT_WEIGHTED_PRED, OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("WeightedPred.ToolTip"));

	prop = obs_properties_add_list(props, "weighted_bi_pred",
				       TEXT_WEIGHTED_BI_PRED,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("WeightedBiPred.ToolTip"));

	prop = obs_properties_add_list(props, "mv_overpic_boundaries",
				       TEXT_MV_OVER_PIC_BOUNDARIES,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_long_description(
		prop, obs_module_text("MVOverpicBoundaries.ToolTip"));

	prop = obs_properties_add_list(props, "trellis", TEXT_TRELLIS,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_trellis);
	obs_property_set_long_description(prop,
					  obs_module_text("Trellis.ToolTip"));

	prop = obs_properties_add_list(props, "lookahead", TEXT_LA,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition);
	obs_property_set_modified_callback(prop, rate_control_modified);
	obs_property_set_long_description(
		prop, obs_module_text("LookaheadDS.ToolTip"));

	prop = obs_properties_add_list(props, "lookahead_ds", TEXT_LA_DS,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_lookahead_ds);
	obs_property_set_long_description(
		prop, obs_module_text("LookaheadDS.ToolTip"));

	prop = obs_properties_add_list(props, "lookahead_latency",
				       TEXT_LA_LATENCY, OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_lookahead_latency);
	obs_property_set_long_description(
		prop, obs_module_text("LookaheadLatency.ToolTip"));

	//obs_properties_add_int_slider(props, "la_depth", TEXT_LA_DEPTH, 0, 200,
	//			      1);
	if (codec == QSV_CODEC_AVC) {
		prop = obs_properties_add_list(props, "denoise_mode",
					       TEXT_DENOISE_MODE,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition_denoise_mode);
		obs_property_set_long_description(
			prop, obs_module_text("DenoiseMode.ToolTip"));
		obs_property_set_modified_callback(prop, visible_modified);

		obs_properties_add_int_slider(props, "denoise_strength",
					      TEXT_DENOISE_STRENGTH, 1, 100, 1);
	}
	prop = obs_properties_add_list(props, "low_power", TEXT_LOW_POWER,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(prop, qsv_params_condition_tristate);
	obs_property_set_modified_callback(prop, rate_control_modified);
	obs_property_set_long_description(prop,
					  obs_module_text("LowPower.ToolTip"));
	if (codec != QSV_CODEC_AV1) {
		prop = obs_properties_add_list(props, "intra_ref_encoding",
					       TEXT_INTRA_REF_ENCODING,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition);
		obs_property_set_modified_callback(prop, visible_modified);
		obs_property_set_long_description(
			prop, obs_module_text("IntraRefEncoding.ToolTip"));

		prop = obs_properties_add_list(props, "intra_ref_type",
					       TEXT_INTRA_REF_TYPE,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition_intra_ref_encoding);
		obs_property_set_long_description(
			prop, obs_module_text("IntraRefType.ToolTip"));

		obs_properties_add_int(props, "intra_ref_cycle_size",
				       TEXT_INTRA_REF_CYCLE_SIZE, 2, 1000, 1);
		obs_property_set_long_description(
			prop, obs_module_text("IntraRefCycleSize.ToolTip"));

		obs_properties_add_int(props, "intra_ref_qp_delta",
				       TEXT_INTRA_REF_QP_DELTA, -51, 51, 1);
		obs_property_set_long_description(
			prop, obs_module_text("IntraRefQPDelta.ToolTip"));
	}

	obs_properties_add_int(props, "device_num", "Select GPU:", 0, 5, 1);
	obs_property_set_long_description(prop,
					  obs_module_text("DeviceNum.ToolTip"));

	/*This is not a necessary parameter,
		its inclusion greatly spoils the picture*/
	//prop = obs_properties_add_list(props, "deblocking", TEXT_DEBLOCKING,
	//			       OBS_COMBO_TYPE_LIST,
	//			       OBS_COMBO_FORMAT_STRING);
	//add_strings(prop, qsv_params_condition_tristate);
	//obs_property_set_long_description(
	//	prop, obs_module_text("Deblocking.ToolTip"));
	//obs_property_set_modified_callback(prop, rate_control_modified);

	if (codec == QSV_CODEC_HEVC) {
		prop = obs_properties_add_list(props, "hevc_sao", TEXT_HEVC_SAO,
					       OBS_COMBO_TYPE_LIST,
					       OBS_COMBO_FORMAT_STRING);
		add_strings(prop, qsv_params_condition_hevc_sao);
		obs_property_set_long_description(
			prop, obs_module_text("SAO.ToolTip"));
	}

	return props;
}

static obs_properties_t *obs_qsv_props_h264(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_qsv_props(QSV_CODEC_AVC, unused, 2);
}

static obs_properties_t *obs_qsv_props_av1(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_qsv_props(QSV_CODEC_AV1, unused, 2);
}

static obs_properties_t *obs_qsv_props_hevc(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_qsv_props(QSV_CODEC_HEVC, unused, 2);
}

static obs_properties_t *obs_qsv_props_vp9(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_qsv_props(QSV_CODEC_VP9, unused, 2);
}

static void update_params(obs_qsv *obsqsv, obs_data_t *settings)
{
	video_t *video = obs_encoder_video(obsqsv->encoder);
	const video_output_info *voi = video_output_get_info(video);
	int device_num =
		static_cast<int>(obs_data_get_int(settings, "device_num"));
	const char *target_usage =
		obs_data_get_string(settings, "target_usage");
	const char *profile = obs_data_get_string(settings, "profile");
	const char *hevc_tier = obs_data_get_string(settings, "hevc_tier");
	const char *rate_control =
		obs_data_get_string(settings, "rate_control");
	int target_bitrate =
		static_cast<int>(obs_data_get_int(settings, "bitrate"));
	bool custom_buffer_size =
		obs_data_get_bool(settings, "custom_buffer_size");
	int buffer_size =
		static_cast<int>(obs_data_get_int(settings, "buffer_size"));
	int max_bitrate =
		static_cast<int>(obs_data_get_int(settings, "max_bitrate"));
	int cqp = static_cast<int>(obs_data_get_int(settings, "cqp"));
	//int ver = (int)obs_data_get_int(settings, "__ver");
	int icq_quality =
		static_cast<int>(obs_data_get_int(settings, "icq_quality"));
	int keyint_sec =
		static_cast<int>(obs_data_get_int(settings, "keyint_sec"));
	bool cbr_override = obs_data_get_bool(settings, "cbr");
	int gop_ref_dist =
		static_cast<int>(obs_data_get_int(settings, "gop_ref_dist"));
	const char *mbbrc = obs_data_get_string(settings, "mbbrc");
	const char *codec = "";
	const char *adaptive_i = obs_data_get_string(settings, "adaptive_i");
	const char *adaptive_b = obs_data_get_string(settings, "adaptive_b");
	const char *adaptive_ref =
		obs_data_get_string(settings, "adaptive_ref");
	const char *adaptive_cqm =
		obs_data_get_string(settings, "adaptive_cqm");
	const char *low_power = obs_data_get_string(settings, "low_power");
	const char *adaptive_ltr =
		obs_data_get_string(settings, "adaptive_ltr");
	//const char *gop_opt_flag =
	//	obs_data_get_string(settings, "gop_opt_flag");
	const char *use_raw_ref = obs_data_get_string(settings, "use_raw_ref");
	int winbrc_max_avg_size = static_cast<int>(
		obs_data_get_int(settings, "winbrc_max_avg_size"));
	int winbrc_size =
		static_cast<int>(obs_data_get_int(settings, "winbrc_size"));
	int denoise_strength = static_cast<int>(
		obs_data_get_int(settings, "denoise_strength"));
	const char *denoise_mode =
		obs_data_get_string(settings, "denoise_mode");
	const char *rdo = obs_data_get_string(settings, "rdo");
	const char *weighted_pred =
		obs_data_get_string(settings, "weighted_pred");
	const char *weighted_bi_pred =
		obs_data_get_string(settings, "weighted_bi_pred");
	const char *trellis = obs_data_get_string(settings, "trellis");
	int num_ref_frame =
		static_cast<int>(obs_data_get_int(settings, "num_ref_frame"));
	int num_ref_frame_layers = static_cast<int>(
		obs_data_get_int(settings, "num_ref_frame_layers"));
	const char *globalmotionbias_adjustment =
		obs_data_get_string(settings, "globalmotionbias_adjustment");
	const char *mv_costscaling_factor =
		obs_data_get_string(settings, "mv_costscaling_factor");
	const char *lookahead = obs_data_get_string(settings, "lookahead");
	const char *lookahead_ds =
		obs_data_get_string(settings, "lookahead_ds");
	const char *lookahead_latency =
		obs_data_get_string(settings, "lookahead_latency");
	const char *directbias_adjustment =
		obs_data_get_string(settings, "directbias_adjustment");
	const char *mv_overpic_boundaries =
		obs_data_get_string(settings, "mv_overpic_boundaries");
	const char *hevc_sao = obs_data_get_string(settings, "hevc_sao");
	//const char *hevc_ctu = obs_data_get_string(settings, "hevc_ctu");
	const char *hevc_gpb = obs_data_get_string(settings, "hevc_gpb");
	const char *tune_quality =
		obs_data_get_string(settings, "tune_quality");
	//const char *deblocking = obs_data_get_string(settings, "deblocking");
	const char *extbrc = obs_data_get_string(settings, "extbrc");
	const char *enctools = obs_data_get_string(settings, "enctools");
	const char *intra_ref_encoding =
		obs_data_get_string(settings, "intra_ref_encoding");
	const char *intra_ref_type =
		obs_data_get_string(settings, "intra_ref_type");
	int intra_ref_cycle_size = static_cast<int>(
		obs_data_get_int(settings, "intra_ref_cycle_size"));
	int intra_ref_qp_delta = static_cast<int>(
		obs_data_get_int(settings, "intra_ref_qp_delta"));

	int width = static_cast<int>(obs_encoder_get_width(obsqsv->encoder));
	int height = static_cast<int>(obs_encoder_get_height(obsqsv->encoder));

	if (astrcmpi(target_usage, "TU1 (Veryslow)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_1;
		blog(LOG_INFO, "Target usage set: TU1 (Veryslow)");
	} else if (astrcmpi(target_usage, "TU2 (Slower)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_2;
		blog(LOG_INFO, "Target usage set: TU2 (Slower)");
	} else if (astrcmpi(target_usage, "TU3 (Slow)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_3;
		blog(LOG_INFO, "Target usage set: TU3 (Slow)");
	} else if (astrcmpi(target_usage, "TU4 (Balanced)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_4;
		blog(LOG_INFO, "Target usage set: TU4 (Balanced)");
	} else if (astrcmpi(target_usage, "TU5 (Fast)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_5;
		blog(LOG_INFO, "Target usage set: TU5 (Fast)");
	} else if (astrcmpi(target_usage, "TU6 (Faster)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_6;
		blog(LOG_INFO, "Target usage set: TU6 (Faster)");
	} else if (astrcmpi(target_usage, "TU7 (Veryfast)") == 0) {
		obsqsv->params.nTargetUsage = MFX_TARGETUSAGE_7;
		blog(LOG_INFO, "Target usage set: TU7 (Veryfast)");
	}

	// Unsupported on dGPU
	//if (astrcmpi(repartitioncheck_enable, "QUALITY") == 0) {
	//	obsqsv->params.nRepartitionCheckEnable = 1;
	//}
	//else if (astrcmpi(repartitioncheck_enable, "PERFORMANCE") == 0) {
	//	obsqsv->params.nRepartitionCheckEnable = 0;
	//}
	//else if (astrcmpi(repartitioncheck_enable, "AUTO") == 0) {
	//	obsqsv->params.nRepartitionCheckEnable = 2;
	//}

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
		obsqsv->params.nTuneQualityMode = 6;
	} else if (astrcmpi(tune_quality, "OFF") == 0) {
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
			obsqsv->params.CodecProfile =
				MFX_PROFILE_AVC_PROGRESSIVE_HIGH;
		}
		break;
	case QSV_CODEC_HEVC:
		codec = "HEVC";
		if (astrcmpi(profile, "main") == 0) {
			obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_MAIN;

			if (obs_p010_tex_active()) {
				blog(LOG_WARNING,
				     "[qsv encoder] Forcing main10 for P010");
				obsqsv->params.CodecProfile =
					MFX_PROFILE_HEVC_MAIN10;
			}

		} else if (astrcmpi(profile, "main10") == 0) {
			obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
		} else if (astrcmpi(profile, "rext") == 0) {
			obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_REXT;
		} /* else if (astrcmpi(profile, "scc") == 0) {
			obsqsv->params.CodecProfile = MFX_PROFILE_HEVC_SCC;
		}*/

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
		obsqsv->params.CodecProfile =
			obsqsv->params.video_fmt_10bit
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

	const bool pq = voi->colorspace == VIDEO_CS_2100_PQ;
	const bool hlg = voi->colorspace == VIDEO_CS_2100_HLG;
	if (pq || hlg) {
		const int hdr_nominal_peak_level =
			pq ? static_cast<int>(
				     obs_get_video_hdr_nominal_peak_level())
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
		warn("\"cbr\" setting has been deprecated for all encoders!  "
		     "Please set \"rate_control\" to \"CBR\" instead.  "
		     "Forcing CBR mode.  "
		     "(Note to all: this is why you shouldn't use strings for "
		     "common settings)");
		rate_control = "CBR";
	}

	if (astrcmpi(mv_overpic_boundaries, "ON") == 0) {
		obsqsv->params.nMotionVectorsOverPicBoundaries = 1;
	} else if (astrcmpi(mv_overpic_boundaries, "OFF") == 0) {
		obsqsv->params.nMotionVectorsOverPicBoundaries = 0;
	} else {
		obsqsv->params.nMotionVectorsOverPicBoundaries = -1;
	}

	//if (astrcmpi(gop_opt_flag, "OPEN") == 0) {
	//	obsqsv->params.bGopOptFlag = 1;
	//} else {
	//	obsqsv->params.bGopOptFlag = 0;
	//}

	if (astrcmpi(mbbrc, "ON") == 0) {
		obsqsv->params.bMBBRC = 1;
	} else if (astrcmpi(mbbrc, "OFF") == 0) {
		obsqsv->params.bMBBRC = 0;
	} else {
		obsqsv->params.bMBBRC = -1;
	}

	//if (astrcmpi(deblocking, "ON") == 0) {
	//	obsqsv->params.bDeblockingIdc = 1;
	//} else if (astrcmpi(deblocking, "OFF") == 0) {
	//	obsqsv->params.bDeblockingIdc = 0;
	//} else {
	//	obsqsv->params.bDeblockingIdc = -1;
	//}

	if (astrcmpi(extbrc, "IMPLICIT") == 0) {
		obsqsv->params.bExtBRC = 1;
	} else if (astrcmpi(extbrc, "EXPLICIT") == 0) {
		obsqsv->params.bExtBRC = 2;
	} else if (astrcmpi(extbrc, "OFF") == 0) {
		obsqsv->params.bExtBRC = 0;
	}

	if (astrcmpi(enctools, "ON") == 0) {
		obsqsv->params.bEncTools = true;
	} else {
		obsqsv->params.bEncTools = false;
	}

	/*Unsupported on dGPU*/
	//if (astrcmpi(fade_detection, "ON") == 0) {
	//	obsqsv->params.bFadeDetection = true;
	//}
	//else {
	//	obsqsv->params.bFadeDetection = false;
	//}

	if (astrcmpi(directbias_adjustment, "ON") == 0) {
		obsqsv->params.bDirectBiasAdjustment = 1;
	} else if (astrcmpi(directbias_adjustment, "OFF") == 0) {
		obsqsv->params.bDirectBiasAdjustment = 0;
	} else {
		obsqsv->params.bDirectBiasAdjustment = -1;
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

	if (astrcmpi(weighted_pred, "ON") == 0) {
		obsqsv->params.bWeightedPred = 1;
	} else if (astrcmpi(weighted_pred, "OFF") == 0) {
		obsqsv->params.bWeightedPred = 0;
	} else {
		obsqsv->params.bWeightedPred = -1;
	}

	if (astrcmpi(weighted_bi_pred, "ON") == 0) {
		obsqsv->params.bWeightedBiPred = 1;
	} else if (astrcmpi(weighted_bi_pred, "OFF") == 0) {
		obsqsv->params.bWeightedBiPred = 0;
	} else {
		obsqsv->params.bWeightedBiPred = -1;
	}

	if (astrcmpi(use_raw_ref, "ON") == 0) {
		obsqsv->params.bRawRef = 1;
	} else if (astrcmpi(use_raw_ref, "OFF") == 0) {
		obsqsv->params.bRawRef = 0;
	} else {
		obsqsv->params.bRawRef = -1;
	}

	if (astrcmpi(globalmotionbias_adjustment, "ON") == 0) {
		obsqsv->params.bGlobalMotionBiasAdjustment = 1;
	} else if (astrcmpi(globalmotionbias_adjustment, "OFF") == 0) {
		obsqsv->params.bGlobalMotionBiasAdjustment = 0;
	} else {
		obsqsv->params.bGlobalMotionBiasAdjustment = -1;
	}

	if (astrcmpi(lookahead, "ON") == 0) {
		obsqsv->params.bLookahead = true;
		if (astrcmpi(lookahead_ds, "SLOW") == 0) {
			obsqsv->params.nLookAheadDS = 0;
		} else if (astrcmpi(lookahead_ds, "MEDIUM") == 0) {
			obsqsv->params.nLookAheadDS = 1;
		} else if (astrcmpi(lookahead_ds, "FAST") == 0) {
			obsqsv->params.nLookAheadDS = 2;
		} else if (astrcmpi(lookahead_ds, "AUTO") == 0) {
			obsqsv->params.nLookAheadDS = -1;
		}

		if (astrcmpi(lookahead_latency, "HIGH") == 0) {
			obsqsv->params.nLADepth = 100;
		} else if (astrcmpi(lookahead_latency, "NORMAL") == 0) {
			obsqsv->params.nLADepth = 60;
		} else if (astrcmpi(lookahead_latency, "LOW") == 0) {
			obsqsv->params.nLADepth = 40;
		} else if (astrcmpi(lookahead_latency, "VERYLOW") == 0) {
			obsqsv->params.nLADepth = 20;
		}
	} else if (astrcmpi(lookahead, "OFF") == 0) {
		obsqsv->params.bLookahead = false;
	}


	if (astrcmpi(intra_ref_encoding, "ON") == 0) {
		obsqsv->params.bIntraRefEncoding = 1;
	} else if (astrcmpi(adaptive_cqm, "OFF") == 0) {
		obsqsv->params.bAdaptiveCQM = 0;
	} else {
		obsqsv->params.bAdaptiveCQM = -1;
	}

	if (astrcmpi(intra_ref_type, "VERTICAL") == 0) {
		obsqsv->params.nIntraRefType = 0;
	} else if (astrcmpi(intra_ref_type, "HORIZONTAL") == 0) {
		obsqsv->params.nIntraRefType = 1;
	} else {
		obsqsv->params.bAdaptiveCQM = -1;
	}

	if (astrcmpi(adaptive_cqm, "ON") == 0) {
		obsqsv->params.bAdaptiveCQM = 1;
	} else if (astrcmpi(adaptive_cqm, "OFF") == 0) {
		obsqsv->params.bAdaptiveCQM = 0;
	} else {
		obsqsv->params.bAdaptiveCQM = -1;
	}

	if (astrcmpi(adaptive_ltr, "ON") == 0) {
		obsqsv->params.bAdaptiveLTR = 1;
	} else if (astrcmpi(adaptive_ltr, "OFF") == 0) {
		obsqsv->params.bAdaptiveLTR = 0;
	} else {
		obsqsv->params.bAdaptiveLTR = -1;
	}

	if (astrcmpi(adaptive_i, "ON") == 0) {
		obsqsv->params.bAdaptiveI = 1;
	} else if (astrcmpi(adaptive_i, "OFF") == 0) {
		obsqsv->params.bAdaptiveI = 0;
	} else {
		obsqsv->params.bAdaptiveI = -1;
	}

	if (astrcmpi(adaptive_b, "ON") == 0) {
		obsqsv->params.bAdaptiveB = 1;
	} else if (astrcmpi(adaptive_b, "OFF") == 0) {
		obsqsv->params.bAdaptiveB = 0;
	} else {
		obsqsv->params.bAdaptiveB = -1;
	}

	if (astrcmpi(adaptive_ref, "ON") == 0) {
		obsqsv->params.bAdaptiveRef = 1;
	} else if (astrcmpi(adaptive_ref, "OFF") == 0) {
		obsqsv->params.bAdaptiveRef = 0;
	} else {
		obsqsv->params.bAdaptiveRef = -1;
	}

	if (astrcmpi(low_power, "ON") == 0) {
		obsqsv->params.bLowPower = 1;
	} else if (astrcmpi(low_power, "OFF") == 0) {
		obsqsv->params.bLowPower = 0;
	} else {
		obsqsv->params.bLowPower = -1;
	}

	if (astrcmpi(rdo, "ON") == 0) {
		obsqsv->params.bRDO = 1;
	} else if (astrcmpi(rdo, "OFF") == 0) {
		obsqsv->params.bRDO = 0;
	} else {
		obsqsv->params.bRDO = -1;
	}

	if (astrcmpi(trellis, "OFF") == 0) {
		obsqsv->params.nTrellis = 0;
	} else if (astrcmpi(trellis, "AUTO") == 0) {
		obsqsv->params.nTrellis = -1;
	} else if (astrcmpi(trellis, "I") == 0) {
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

	if (astrcmpi(denoise_mode, "OFF") == 0) {
		obsqsv->params.nDenoiseMode = -1;
	} else if (astrcmpi(denoise_mode, "DEFAULT") == 0) {
		obsqsv->params.nDenoiseMode = 0;
	} else if (astrcmpi(denoise_mode, "AUTO | BDRATE | PRE ENCODE") == 0) {
		obsqsv->params.nDenoiseMode = 1;
	} else if (astrcmpi(denoise_mode, "AUTO | ADJUST | POST ENCODE") == 0) {
		obsqsv->params.nDenoiseMode = 2;
	} else if (astrcmpi(denoise_mode, "AUTO | SUBJECTIVE | PRE ENCODE") ==
		   0) {
		obsqsv->params.nDenoiseMode = 3;
	} else if (astrcmpi(denoise_mode, "MANUAL | PRE ENCODE") == 0) {
		obsqsv->params.nDenoiseMode = 4;
		obsqsv->params.nDenoiseStrength =
			static_cast<mfxU16>(denoise_strength);
	} else if (astrcmpi(denoise_mode, "MANUAL | POST ENCODE") == 0) {
		obsqsv->params.nDenoiseMode = 5;
		obsqsv->params.nDenoiseStrength =
			static_cast<mfxU16>(denoise_strength);
	}

	if (astrcmpi(hevc_sao, "DISABLE") == 0) {
		obsqsv->params.nSAO = 0;
	} else if (astrcmpi(hevc_sao, "LUMA") == 0) {
		obsqsv->params.nSAO = 1;
	} else if (astrcmpi(hevc_sao, "CHROMA") == 0) {
		obsqsv->params.nSAO = 2;
	} else if (astrcmpi(hevc_sao, "ALL") == 0) {
		obsqsv->params.nSAO = 3;
	} else if (astrcmpi(hevc_sao, "AUTO") == 0) {
		obsqsv->params.nSAO = -1;
	}

	if (astrcmpi(hevc_gpb, "ON") == 0) {
		obsqsv->params.bGPB = 1;
	} else if (astrcmpi(hevc_gpb, "OFF") == 0) {
		obsqsv->params.bGPB = 0;
	} else {
		obsqsv->params.bGPB = -1;
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
		obsqsv->params.bLAExtBRC = 1;
		obsqsv->params.RateControl = MFX_RATECONTROL_ICQ;
	}

	obsqsv->params.nAsyncDepth =
		static_cast<mfxU16>(obs_data_get_int(settings, "async_depth"));

	int actual_cqp = cqp;
	if (obsqsv->codec == QSV_CODEC_AV1) {
		actual_cqp *= 4;
	}
	obsqsv->params.nQPI = static_cast<mfxU16>(actual_cqp);
	obsqsv->params.nQPP = static_cast<mfxU16>(actual_cqp);
	obsqsv->params.nQPB = static_cast<mfxU16>(actual_cqp);

	obsqsv->params.nDeviceNum = (device_num < 0) ? 0 : (device_num > 10) ? 10 : device_num;
	obsqsv->params.nTargetBitRate =
		static_cast<mfxU16>(target_bitrate / 100);
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
	obsqsv->params.nNumRefFrameLayers =
		static_cast<mfxU16>(num_ref_frame_layers);
	obsqsv->params.nWinBRCMaxAvgSize =
		static_cast<mfxU16>(winbrc_max_avg_size);
	obsqsv->params.nWinBRCSize = static_cast<mfxU16>(winbrc_size);

	obsqsv->params.nIntraRefCycleSize =
		static_cast<mfxU16>(intra_ref_cycle_size);
	obsqsv->params.nIntraRefQPDelta =
		static_cast<mfxU16>(intra_ref_qp_delta);

	switch (voi->format) {
	default:
	case VIDEO_FORMAT_NV12:
		obsqsv->params.nFourCC = MFX_FOURCC_NV12;
		obsqsv->params.nChromaFormat = MFX_CHROMAFORMAT_YUV420;
		break;
	case VIDEO_FORMAT_P010:
		obsqsv->params.nFourCC = MFX_FOURCC_P010;
		obsqsv->params.nChromaFormat = MFX_CHROMAFORMAT_YUV420;
		break;
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_BGRA:
		obsqsv->params.nFourCC = MFX_FOURCC_RGB4;
		obsqsv->params.nChromaFormat = MFX_CHROMAFORMAT_YUV444;
		break;
	}

	info("settings:\n"
	     "\tcodec:          %s\n"
	     "\trate_control:   %s",
	     codec, rate_control);

	if (obsqsv->params.RateControl != MFX_RATECONTROL_ICQ &&
	    obsqsv->params.RateControl != MFX_RATECONTROL_CQP)
		blog(LOG_INFO, "\ttarget_bitrate: %d",
		     obsqsv->params.nTargetBitRate * 100);

	if (obsqsv->params.RateControl == MFX_RATECONTROL_VBR)
		blog(LOG_INFO, "\tmax_bitrate:    %d",
		     obsqsv->params.nMaxBitRate * 100);

	if (obsqsv->params.RateControl == MFX_RATECONTROL_ICQ)
		blog(LOG_INFO, "\tICQ Quality:    %d",
		     obsqsv->params.nICQQuality);

	if (obsqsv->params.RateControl == MFX_RATECONTROL_CQP)
		blog(LOG_INFO, "\tCQP:            %d\n", actual_cqp);

	blog(LOG_INFO,
	     "\tfps_num:        %d\n"
	     "\tfps_den:        %d\n"
	     "\twidth:          %d\n"
	     "\theight:         %d",
	     voi->fps_num, voi->fps_den, width, height);

	info("debug info:");
}

static bool update_settings(obs_qsv *obsqsv, obs_data_t *settings)
{
	update_params(obsqsv, settings);
	return true;
}

static void load_headers(obs_qsv* obsqsv)
{
	DARRAY(uint8_t) header;
	DARRAY(uint8_t) sei;

	da_init(header);
	da_init(sei);

	uint8_t *pVPS, *pSPS, *pPPS;
	uint16_t nVPS, nSPS, nPPS;
	qsv_encoder_headers(obsqsv->context, &pVPS, &pSPS, &pPPS, &nVPS,
		&nSPS, &nPPS);
	if (obsqsv->codec == QSV_CODEC_HEVC) {
		da_push_back_array(header, pVPS, nVPS);
		da_push_back_array(header, pSPS, nSPS);
		da_push_back_array(header, pPPS, nPPS);
	}
	else if (obsqsv->codec == QSV_CODEC_AVC) {
		da_push_back_array(header, pSPS, nSPS);
		da_push_back_array(header, pPPS, nPPS);
	}
	else if (obsqsv->codec == QSV_CODEC_VP9 ||
		obsqsv->codec == QSV_CODEC_AV1) {
		da_push_back_array(header, pSPS, nSPS);
	}

	obsqsv->extra_data = header.array;
	obsqsv->extra_data_size = header.num;
	obsqsv->sei = sei.array;
	obsqsv->sei_size = sei.num;
}

static bool obs_qsv_update(void *data, obs_data_t *settings)
{
	data;
	settings;
	//struct obs_qsv *obsqsv = data;

	//obsqsv->params.bResetAllowed = false;

	//const char *bitrate_control =
	//	obs_data_get_string(settings, "rate_control");
	//if (astrcmpi(bitrate_control, "CBR") == 0 ||
	//    astrcmpi(bitrate_control, "VBR") == 0 ||
	//    astrcmpi(bitrate_control, "CQP") == 0 ||
	//    astrcmpi(bitrate_control, "ICQ") == 0) {
	//	obsqsv->params.bResetAllowed = true;
	//	obsqsv->params.nTargetBitRate =
	//		(mfxU16)obs_data_get_int(settings, "bitrate");
	//	obsqsv->params.nMaxBitRate =
	//		(mfxU16)obs_data_get_int(settings, "max_bitrate");
	//	obsqsv->params.nQPI = (mfxU16)obs_data_get_int(settings, "cqp");
	//	obsqsv->params.nQPP = (mfxU16)obs_data_get_int(settings, "cqp");
	//	obsqsv->params.nQPB = (mfxU16)obs_data_get_int(settings, "cqp");
	//	obsqsv->params.nICQQuality =
	//		(mfxU16)obs_data_get_int(settings, "icq_quality");
	//	if (!qsv_encoder_reconfig(obsqsv->context, &obsqsv->params)) {
	//		warn("Failed to reconfigure");
	//		return false;
	//	}
	//}
	return true;
}

static void *obs_qsv_create(enum qsv_codec codec, obs_data_t *settings,
			    obs_encoder_t *encoder)
{
	//obs_qsv *obsqsv =
	//	static_cast<obs_qsv *>(calloc(1, sizeof(obs_qsv) * 2));

	obs_qsv *obsqsv = new obs_qsv();
	if (!obsqsv) {
		/*delete obsqsv;*/
		return nullptr;
	}

	obsqsv->encoder = encoder;
	obsqsv->codec = codec;

	video_t *video = obs_encoder_video(encoder);
	const video_output_info *voi = video_output_get_info(video);
	switch (voi->format) {
	case VIDEO_FORMAT_I010:
	case VIDEO_FORMAT_P010:
		if (codec == QSV_CODEC_AVC) {
			const char *const text =
				obs_module_text("10bitUnsupportedAvc");
			obs_encoder_set_last_error(encoder, text);
			error("%s", text);
			delete obsqsv;
			return nullptr;
		}
		obsqsv->params.video_fmt_10bit = true;
		break;
	default:
		if (voi->colorspace == VIDEO_CS_2100_PQ ||
		    voi->colorspace == VIDEO_CS_2100_HLG) {
			const char *const text =
				obs_module_text("8bitUnsupportedHdr");
			obs_encoder_set_last_error(encoder, text);
			error("%s", text);
			delete obsqsv;
			return nullptr;
		}
	}

	if (update_settings(obsqsv, settings)) {
		pthread_mutex_lock(&g_QsvLock);
		obsqsv->context = qsv_encoder_open(&obsqsv->params, codec);
		pthread_mutex_unlock(&g_QsvLock);

		if (obsqsv->context == nullptr) {
			warn("qsv failed to load");
			delete obsqsv;
			return nullptr;
		} else {
			load_headers(obsqsv);
		}
	} else {
		warn("bad settings specified");
	}

	qsv_encoder_version(&g_verMajor, &g_verMinor);

	blog(LOG_INFO,
	     "\tmajor:          %d\n"
	     "\tminor:          %d",
	     g_verMajor, g_verMinor);

	if (!obsqsv->context) {
		delete obsqsv;
		return nullptr;
	}

	obsqsv->performance_token = os_request_high_performance("qsv encoding");

	g_bFirst = true;

	return obsqsv;
}

static void *obs_qsv_create_h264(obs_data_t *settings, obs_encoder_t *encoder)
{
	return obs_qsv_create(QSV_CODEC_AVC, settings, encoder);
}

static void *obs_qsv_create_av1(obs_data_t *settings, obs_encoder_t *encoder)
{
	return obs_qsv_create(QSV_CODEC_AV1, settings, encoder);
}

static void *obs_qsv_create_hevc(obs_data_t *settings, obs_encoder_t *encoder)
{
	return obs_qsv_create(QSV_CODEC_HEVC, settings, encoder);
}

static void *obs_qsv_create_vp9(obs_data_t *settings, obs_encoder_t *encoder)
{
	return obs_qsv_create(QSV_CODEC_VP9, settings, encoder);
}

static inline bool valid_sdr_format(enum video_format format)
{
	return format == VIDEO_FORMAT_NV12 || format == VIDEO_FORMAT_RGBA ||
	       format == VIDEO_FORMAT_BGRA;
}

static inline bool valid_hdr_format(enum video_format format)
{
	return format == VIDEO_FORMAT_NV12 || format == VIDEO_FORMAT_RGBA ||
	       format == VIDEO_FORMAT_P010 || format == VIDEO_FORMAT_BGRA;
}

static bool obs_qsv_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

	if (!obsqsv->context)
		return false;

	*extra_data = obsqsv->extra_data;
	*size = obsqsv->extra_data_size;
	return true;
}

static bool obs_qsv_sei(void *data, uint8_t **sei, size_t *size)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

	if (!obsqsv->context)
		return false;

	*sei = obsqsv->sei;
	*size = obsqsv->sei_size;
	return true;
}

static inline void cap_resolution(obs_qsv *obsqsv, video_scale_info *info)
{
	uint32_t width = obs_encoder_get_width(obsqsv->encoder);
	uint32_t height = obs_encoder_get_height(obsqsv->encoder);

	info->height = height;
	info->width = width;
}

static void obs_qsv_video_info(void *data, video_scale_info *info)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);
	enum video_format pref_format;

	/*pref_format = qsv_encoder_get_video_format(obsqsv->context);*/

	pref_format = obs_encoder_get_preferred_video_format(obsqsv->encoder);

	if (!valid_sdr_format(pref_format)) {
		pref_format = valid_sdr_format(info->format)
				      ? info->format
				      : VIDEO_FORMAT_NV12;
	}

	info->format = pref_format;
	cap_resolution(obsqsv, info);
}

static void obs_qsv_video_plus_hdr_info(void *data, video_scale_info *info)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);
	enum video_format pref_format;

	pref_format = obs_encoder_get_preferred_video_format(obsqsv->encoder);

	if (!valid_hdr_format(pref_format)) {
		pref_format = valid_hdr_format(info->format)
				      ? info->format
				      : VIDEO_FORMAT_NV12;
	}

	info->format = pref_format;
	cap_resolution(obsqsv, info);
}

static mfxU64 ts_obs_to_mfx(int64_t ts, const video_output_info *voi)
{
	return ts * 90000 / voi->fps_num;
}

static int64_t ts_mfx_to_obs(mfxI64 ts, const video_output_info *voi)
{
	int64_t div = 90000 * (int64_t)voi->fps_den;
	/* Round to the nearest integer multiple of `voi->fps_den`. */
	if (ts < 0) {
		return ((ts * voi->fps_num - div / 2) / (div * voi->fps_den));
	} else {
		return ((ts * voi->fps_num + div / 2) / (div * voi->fps_den));
	}

	return voi->fps_num * ts / 90000;
}

static void parse_packet_h264(obs_qsv *obsqsv, encoder_packet *packet,
			      mfxBitstream *pBS, const video_output_info *voi,
			      bool *received_packet)
{
	if (pBS == nullptr || pBS->DataLength == 0) {
		*received_packet = false;
		return;
	}

	da_resize(obsqsv->packet_data, 0);
	da_push_back_array(obsqsv->packet_data, &pBS->Data[pBS->DataOffset],
			   pBS->DataLength);

	packet->data = obsqsv->packet_data.array;
	packet->size = obsqsv->packet_data.num;
	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = ts_mfx_to_obs((mfxI64)pBS->TimeStamp, voi);
	//packet->keyframe = obs_avc_keyframe(packet->data, packet->size);
	packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_IDR) ||
			   (pBS->FrameType & MFX_FRAMETYPE_I);

	uint16_t frameType = pBS->FrameType;
	uint8_t priority;
	if ((frameType & MFX_FRAMETYPE_IDR) || (frameType & MFX_FRAMETYPE_I)) {
		priority = OBS_NAL_PRIORITY_HIGHEST;
	} else if ((frameType & MFX_FRAMETYPE_REF)) {
		priority = OBS_NAL_PRIORITY_HIGH;
	} else if ((frameType & MFX_FRAMETYPE_P)) {
		priority = OBS_NAL_PRIORITY_LOW;
	} else {
		priority = OBS_NAL_PRIORITY_DISPOSABLE;
	}

	packet->priority = static_cast<int>(priority);
	packet->drop_priority = 0;

	packet->dts = ts_mfx_to_obs(pBS->DecodeTimeStamp, voi);

	*received_packet = true;
	pBS->DataLength = 0;
}

//static bool obs_qsv_encode_texture_available(void *data,
//					     video_scale_info *info)
//{
//	UNUSED_PARAMETER(data);
//	return valid_sdr_format(info->format);
//}

static void parse_packet_av1(obs_qsv *obsqsv, encoder_packet *packet,
			     mfxBitstream *pBS, const video_output_info *voi,
			     bool *received_packet)
{
	if (pBS == NULL || pBS->DataLength == 0) {
		*received_packet = false;
		return;
	}

	da_resize(obsqsv->packet_data, 0);
	da_push_back_array(obsqsv->packet_data, &pBS->Data[pBS->DataOffset],
			   pBS->DataLength);

	packet->data = obsqsv->packet_data.array;
	packet->size = obsqsv->packet_data.num;
	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = ts_mfx_to_obs((mfxI64)pBS->TimeStamp, voi);
	packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_IDR);

	uint16_t frameType = pBS->FrameType;
	uint8_t priority;
	if ((frameType & MFX_FRAMETYPE_IDR)) {
		priority = OBS_NAL_PRIORITY_HIGHEST;
	} else if ((frameType & MFX_FRAMETYPE_I)) {
		priority = OBS_NAL_PRIORITY_HIGH;
	} else if ((frameType & MFX_FRAMETYPE_P)) {
		priority = OBS_NAL_PRIORITY_LOW;
	} else {
		priority = OBS_NAL_PRIORITY_DISPOSABLE;
	}

	packet->priority = static_cast<int>(priority);

	packet->dts = ts_mfx_to_obs(pBS->DecodeTimeStamp, voi);

	*received_packet = true;
	pBS->DataLength = 0;
}

static void parse_packet_hevc(obs_qsv *obsqsv, encoder_packet *packet,
			      mfxBitstream *pBS, const video_output_info *voi,
			      bool *received_packet)
{
	//bool is_vcl_packet = false;

	if (pBS == NULL || pBS->DataLength == 0) {
		*received_packet = false;
		return;
	}

	da_resize(obsqsv->packet_data, 0);
	da_push_back_array(obsqsv->packet_data, &pBS->Data[pBS->DataOffset],
			   pBS->DataLength);

	packet->data = obsqsv->packet_data.array;
	packet->size = obsqsv->packet_data.num;
	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = ts_mfx_to_obs((mfxI64)pBS->TimeStamp, voi);
	packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_IDR);

	uint16_t frameType = pBS->FrameType;
	uint8_t priority;
	if ((frameType & MFX_FRAMETYPE_IDR)) {
		priority = OBS_NAL_PRIORITY_HIGHEST;
	} else if ((frameType & MFX_FRAMETYPE_I)) {
		priority = OBS_NAL_PRIORITY_HIGH;
	} else if ((frameType & MFX_FRAMETYPE_P)) {
		priority = OBS_NAL_PRIORITY_LOW;
	} else {
		priority = OBS_NAL_PRIORITY_DISPOSABLE;
	}

	packet->priority = static_cast<int>(priority);

	packet->dts = ts_mfx_to_obs(pBS->DecodeTimeStamp, voi);

	*received_packet = true;
	pBS->DataLength = 0;
}

static void parse_packet_vp9(obs_qsv *obsqsv, encoder_packet *packet,
			     mfxBitstream *pBS, const video_output_info *voi,
			     bool *received_packet)
{
	if (pBS == NULL || pBS->DataLength == 0) {
		*received_packet = false;
		return;
	}

	da_resize(obsqsv->packet_data, 0);
	da_push_back_array(obsqsv->packet_data, &pBS->Data[pBS->DataOffset],
			   pBS->DataLength);

	packet->data = obsqsv->packet_data.array;
	packet->size = obsqsv->packet_data.num;
	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = ts_mfx_to_obs((mfxI64)pBS->TimeStamp, voi);
	packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_I);

	uint16_t frameType = pBS->FrameType;
	uint8_t priority;
	if ((frameType & MFX_FRAMETYPE_IDR) || (frameType & MFX_FRAMETYPE_I)) {
		priority = OBS_NAL_PRIORITY_HIGHEST;
	} else if ((frameType & MFX_FRAMETYPE_REF)) {
		priority = OBS_NAL_PRIORITY_HIGH;
	} else if ((frameType & MFX_FRAMETYPE_P)) {
		priority = OBS_NAL_PRIORITY_LOW;
	} else {
		priority = OBS_NAL_PRIORITY_DISPOSABLE;
	}

	packet->priority = priority;

	packet->dts = packet->pts;

	*received_packet = true;
	pBS->DataLength = 0;
}

static bool obs_qsv_encode(void *data, encoder_frame *frame,
			   encoder_packet *packet, bool *received_packet)
{
	obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

	if (!frame || !packet || !received_packet) {
		return false;
	}
	pthread_mutex_lock(&g_QsvLock);

	video_t *video = obs_encoder_video(obsqsv->encoder);
	const video_output_info *voi = video_output_get_info(video);

	mfxBitstream *pBS = nullptr;

	int ret = 0;

	mfxU64 qsvPTS = ts_obs_to_mfx(frame->pts, voi);

	// FIXME: remove null check from the top of this function
	// if we actually do expect null frames to complete output.
	if (frame) {
		ret = qsv_encoder_encode(obsqsv->context, qsvPTS,
					 frame->data[0], frame->data[1],
					 frame->linesize[0], frame->linesize[1],
					 &pBS);
	} else {

		ret = qsv_encoder_encode(obsqsv->context, qsvPTS, NULL, NULL, 0,
					 0, &pBS);
	}
	if (ret < 0) {

		warn("encode failed");
		pthread_mutex_unlock(&g_QsvLock);
		return false;
	}

	if (obsqsv->codec == QSV_CODEC_AVC) {
		parse_packet_h264(obsqsv, packet, pBS, voi, received_packet);
	} else if (obsqsv->codec == QSV_CODEC_AV1) {
		parse_packet_av1(obsqsv, packet, pBS, voi, received_packet);
	} else if (obsqsv->codec == QSV_CODEC_HEVC) {
		parse_packet_hevc(obsqsv, packet, pBS, voi, received_packet);
	} else if (obsqsv->codec == QSV_CODEC_VP9) {
		parse_packet_vp9(obsqsv, packet, pBS, voi, received_packet);
	}

	pthread_mutex_unlock(&g_QsvLock);

	return true;
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
					 .caps = OBS_ENCODER_CAP_DYN_BITRATE};

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
	.get_video_info = obs_qsv_video_plus_hdr_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE};

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
	.get_video_info = obs_qsv_video_plus_hdr_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE};

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
	.get_video_info = obs_qsv_video_plus_hdr_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE};
