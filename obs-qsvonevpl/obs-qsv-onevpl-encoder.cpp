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

// QSV_Encoder.cpp : Defines the exported functions for the DLL application.
//
//#define MFX_DEPRECATED_OFF
#define ONEVPL_EXPERIMENTAL

#include "obs-qsv-onevpl-encoder.hpp"
#include "obs-qsv-onevpl-encoder-internal.hpp"
#include "helpers/common_utils.hpp"
#include <obs-module.h>

#include <string>
#include <atomic>

#define do_log(level, format, ...) \
	blog(level, "[qsv encoder: '%s'] " format, "msdk_impl", ##__VA_ARGS__);

mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;
mfxVersion ver = {{0, 1}}; // for backward compatibility
std::atomic<bool> is_active{false};
bool isDGPU = false;
void qsv_encoder_version(unsigned short *major, unsigned short *minor)
{
	*major = ver.Major;
	*minor = ver.Minor;
}

qsv_t *qsv_encoder_open(qsv_param_t *pParams, enum qsv_codec codec)
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);
	size_t adapter_idx = ovi.adapter;

	// Select current adapter - will be iGPU if exists due to adapter reordering
	if (codec == QSV_CODEC_AV1 && !adapters[adapter_idx].supports_av1) {
		for (size_t i = 0; i < 4; ++i) {
			if (adapters[i].supports_av1) {
				adapter_idx = i;
				break;
			}
		}
	} else if (!adapters[adapter_idx].is_intel) {
		for (size_t i = 0; i < 4; ++i) {
			if (adapters[i].is_intel) {
				adapter_idx = i;
				break;
			}
		}
	}
	blog(LOG_INFO, "Selected adapter: %d", adapter_idx);
	isDGPU = adapters[adapter_idx].is_dgpu;
	mfxStatus sts;

	QSV_VPL_Encoder_Internal *pEncoder =
		new QSV_VPL_Encoder_Internal(ver, isDGPU);

	sts = pEncoder->Open(pParams, codec);

	// Fall back to NV12 from ARGB, if needed
	if (sts == MFX_ERR_UNSUPPORTED && pParams->nFourCC == MFX_FOURCC_BGR4) {
		pParams->nFourCC = MFX_FOURCC_NV12;
		pParams->nChromaFormat = MFX_CHROMAFORMAT_YUV420;
		sts = pEncoder->Reset(pParams, codec);
	}

	if (sts != MFX_ERR_NONE) {

#define WARN_ERR_IMPL(err, str, err_name)                   \
	case err:                                           \
		do_log(LOG_WARNING, str " (" err_name ")"); \
		break;
#define WARN_ERR(err, str) WARN_ERR_IMPL(err, str, #err)

		switch (sts) {
			WARN_ERR(MFX_ERR_UNKNOWN, "Unknown QSV error");
			WARN_ERR(
				MFX_ERR_NOT_INITIALIZED,
				"Member functions called without initialization");
			WARN_ERR(MFX_ERR_INVALID_HANDLE,
				 "Invalid session or MemId handle");
			WARN_ERR(
				MFX_ERR_NULL_PTR,
				"NULL pointer in the input or output arguments");
			WARN_ERR(MFX_ERR_UNDEFINED_BEHAVIOR,
				 "Undefined behavior");
			WARN_ERR(MFX_ERR_NOT_ENOUGH_BUFFER,
				 "Insufficient buffer for input or output.");
			WARN_ERR(MFX_ERR_NOT_FOUND,
				 "Specified object/item/sync point not found.");
			WARN_ERR(MFX_ERR_MEMORY_ALLOC,
				 "Gailed to allocate memory");
			WARN_ERR(MFX_ERR_LOCK_MEMORY,
				 "failed to lock the memory block "
				 "(external allocator).");
			WARN_ERR(
				MFX_ERR_UNSUPPORTED,
				"Unsupported configurations, parameters, or features");
			WARN_ERR(MFX_ERR_INVALID_VIDEO_PARAM,
				 "Incompatible video parameters detected");
			WARN_ERR(
				MFX_WRN_VIDEO_PARAM_CHANGED,
				"The decoder detected a new sequence header in the "
				"bitstream. Video parameters may have changed.");
			WARN_ERR(
				MFX_WRN_VALUE_NOT_CHANGED,
				"The parameter has been clipped to its value range");
			WARN_ERR(MFX_WRN_OUT_OF_RANGE,
				 "The parameter is out of valid value range");
			WARN_ERR(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
				 "Incompatible video parameters detected");
			WARN_ERR(
				MFX_WRN_FILTER_SKIPPED,
				"The SDK VPP has skipped one or more optional filters "
				"requested by the application");
			WARN_ERR(MFX_ERR_ABORTED,
				 "The asynchronous operation aborted");
			WARN_ERR(
				MFX_ERR_MORE_DATA,
				"Need more bitstream at decoding input, encoding "
				"input, or video processing input frames");
			WARN_ERR(MFX_ERR_MORE_SURFACE,
				 "Need more frame surfaces at "
				 "decoding or video processing output");
			WARN_ERR(
				MFX_ERR_MORE_BITSTREAM,
				"Need more bitstream buffers at the encoding output");
			WARN_ERR(MFX_WRN_IN_EXECUTION,
				 "Synchronous operation still running");
			WARN_ERR(MFX_ERR_DEVICE_FAILED,
				 "Hardware device returned unexpected errors");
			WARN_ERR(MFX_ERR_DEVICE_LOST,
				 "Hardware device was lost");
			WARN_ERR(MFX_WRN_DEVICE_BUSY,
				 "Hardware device is currently busy");
			WARN_ERR(MFX_WRN_PARTIAL_ACCELERATION,
				 "The hardware does not support the specified "
				 "configuration. Encoding, decoding, or video "
				 "processing may be partially accelerated");
		}

#undef WARN_ERR
#undef WARN_ERR_IMPL
		if (pEncoder) {
			/*delete pEncoder;*/
			pEncoder = nullptr;
			is_active.store(false);
		}
		return NULL;
	}
	is_active.store(true);
	return (qsv_t *)pEncoder;
}

