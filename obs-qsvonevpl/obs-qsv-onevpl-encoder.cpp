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
// #define MFX_DEPRECATED_OFF

#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif

#include "helpers/common_utils.hpp"
#include "helpers/qsv_params.hpp"
#include "obs-qsv-onevpl-encoder-internal.hpp"
#include <atomic>
#include <cstdint>
#include <media-io/video-io.h>
#include <new>
#include <obs.h>
#include <util/base.h>
#include <vpl/mfxcommon.h>
#include <vpl/mfxdefs.h>
#include <vpl/mfxstructures.h>

#define do_log(level, format, ...)                                             \
  blog(level, "[qsv encoder: '%s'] " format, "msdk_impl", ##__VA_ARGS__);

mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;
mfxVersion ver = {{0, 1}}; // for backward compatibility
std::atomic<bool> is_active{false};
bool isDGPU = false;
void qsv_encoder_version(unsigned short *major, unsigned short *minor) {
  *major = ver.Major;
  *minor = ver.Minor;
}

qsv_t *qsv_encoder_open(qsv_param_t *pParams, enum qsv_codec codec) {
  obs_video_info ovi;
  obs_get_video_info(&ovi);
  auto adapter_idx = ovi.adapter;

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

#define WARN_ERR_IMPL(err, str, err_name)                                      \
  case err:                                                                    \
    do_log(LOG_WARNING, str " (" err_name ")");                                \
    break;
#define WARN_ERR(err, str) WARN_ERR_IMPL(err, str, #err)

    switch (sts) {
      WARN_ERR(MFX_ERR_UNKNOWN, "Unknown QSV error");
      WARN_ERR(MFX_ERR_NOT_INITIALIZED,
               "Member functions called without initialization");
      WARN_ERR(MFX_ERR_INVALID_HANDLE, "Invalid session or MemId handle");
      WARN_ERR(MFX_ERR_NULL_PTR,
               "NULL pointer in the input or output arguments");
      WARN_ERR(MFX_ERR_UNDEFINED_BEHAVIOR, "Undefined behavior");
      WARN_ERR(MFX_ERR_NOT_ENOUGH_BUFFER,
               "Insufficient buffer for input or output.");
      WARN_ERR(MFX_ERR_NOT_FOUND,
               "Specified object/item/sync point not found.");
      WARN_ERR(MFX_ERR_MEMORY_ALLOC, "Gailed to allocate memory");
      WARN_ERR(MFX_ERR_LOCK_MEMORY, "failed to lock the memory block "
                                    "(external allocator).");
      WARN_ERR(MFX_ERR_UNSUPPORTED,
               "Unsupported configurations, parameters, or features");
      WARN_ERR(MFX_ERR_INVALID_VIDEO_PARAM,
               "Incompatible video parameters detected");
      WARN_ERR(MFX_WRN_VIDEO_PARAM_CHANGED,
               "The decoder detected a new sequence header in the "
               "bitstream. Video parameters may have changed.");
      WARN_ERR(MFX_WRN_VALUE_NOT_CHANGED,
               "The parameter has been clipped to its value range");
      WARN_ERR(MFX_WRN_OUT_OF_RANGE,
               "The parameter is out of valid value range");
      WARN_ERR(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
               "Incompatible video parameters detected");
      WARN_ERR(MFX_WRN_FILTER_SKIPPED,
               "The SDK VPP has skipped one or more optional filters "
               "requested by the application");
      WARN_ERR(MFX_ERR_ABORTED, "The asynchronous operation aborted");
      WARN_ERR(MFX_ERR_MORE_DATA,
               "Need more bitstream at decoding input, encoding "
               "input, or video processing input frames");
      WARN_ERR(MFX_ERR_MORE_SURFACE, "Need more frame surfaces at "
                                     "decoding or video processing output");
      WARN_ERR(MFX_ERR_MORE_BITSTREAM,
               "Need more bitstream buffers at the encoding output");
      WARN_ERR(MFX_WRN_IN_EXECUTION, "Synchronous operation still running");
      WARN_ERR(MFX_ERR_DEVICE_FAILED,
               "Hardware device returned unexpected errors");
      WARN_ERR(MFX_ERR_DEVICE_LOST, "Hardware device was lost");
      WARN_ERR(MFX_WRN_DEVICE_BUSY, "Hardware device is currently busy");
      WARN_ERR(MFX_WRN_PARTIAL_ACCELERATION,
               "The hardware does not support the specified "
               "configuration. Encoding, decoding, or video "
               "processing may be partially accelerated");
    }

#undef WARN_ERR
#undef WARN_ERR_IMPL
    if (pEncoder) {
      pEncoder = nullptr;
      delete pEncoder;
      is_active.store(false);
    }
    return nullptr;
  }
  is_active.store(true);
  return (qsv_t *)pEncoder;
}

