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

struct qsv_t {};

enum qsv_codec { QSV_CODEC_AVC, QSV_CODEC_AV1, QSV_CODEC_HEVC, QSV_CODEC_VP9 };

int qsv_encoder_close(qsv_t *);
//int qsv_param_parse(qsv_param_t *, const char *name, const char *value);
//int qsv_param_apply_profile(qsv_param_t *, const char *profile);
//int qsv_param_default_preset(qsv_param_t *, const char *preset,
//                             const char *tune);
int qsv_encoder_reconfig(qsv_t *, qsv_param_t *);
void qsv_encoder_version(unsigned short *major, unsigned short *minor);
qsv_t *qsv_encoder_open(qsv_param_t *, enum qsv_codec codec);
bool qsv_encoder_is_dgpu(qsv_t *);
int qsv_encoder_encode(qsv_t *pContext, mfxU64 ts, uint8_t *pDataY,
                       uint8_t *pDataUV, uint32_t strideY, uint32_t strideUV,
                       mfxBitstream **pBS);
int qsv_encoder_encode_tex(qsv_t *pContext, mfxU64 ts, uint32_t tex_handle,
                           uint64_t lock_key, uint64_t *next_key,
                           mfxBitstream **pBS);
// enum video_format qsv_encoder_get_video_format(qsv_t *);
//bool prefer_igpu_enc(int *iGPUIndex);

int qsv_encoder_headers(qsv_t *pContext, uint8_t **pVPS, uint8_t **pSPS,
                        uint8_t **pPPS, uint16_t *pnVPS, uint16_t *pnSPS,
                        uint16_t *pnPPS);

// #ifdef __cplusplus
// }
// #endif
