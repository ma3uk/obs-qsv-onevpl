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

mfxVersion ver = {{0, 1}}; // for backward compatibility
std::atomic<bool> is_active{false};
//bool isDGPU = false;
void qsv_encoder_version(unsigned short *major, unsigned short *minor) {
  *major = ver.Major;
  *minor = ver.Minor;
}

qsv_t *qsv_encoder_open(qsv_param_t *pParams, enum qsv_codec codec,
                        bool useTexAlloc) {
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
  info("\tSelected adapter: %d", adapter_idx);

  mfxStatus sts;

  QSV_VPL_Encoder_Internal *pEncoder =
      new QSV_VPL_Encoder_Internal(ver, useTexAlloc);

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
    warn("Failed to reconfigure \nReset status: %d", sts);
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
  if (obsqsv->codec == QSV_CODEC_AVC) {
    if (!(pref_format == VIDEO_FORMAT_NV12)) {
      pref_format = (info->format == VIDEO_FORMAT_NV12) ? info->format
                                                        : VIDEO_FORMAT_NV12;
    }
  } else {
    if (!(pref_format == VIDEO_FORMAT_NV12 ||
          pref_format == VIDEO_FORMAT_P010)) {
      pref_format = (info->format == VIDEO_FORMAT_NV12 ||
                     info->format == VIDEO_FORMAT_P010)
                        ? info->format
                        : VIDEO_FORMAT_NV12;
    }
  }
  info->format = pref_format;
}

mfxU64 ts_obs_to_mfx(int64_t ts, const video_output_info *voi) {
  return ts * 90000 / voi->fps_num;
}

int64_t ts_mfx_to_obs(mfxI64 ts, const video_output_info *voi) {
  return voi->fps_num * ts / 90000;
}

void static parse_packet(obs_qsv *obsqsv, encoder_packet *packet,
                         mfxBitstream *pBS, const video_output_info *voi,
                         bool *received_packet) {
  if (pBS == nullptr || pBS->DataLength == 0) {
    *received_packet = false;
    return;
  }

  da_resize(obsqsv->packet_data, 0);
  da_push_back_array(obsqsv->packet_data, *(&pBS->Data + *(&pBS->DataOffset)),
                     std::move(*(&pBS->DataLength)));

  packet->data = std::move(obsqsv->packet_data.array);
  packet->size = std::move(obsqsv->packet_data.num);
  packet->type = OBS_ENCODER_VIDEO;
  packet->pts =
      std::move(ts_mfx_to_obs(static_cast<mfxI64>(pBS->TimeStamp), voi));
  packet->dts =
      (obsqsv->codec == QSV_CODEC_VP9 || obsqsv->codec == QSV_CODEC_AV1)
          ? std::move(packet->pts)
          : std::move(ts_mfx_to_obs(pBS->DecodeTimeStamp, voi));
  packet->keyframe = ((pBS->FrameType & MFX_FRAMETYPE_I) ||
                      (pBS->FrameType & MFX_FRAMETYPE_IDR) ||
                      (pBS->FrameType & MFX_FRAMETYPE_S) ||
                      (pBS->FrameType & MFX_FRAMETYPE_xI) ||
                      (pBS->FrameType & MFX_FRAMETYPE_xIDR) ||
                      (pBS->FrameType & MFX_FRAMETYPE_xS));

  if ((pBS->FrameType & MFX_FRAMETYPE_I) ||
      (pBS->FrameType & MFX_FRAMETYPE_IDR) ||
      (pBS->FrameType & MFX_FRAMETYPE_S) ||
      (pBS->FrameType & MFX_FRAMETYPE_xI) ||
      (pBS->FrameType & MFX_FRAMETYPE_xIDR) ||
      (pBS->FrameType & MFX_FRAMETYPE_xS)) {
    packet->priority = static_cast<int>(OBS_NAL_PRIORITY_HIGHEST);
  } else if ((pBS->FrameType & MFX_FRAMETYPE_REF) ||
             (pBS->FrameType & MFX_FRAMETYPE_xREF)) {
    packet->priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
  } else if ((pBS->FrameType & MFX_FRAMETYPE_P) ||
             (pBS->FrameType & MFX_FRAMETYPE_xP)) {
    packet->priority = static_cast<int>(OBS_NAL_PRIORITY_LOW);
  } else {
    packet->priority = static_cast<int>(OBS_NAL_PRIORITY_DISPOSABLE);
  }

  *received_packet = true;

  *pBS->Data = 0;
  pBS->DataLength = 0;
  pBS->DataOffset = 0;
}