bool qsv_encoder_is_dgpu(qsv_t *pContext)
{
	QSV_VPL_Encoder_Internal *pEncoder =
		(QSV_VPL_Encoder_Internal *)pContext;
	return pEncoder->IsDGPU();
}

int qsv_encoder_encode(qsv_t *pContext, uint64_t ts, uint8_t *pDataY,
		       uint8_t *pDataUV, uint32_t strideY, uint32_t strideUV,
		       mfxBitstream **pBS)
{
	QSV_VPL_Encoder_Internal *pEncoder =
		(QSV_VPL_Encoder_Internal *)pContext;

	mfxStatus sts = MFX_ERR_NONE;

	if (pDataY != NULL)
		sts = pEncoder->Encode(ts, pDataY, pDataUV, strideY, strideUV,
				       pBS);

	if (sts == MFX_ERR_NONE) {
		return 0;
	} else if (sts == MFX_ERR_MORE_DATA) {
		return 1;
	} else {
		return -1;
	}
}

int qsv_encoder_close(qsv_t *pContext)
{
	QSV_VPL_Encoder_Internal *pEncoder =
		(QSV_VPL_Encoder_Internal *)pContext;
	pEncoder->ClearData();
	delete pEncoder;
	pEncoder = nullptr;
	if (pEncoder)
		is_active.store(false);

	return 0;
}

int qsv_encoder_reconfig(qsv_t *pContext, qsv_param_t *pParams)
{
	if (pParams->bResetAllowed == true) {
		QSV_VPL_Encoder_Internal *pEncoder =
			(QSV_VPL_Encoder_Internal *)pContext;
		if (pEncoder->UpdateParams(pParams)) {
			mfxStatus sts = pEncoder->ReconfigureEncoder();
			if (sts < MFX_ERR_NONE) {
				return false;
			}
		}
	}
	return true;
}

int qsv_encoder_headers(qsv_t *pContext, uint8_t **pVPS, uint8_t **pSPS,
			     uint8_t **pPPS, uint16_t *pnVPS, uint16_t *pnSPS,
			     uint16_t *pnPPS)
{
	QSV_VPL_Encoder_Internal *pEncoder =
		(QSV_VPL_Encoder_Internal *)pContext;
	pEncoder->GetVPSSPSPPS(pVPS, pSPS, pPPS, pnVPS, pnSPS, pnPPS);

	return 0;
}

enum video_format qsv_encoder_get_video_format(qsv_t *pContext)
{
	QSV_VPL_Encoder_Internal *pEncoder =
		(QSV_VPL_Encoder_Internal *)pContext;

	mfxU32 fourCC;
	mfxStatus sts = pEncoder->GetCurrentFourCC(fourCC);
	if (sts != MFX_ERR_NONE)
		return VIDEO_FORMAT_NONE;

	switch (fourCC) {
	case MFX_FOURCC_NV12:
		return VIDEO_FORMAT_NV12;
	case MFX_FOURCC_BGR4:
		return VIDEO_FORMAT_RGBA;
	}

	return VIDEO_FORMAT_NONE;
}