// bool qsv_encoder_is_dgpu(qsv_t *pContext) {
//   QSV_VPL_Encoder_Internal *pEncoder =
//   reinterpret_cast<QSV_VPL_Encoder_Internal *>(pContext); return
//   pEncoder->IsDGPU();
// }

void obs_qsv_destroy(void *data) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

  if (obsqsv) {
    os_end_high_performance(obsqsv->performance_token);
    if (obsqsv->context) {
      pthread_mutex_lock(&g_QsvLock);
      QSV_VPL_Encoder_Internal *pEncoder =
          reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);
      pEncoder->ClearData();
      delete pEncoder;
      is_active.store(false);
      obsqsv->context = nullptr;
      pthread_mutex_unlock(&g_QsvLock);
      bfree(obsqsv->sei);
      bfree(obsqsv->extra_data);
      obsqsv->sei = nullptr;
      obsqsv->sei_size = 0;
      obsqsv->extra_data = nullptr;
      obsqsv->extra_data_size = 0;
    }
    da_free(obsqsv->packet_data);

    bfree(obsqsv);
  }
}

bool obs_qsv_update(void *data, obs_data_t *settings) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);
  const char *bitrate_control = obs_data_get_string(settings, "rate_control");
  if (astrcmpi(bitrate_control, "CBR") == 0) {
    obsqsv->params.nTargetBitRate =
        static_cast<mfxU16>(obs_data_get_int(settings, "bitrate"));
  } else if (astrcmpi(bitrate_control, "VBR") == 0) {
    obsqsv->params.nTargetBitRate =
        static_cast<mfxU16>(obs_data_get_int(settings, "bitrate"));
    obsqsv->params.nMaxBitRate =
        static_cast<mfxU16>(obs_data_get_int(settings, "max_bitrate"));
  } else if (astrcmpi(bitrate_control, "CQP") == 0) {
    obsqsv->params.nQPI =
        static_cast<mfxU16>(obs_data_get_int(settings, "cqp"));
    obsqsv->params.nQPP =
        static_cast<mfxU16>(obs_data_get_int(settings, "cqp"));
    obsqsv->params.nQPB =
        static_cast<mfxU16>(obs_data_get_int(settings, "cqp"));
  } else if (astrcmpi(bitrate_control, "ICQ") == 0) {
    obsqsv->params.nICQQuality =
        static_cast<mfxU16>(obs_data_get_int(settings, "icq_quality"));
  }

  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);
  pEncoder->UpdateParams(&obsqsv->params);
  mfxStatus sts = pEncoder->ReconfigureEncoder();

  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Failed to reconfigure \nReset status: %d", sts);
    return false;
  }

  return true;
}

int qsv_encoder_reconfig(qsv_t *pContext, qsv_param_t *pParams) {
  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(pContext);
  pEncoder->UpdateParams(pParams);
  mfxStatus sts = pEncoder->ReconfigureEncoder();

  if (sts < MFX_ERR_NONE) {
    
    return false;
  }
  return true;
}

enum video_format qsv_encoder_get_video_format(qsv_t *pContext) {
  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(pContext);

  mfxU32 fourCC;
  mfxStatus sts = pEncoder->GetCurrentFourCC(fourCC);
  if (sts != MFX_ERR_NONE)
    return VIDEO_FORMAT_NONE;

  switch (fourCC) {
  case MFX_FOURCC_NV12:
    return VIDEO_FORMAT_NV12;
  case MFX_FOURCC_BGR4:
    return VIDEO_FORMAT_RGBA;
  case MFX_FOURCC_RGB4:
    return VIDEO_FORMAT_RGBA;
  case MFX_FOURCC_P010:
    return VIDEO_FORMAT_P010;
  }

  return VIDEO_FORMAT_NONE;
}

void load_headers(obs_qsv *obsqsv) {

  DARRAY(uint8_t) header;
  DARRAY(uint8_t) sei;

  da_clear(header);
  da_clear(sei);
  da_init(header);
  da_init(sei);

  uint8_t *pVPS, *pSPS, *pPPS;
  uint16_t nVPS, nSPS, nPPS;
  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);
  pEncoder->GetVPSSPSPPS(&pVPS, &pSPS, &pPPS, &nVPS, &nSPS, &nPPS);

  if (obsqsv->codec == QSV_CODEC_HEVC) {
    da_push_back_array(header, pVPS, nVPS);
    da_push_back_array(header, pSPS, nSPS);
    da_push_back_array(header, pPPS, nPPS);
  } else if (obsqsv->codec == QSV_CODEC_AVC) {
    da_push_back_array(header, pSPS, nSPS);
    da_push_back_array(header, pPPS, nPPS);
  } else if (obsqsv->codec == QSV_CODEC_VP9 || obsqsv->codec == QSV_CODEC_AV1) {
    da_push_back_array(header, pSPS, nSPS);
  }
  obsqsv->extra_data = header.array;
  obsqsv->extra_data_size = header.num;
  obsqsv->sei = sei.array;
  obsqsv->sei_size = sei.num;
}