// void qsv_encoder_add_roi(qsv_t *pContext, const obs_encoder_roi *roi) {
//   QSV_VPL_Encoder_Internal *pEncoder =
//       static_cast<QSV_VPL_Encoder_Internal *>(pContext);
//
//   /* QP value is range 0..51 */
//   // ToDo figure out if this is different for AV1
//   mfxI16 delta = static_cast<mfxI16>(-51.0f * roi->priority);
//   pEncoder->AddROI(roi->left, roi->top, roi->right, roi->bottom, delta);
// }
//
// void qsv_encoder_clear_roi(qsv_t *pContext) {
//   QSV_VPL_Encoder_Internal *pEncoder =
//       static_cast<QSV_VPL_Encoder_Internal *>(pContext);
//   pEncoder->ClearROI();
// }
//
// static void roi_cb(void *param, struct obs_encoder_roi *roi) {
//   struct darray *da = static_cast<darray *>(param);
//   darray_push_back(sizeof(struct obs_encoder_roi), da, roi);
// }
//
// static void obs_qsv_setup_rois(struct obs_qsv *obsqsv) {
//   const uint32_t increment = obs_encoder_get_roi_increment(obsqsv->encoder);
//   if (obsqsv->roi_increment == increment)
//     return;
//
//   qsv_encoder_clear_roi(obsqsv->context);
//   /* Because we pass-through the ROIs more or less directly we need to
//    * pass them in reverse order, so make a temporary copy and then use
//    * that instead. */
//   DARRAY(struct obs_encoder_roi) rois;
//   da_init(rois);
//
//   obs_encoder_enum_roi(obsqsv->encoder, roi_cb, &rois);
//
//   size_t idx = rois.num;
//   while (idx)
//     qsv_encoder_add_roi(obsqsv->context, &rois.array[--idx]);
//
//   da_free(rois);
//
//   obsqsv->roi_increment = increment;
// }

bool obs_qsv_encode_tex(void *data, struct encoder_texture *tex, int64_t pts,
                        uint64_t lock_key, uint64_t *next_key,
                        struct encoder_packet *packet, bool *received_packet) {
  obs_qsv *obsqsv = static_cast<obs_qsv *>(data);

#ifdef _WIN32
  if (!tex || tex->handle == static_cast<uint32_t>(-1)) {
#else
  if (!tex || !tex->tex[0] || !tex->tex[1]) {
#endif
    warn("Encode failed: bad texture handle");
    *next_key = lock_key;
    return false;
  }

  if (!packet || !received_packet)
    return false;

  pthread_mutex_lock(&g_QsvLock);

  auto *video = std::move(obs_encoder_video(obsqsv->encoder));
  auto *voi = std::move(video_output_get_info(video));

  auto *pBS = static_cast<mfxBitstream *>(nullptr);

  // if (obs_encoder_has_roi(obsqsv->encoder))
  //   obs_qsv_setup_rois(obsqsv);

  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);
  
  mfxStatus sts =
      pEncoder->Encode_tex(std::move(ts_obs_to_mfx(pts, voi)),
                           std::move(static_cast<void*>(tex)), lock_key, next_key, &pBS);

  if (sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) {
    warn("encode failed");
    pthread_mutex_unlock(&g_QsvLock);
    return false;
  }

  parse_packet(obsqsv, packet, std::move(pBS), voi, received_packet);

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

  auto *video = std::move(obs_encoder_video(obsqsv->encoder));
  auto *voi = std::move(video_output_get_info(video));

  auto *pBS = static_cast<mfxBitstream *>(nullptr);

  // mfxU64 qsvPTS = std::move(ts_obs_to_mfx(frame->pts, voi));

  // if (obs_encoder_has_roi(obsqsv->encoder))
  //   obs_qsv_setup_rois(obsqsv);

  QSV_VPL_Encoder_Internal *pEncoder =
      reinterpret_cast<QSV_VPL_Encoder_Internal *>(obsqsv->context);

  mfxStatus sts = MFX_ERR_NONE;

  if (frame->data[0]) {
    sts = pEncoder->Encode(std::move(ts_obs_to_mfx(frame->pts, voi)),
                           std::move(frame->data), std::move(frame->linesize),
                           &pBS);
  } else {
    sts = pEncoder->Encode(std::move(ts_obs_to_mfx(frame->pts, voi)), nullptr,
                           0, &pBS);
  }

  if (sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) {
    warn("encode failed");
    pthread_mutex_unlock(&g_QsvLock);
    return false;
  }

  parse_packet(obsqsv, packet, std::move(pBS), voi, received_packet);

  pthread_mutex_unlock(&g_QsvLock);

  return true;
}