#pragma once

#ifndef __QSV_VPL_ENCODER_H__
#define __QSV_VPL_ENCODER_H__
#endif

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif

#ifndef __QSVEncoder_H__
#include "obs-qsv-onevpl-encoder-internal.hpp"
#endif

#ifndef __QSV_VPL_PLUGIN_INIT_H__
#include "obs-qsv-onevpl-plugin-init.hpp"
#endif

static inline std::mutex Mutex;

static unsigned short VPLVersionMajor;
static unsigned short VPLVersionMinor;

void GetEncoderVersion(unsigned short *Major, unsigned short *Minor);
bool OpenEncoder(std::unique_ptr<class QSVEncoder> &EncoderPTR,
                 encoder_params *EncoderParams, enum codec_enum Codec,
                 bool IsTextureEncoder);
void DestroyPluginContext(void *);
bool UpdateEncoderParams(void *Data, obs_data_t *Params);

mfxU64 ConvertTSOBSMFX(int64_t TS, const video_output_info *VOI);

int64_t ConvertTSMFXOBS(mfxI64 TS, const video_output_info *VOI);

bool GetExtraData(void *Data, uint8_t **ExtraData, size_t *Size);

void ParseEncodedPacket(plugin_context *Context, encoder_packet *Packet,
                  mfxBitstream *Bitstream, const video_output_info *VOI,
                        bool *ReceivedPacketStatus);

bool EncodeTexture(void *Data, encoder_texture *Texture, int64_t PTS,
                        uint64_t LockKey, uint64_t *NextKey, encoder_packet *Packet,
                   bool *ReceivedPacketStatus);

bool EncodeFrame(void *Data, encoder_frame *Frame, encoder_packet *Packet,
                 bool *ReceivedPacketStatus);

bool GetSEIData(void *Data, uint8_t **SEI, size_t *Size);

void GetVideoInfo(void *Data, video_scale_info *Info);