bool obs_qsv_extra_data(void *data, uint8_t **extra_data, size_t *size) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

  if (!obsqsv->context)
    return false;

  *extra_data = obsqsv->extra_data;
  *size = obsqsv->extra_data_size;
  return true;
}

bool obs_qsv_sei(void *data, uint8_t **sei, size_t *size) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

  if (!obsqsv->context)
    return false;

  *sei = obsqsv->sei;
  *size = obsqsv->sei_size;
  return true;
}

void obs_qsv_video_info(void *data, video_scale_info *info) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);
  auto pref_format = obs_encoder_get_preferred_video_format(obsqsv->encoder);

  if (!(pref_format == VIDEO_FORMAT_NV12 || pref_format == VIDEO_FORMAT_RGBA ||
        pref_format == VIDEO_FORMAT_BGRA)) {
    pref_format =
        (info->format == VIDEO_FORMAT_NV12 ||
         info->format == VIDEO_FORMAT_RGBA || info->format == VIDEO_FORMAT_BGRA)
            ? info->format
            : VIDEO_FORMAT_NV12;
  }

  info->format = pref_format;
}

void obs_qsv_video_plus_hdr_info(void *data, video_scale_info *info) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);
  auto pref_format = obs_encoder_get_preferred_video_format(obsqsv->encoder);

  if (!(pref_format == VIDEO_FORMAT_NV12 || pref_format == VIDEO_FORMAT_RGBA ||
        pref_format == VIDEO_FORMAT_P010 || pref_format == VIDEO_FORMAT_BGRA)) {
    pref_format =
        (info->format == VIDEO_FORMAT_NV12 ||
         info->format == VIDEO_FORMAT_RGBA ||
         info->format == VIDEO_FORMAT_P010 || info->format == VIDEO_FORMAT_BGRA)
            ? info->format
            : VIDEO_FORMAT_NV12;
  }

  info->format = pref_format;
}

mfxU64 ts_obs_to_mfx(int64_t ts, const video_output_info *voi) {
  return ts * 90000 / voi->fps_num;
}

int64_t ts_mfx_to_obs(mfxI64 ts, const video_output_info *voi) {
  return voi->fps_num * ts / 90000;
}

void parse_packet_h264(obs_qsv *obsqsv, encoder_packet *packet,
                       mfxBitstream *pBS, const video_output_info *voi,
                       bool *received_packet) {
  if (pBS == nullptr || pBS->DataLength == 0) {
    *received_packet = false;
    return;
  }

  da_resize(obsqsv->packet_data, 0);
  da_push_back_array(obsqsv->packet_data, *(&pBS->Data + *(&pBS->DataOffset)),
                     *(&pBS->DataLength));

  packet->data = obsqsv->packet_data.array;
  packet->size = obsqsv->packet_data.num;
  packet->type = OBS_ENCODER_VIDEO;
  packet->pts = ts_mfx_to_obs(static_cast<mfxI64>(pBS->TimeStamp), voi);
  packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_I) ||
                     (pBS->FrameType & MFX_FRAMETYPE_IDR);

  auto frameType = pBS->FrameType;
  auto priority = -1;
  if ((frameType & MFX_FRAMETYPE_I) || (frameType & MFX_FRAMETYPE_IDR)) {
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

  *pBS->Data = 0;
  pBS->DataLength = 0;
  pBS->DataOffset = 0;
  memset(&pBS->DataLength, 0, sizeof(pBS->DataLength));
  memset(&pBS->DataOffset, 0, sizeof(pBS->DataOffset));
}

void parse_packet_av1(obs_qsv *obsqsv, encoder_packet *packet,
                      mfxBitstream *pBS, const video_output_info *voi,
                      bool *received_packet) {
  if (pBS == nullptr || pBS->DataLength == 0) {
    *received_packet = false;
    return;
  }

  da_resize(obsqsv->packet_data, 0);
  da_push_back_array(obsqsv->packet_data, *(&pBS->Data + *(&pBS->DataOffset)),
                     *(&pBS->DataLength));

  packet->data = obsqsv->packet_data.array;
  packet->size = obsqsv->packet_data.num;
  packet->type = OBS_ENCODER_VIDEO;
  packet->pts = ts_mfx_to_obs(static_cast<mfxI64>(pBS->TimeStamp), voi);
  packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_I);

  auto frameType = pBS->FrameType;
  auto priority = -1;
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
  *pBS->Data = 0;
  pBS->DataLength = 0;
  pBS->DataOffset = 0;
  memset(&pBS->DataLength, 0, sizeof(pBS->DataLength));
  memset(&pBS->DataOffset, 0, sizeof(pBS->DataOffset));
}

