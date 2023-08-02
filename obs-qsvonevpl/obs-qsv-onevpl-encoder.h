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

#pragma once
#include <Windows.h>
#include "mfxstructures.h"
#include "mfxadapter.h"
#include <stdint.h>

//#ifdef __cplusplus
//extern "C" {
//#endif

static const char *const qsv_ratecontrols_h264[] = {"CBR", "VBR", "CQP", "ICQ",
						    0};

static const char *const qsv_ratecontrols_vp9[] = {"CBR", "VBR",        "CQP",
						   "ICQ", "LA_EXT_ICQ", 0};

static const char *const qsv_ratecontrols_hevc[] = {"CBR", "VBR", "CQP", "ICQ",
						    0};

static const char *const qsv_ratecontrols_av1[] = {"CBR", "VBR",        "CQP",
						   "ICQ", "LA_EXT_ICQ", 0};

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
static const char *const qsv_params_condition_extbrc[] = {"IMPLICIT", "EXPLICIT", "OFF",
							  0};
//static const char *const qsv_params_condition_gop[] = {"CLOSED", "OPEN", 0};
static const char *const qsv_params_condition_intra_ref_encoding[] = {
	"VERTICAL", "HORIZONTAL", 0};
static const char *const qsv_params_condition_mv_cost_scaling[] = {
	"DEFAULT", "1/2", "1/4", "1/8", 0};
static const char *const qsv_params_condition_lookahead_latency[] = {
	"NORMAL", "HIGH", "LOW", "VERYLOW", 0};
static const char *const qsv_params_condition_lookahead_ds[] = {
	"SLOW", "MEDIUM", "FAST", "AUTO", 0};
static const char *const qsv_params_condition_trellis[] = {
	"OFF", "I", "IP", "IPB", "IB", "P", "PB", "B", "AUTO", 0};
static const char *const qsv_params_condition_hevc_sao[] = {
	"AUTO", "DISABLE", "LUMA", "CHROMA", "ALL", 0};
//static const char *const qsv_params_condition_hevc_ctu[] = {"AUTO", "16", "32",
//							    "64", 0};
//static const char *const qsv_params_condition_tune_quality[] = {
//	"DEFAULT", "PSNR", "SSIM", "MS SSIM", "VMAF", "PERCEPTUAL", "OFF", 0};
static const char *const qsv_params_condition_denoise_mode[] = {
	"DEFAULT",
	"AUTO | BDRATE | PRE ENCODE",
	"AUTO | ADJUST | POST ENCODE",
	"AUTO | SUBJECTIVE | PRE ENCODE",
	"MANUAL | PRE ENCODE",
	"MANUAL | POST ENCODE",
	"OFF",
	0};

struct qsv_t {
};

struct adapter_info {
	bool is_intel;
	bool is_dgpu;
	bool supports_av1;
	bool supports_hevc;
	bool supports_vp9;
};

enum qsv_codec { QSV_CODEC_AVC, QSV_CODEC_AV1, QSV_CODEC_HEVC, QSV_CODEC_VP9 };

#define MAX_ADAPTERS 10
extern struct adapter_info adapters[MAX_ADAPTERS];
extern size_t adapter_count;

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

	int bQualityEnchance;
	int bMBBRC;
	int bAdaptiveI;
	int bAdaptiveB;
	int bAdaptiveRef;
	int bAdaptiveCQM;
	int bAdaptiveLTR;
	int bAdaptiveMaxFrameSize;
	int bRDO;
	int bLowPower;
	int bRawRef;
	int bGPB;
	int bDirectBiasAdjustment;
	int bGopOptFlag;
	int bWeightedPred;
	int bWeightedBiPred;
	int bGlobalMotionBiasAdjustment;
	int bLAExtBRC;
	int bExtBRC;
	int bIntraRefEncoding;

	//bool bFadeDetection;
	bool video_fmt_10bit;
	bool bResetAllowed;
	bool bCustomBufferSize;
	bool bLookahead;
	bool bEncTools;

	int nIntraRefType;

	int nDeviceNum;
	int nTrellis;
	int nDenoiseMode;
	int nMVCostScalingFactor;
	int nLookAheadDS;
	//int nRepartitionCheckEnable;
	int nMotionVectorsOverPicBoundaries;
	int nTuneQualityMode;
	int nNumRefFrameLayers;
	int nSAO;
	int nHyperMode;

	mfxU32 nFourCC;
	mfxU16 nChromaFormat;
};

enum qsv_cpu_platform {
	QSV_CPU_PLATFORM_UNKNOWN,
	QSV_CPU_PLATFORM_BNL,
	QSV_CPU_PLATFORM_SNB,
	QSV_CPU_PLATFORM_IVB,
	QSV_CPU_PLATFORM_SLM,
	QSV_CPU_PLATFORM_CHT,
	QSV_CPU_PLATFORM_HSW,
	QSV_CPU_PLATFORM_BDW,
	QSV_CPU_PLATFORM_SKL,
	QSV_CPU_PLATFORM_APL,
	QSV_CPU_PLATFORM_KBL,
	QSV_CPU_PLATFORM_GLK,
	QSV_CPU_PLATFORM_CNL,
	QSV_CPU_PLATFORM_ICL,
	QSV_CPU_PLATFORM_TGL,
	QSV_CPU_PLATFORM_RKL,
	QSV_CPU_PLATFORM_ADL,
	QSV_CPU_PLATFORM_RPL,
	QSV_CPU_PLATFORM_INTEL
};

int qsv_encoder_close(qsv_t *);
int qsv_param_parse(qsv_param_t *, const char *name, const char *value);
int qsv_param_apply_profile(qsv_param_t *, const char *profile);
int qsv_param_default_preset(qsv_param_t *, const char *preset,
			     const char *tune);
int qsv_encoder_reconfig(qsv_t *, qsv_param_t *);
void qsv_encoder_version(unsigned short *major, unsigned short *minor);
qsv_t *qsv_encoder_open(qsv_param_t *, enum qsv_codec codec);
bool qsv_encoder_is_dgpu(qsv_t *);
int qsv_encoder_encode(qsv_t *, uint64_t, uint8_t *, uint8_t *, uint32_t,
		       uint32_t, mfxBitstream **pBS);
int qsv_encoder_encode_tex(qsv_t *, uint64_t, uint32_t, uint64_t, uint64_t *,
			   mfxBitstream **pBS);
int qsv_encoder_headers(qsv_t *, uint8_t **pSPS, uint8_t **pPPS,
			uint16_t *pnSPS, uint16_t *pnPPS);
enum qsv_cpu_platform qsv_get_cpu_platform();
//enum video_format qsv_encoder_get_video_format(qsv_t *);
bool prefer_igpu_enc(int *iGPUIndex);

int qsv_hevc_encoder_headers(qsv_t *pContext, uint8_t **vVPS, uint8_t **pSPS,
			     uint8_t **pPPS, uint16_t *pnVPS, uint16_t *pnSPS,
			     uint16_t *pnPPS);

//#ifdef __cplusplus
//}
//#endif
