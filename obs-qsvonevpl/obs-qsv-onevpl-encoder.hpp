#pragma once

#ifndef __QSV_VPL_ENCODER_H__
#define __QSV_VPL_ENCODER_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include <obs-avc.h>
#include <obs-hevc.h>
// #include <obs-av1.h>
#include <obs-module.h>
#include <obs.h>

#include <util/darray.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/threading.h>

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif
#ifndef __QSV_VPL_ENCODER_INTERNAL_H__
#include "obs-qsv-onevpl-encoder-internal.hpp"
#endif

#ifndef __QSV_VPL_ENCODER_PARAMS_H__
#include "helpers/qsv_params.hpp"
#endif

struct qsv_context_t {};
typedef qsv_context_t qsv_t;
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

enum qsv_codec { QSV_CODEC_AVC, QSV_CODEC_AV1, QSV_CODEC_HEVC, QSV_CODEC_VP9 };

static inline pthread_mutex_t g_QsvLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned short g_verMajor;
static unsigned short g_verMinor;

// int qsv_param_parse(qsv_param_t *, const char *name, const char *value);
// int qsv_param_apply_profile(qsv_param_t *, const char *profile);
// int qsv_param_default_preset(qsv_param_t *, const char *preset,
//                              const char *tune);
void qsv_encoder_version(unsigned short *major, unsigned short *minor);
qsv_t *qsv_encoder_open(qsv_param_t *, enum qsv_codec codec);
bool qsv_encoder_is_dgpu(qsv_t *);
void obs_qsv_destroy(void *data);
bool obs_qsv_update(void *data, obs_data_t *settings);
// enum video_format qsv_encoder_get_video_format(qsv_t *);
// bool prefer_igpu_enc(int *iGPUIndex);

video_format qsv_encoder_get_video_format(qsv_t *pContext);

mfxU64 ts_obs_to_mfx(int64_t ts, const video_output_info *voi);

int64_t ts_mfx_to_obs(mfxI64 ts, const video_output_info *voi);

void load_headers(obs_qsv *obsqsv);

bool obs_qsv_extra_data(void *data, uint8_t **extra_data, size_t *size);

void parse_packet_h264(obs_qsv *obsqsv, encoder_packet *packet,
                       mfxBitstream *pBS, const video_output_info *voi,
                       bool *received_packet);

void parse_packet_av1(obs_qsv *obsqsv, encoder_packet *packet,
                      mfxBitstream *pBS, const video_output_info *voi,
                      bool *received_packet);

void parse_packet_hevc(obs_qsv *obsqsv, encoder_packet *packet,
                       mfxBitstream *pBS, const video_output_info *voi,
                       bool *received_packet);

void parse_packet_vp9(obs_qsv *obsqsv, encoder_packet *packet,
                      mfxBitstream *pBS, const video_output_info *voi,
                      bool *received_packet);

bool obs_qsv_encode_tex(void *data, uint32_t handle, int64_t pts,
                        uint64_t lock_key, uint64_t *next_key,
                        encoder_packet *packet, bool *received_packet);

bool obs_qsv_encode(void *data, encoder_frame *frame, encoder_packet *packet,
                    bool *received_packet);

bool obs_qsv_sei(void *data, uint8_t **sei, size_t *size);

void obs_qsv_video_info(void *data, video_scale_info *info);

void obs_qsv_video_plus_hdr_info(void *data, video_scale_info *info);