void parse_packet_hevc(obs_qsv *obsqsv, encoder_packet *packet,
                       mfxBitstream *pBS, const video_output_info *voi,
                       bool *received_packet) {
  if (pBS == nullptr || pBS->DataLength == 0) {
    *received_packet = false;
    return;
  }

  da_resize(obsqsv->packet_data, 0);
  da_push_back_array(obsqsv->packet_data, *(&pBS->Data + *(&pBS->DataOffset)),
                     *(&pBS->DataLength));

  packet->data = obsqsv->packet_data.array;
  packet->size = obsqsv->packet_data.num;
  packet->type = OBS_ENCODER_VIDEO;
  packet->pts = ts_mfx_to_obs(static_cast<mfxI64>(pBS->TimeStamp), voi);
  packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_I) ||
                     (pBS->FrameType & MFX_FRAMETYPE_IDR);

  auto frameType = pBS->FrameType;
  auto priority = -1;
  if ((frameType & MFX_FRAMETYPE_I) || (frameType & MFX_FRAMETYPE_IDR)) {
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

  *pBS->Data = 0;
  pBS->DataLength = 0;
  pBS->DataOffset = 0;
  memset(&pBS->DataLength, 0, sizeof(pBS->DataLength));
  memset(&pBS->DataOffset, 0, sizeof(pBS->DataOffset));
}

void parse_packet_vp9(obs_qsv *obsqsv, encoder_packet *packet,
                      mfxBitstream *pBS, const video_output_info *voi,
                      bool *received_packet) {
  if (pBS == NULL || pBS->DataLength == 0) {
    *received_packet = false;
    return;
  }

  da_resize(obsqsv->packet_data, 0);
  da_push_back_array(obsqsv->packet_data, *(&pBS->Data + *(&pBS->DataOffset)),
                     *(&pBS->DataLength));

  packet->data = obsqsv->packet_data.array;
  packet->size = obsqsv->packet_data.num;
  packet->type = OBS_ENCODER_VIDEO;
  packet->pts = ts_mfx_to_obs(static_cast<mfxI64>(pBS->TimeStamp), voi);
  packet->keyframe = (pBS->FrameType & MFX_FRAMETYPE_I);

  auto frameType = pBS->FrameType;
  auto priority = -1;
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
  *pBS->Data = 0;
  pBS->DataLength = 0;
  pBS->DataOffset = 0;
  memset(&pBS->DataLength, 0, sizeof(pBS->DataLength));
  memset(&pBS->DataOffset, 0, sizeof(pBS->DataOffset));
}

bool obs_qsv_encode_tex(void *data, uint32_t handle, int64_t pts,
                        uint64_t lock_key, uint64_t *next_key,
                        struct encoder_packet *packet, bool *received_packet) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

  if (handle == GS_INVALID_HANDLE) {
    blog(LOG_WARNING, "Encode failed: bad texture handle");
    *next_key = lock_key;
    return false;
  }

  if (!packet || !received_packet)
    return false;

  pthread_mutex_lock(&g_QsvLock);

  auto *video = obs_encoder_video(obsqsv->encoder);
  auto *voi = video_output_get_info(video);

  auto *pBS = static_cast<mfxBitstream *>(nullptr);

  mfxU64 qsvPTS = ts_obs_to_mfx(pts, voi);

  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);

  mfxStatus sts =
      pEncoder->Encode_tex(qsvPTS, handle, lock_key, next_key, &pBS);

  if (sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) {
    blog(LOG_WARNING, "encode failed");
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

bool obs_qsv_encode(void *data, encoder_frame *frame, encoder_packet *packet,
                    bool *received_packet) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

  if (!frame || !packet || !received_packet) {
    return false;
  }

  pthread_mutex_lock(&g_QsvLock);

  auto *video = obs_encoder_video(obsqsv->encoder);
  auto *voi = video_output_get_info(video);

  auto ret = 0;

  auto *pBS = static_cast<mfxBitstream *>(nullptr);

  mfxU64 qsvPTS = ts_obs_to_mfx(frame->pts, voi);
  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);
  mfxStatus sts = MFX_ERR_NONE;
  if (frame) {
    sts = pEncoder->Encode(qsvPTS, frame->data[0], frame->data[1],
                           frame->linesize[0], frame->linesize[1], &pBS);
  } else {
    sts = pEncoder->Encode(qsvPTS, nullptr, nullptr, 0, 0, &pBS);
  }

  if (sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) {
    blog(LOG_WARNING, "encode failed");
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