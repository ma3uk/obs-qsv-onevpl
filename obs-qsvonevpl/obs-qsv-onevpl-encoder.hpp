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

  uint32_t roi_increment;
};

static inline pthread_mutex_t g_QsvLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned short g_verMajor;
static unsigned short g_verMinor;

void qsv_encoder_version(unsigned short *major, unsigned short *minor);
qsv_t *qsv_encoder_open(qsv_param_t *, enum qsv_codec codec);
bool qsv_encoder_is_dgpu(qsv_t *);
void obs_qsv_destroy(void *data);
bool obs_qsv_update(void *data, obs_data_t *settings);


video_format qsv_encoder_get_video_format(qsv_t *pContext);

mfxU64 ts_obs_to_mfx(int64_t ts, const video_output_info *voi);

int64_t ts_mfx_to_obs(mfxI64 ts, const video_output_info *voi);

void load_headers(obs_qsv *obsqsv);

bool obs_qsv_extra_data(void *data, uint8_t **extra_data, size_t *size);

void static parse_packet(obs_qsv *obsqsv, encoder_packet *packet,
                         mfxBitstream *pBS, const video_output_info *voi,
                         bool *received_packet);

void qsv_encoder_add_roi(qsv_t *, const struct obs_encoder_roi *roi);

void qsv_encoder_clear_roi(qsv_t *pContext);

static void roi_cb(void *param, obs_encoder_roi *roi);

static void obs_qsv_setup_rois(obs_qsv *obsqsv);

bool obs_qsv_encode_tex(void *data, uint32_t handle, int64_t pts,
                        uint64_t lock_key, uint64_t *next_key,
                        encoder_packet *packet, bool *received_packet);

bool obs_qsv_encode(void *data, encoder_frame *frame, encoder_packet *packet,
                    bool *received_packet);

bool obs_qsv_sei(void *data, uint8_t **sei, size_t *size);

void obs_qsv_video_info(void *data, video_scale_info *info);

void obs_qsv_video_plus_hdr_info(void *data, video_scale_info *info);