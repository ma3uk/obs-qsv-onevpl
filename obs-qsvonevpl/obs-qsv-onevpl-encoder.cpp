// #define MFXDEPRECATED_OFF

#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif

mfxVersion VPLVersion = {{0, 1}}; // for backward compatibility
std::atomic<bool> IsActive{false};

void GetEncoderVersion(unsigned short *Major, unsigned short *Minor) {
  *Major = VPLVersion.Major;
  *Minor = VPLVersion.Minor;
}

bool OpenEncoder(std::unique_ptr<QSVEncoder> &EncoderPTR,
                 encoder_params *EncoderParams, enum codec_enum Codec,
                 bool IsTextureEncoder) {
  try {
    EncoderPTR = std::make_unique<QSVEncoder>();
    // throw std::exception("test");
    if (EncoderParams->GPUNum == 0) {
      obs_video_info OVI;
      obs_get_video_info(&OVI);
      mfxU32 AdapterID = OVI.adapter;
      mfxU32 AdapterIDAdjustment = 0;
      // Select current adapter - will be iGPU if exiStatus due to adapter
      // reordering
      if (Codec == QSV_CODEC_AV1 && !AdaptersInfo[AdapterID].SupportAV1) {
        for (mfxU32 i = 0; i < MAX_ADAPTERS; i++) {
          if (!AdaptersInfo[i].IsIntel) {
            AdapterIDAdjustment++;
            continue;
          }
          if (AdaptersInfo[i].SupportAV1) {
            AdapterID = i;
            break;
          }
        }
      } else if (!AdaptersInfo[AdapterID].IsIntel) {
        for (mfxU32 i = 0; i < MAX_ADAPTERS; i++) {
          if (AdaptersInfo[i].IsIntel) {
            AdapterID = i;
            break;
          }
          AdapterIDAdjustment++;
        }
      }

      AdapterID -= AdapterIDAdjustment;

      EncoderParams->GPUNum = AdapterID;
    }

    if (EncoderParams->GPUNum > 0) {
      IsTextureEncoder = false;
    }

    EncoderPTR->GetVPLVersion(VPLVersion);
    EncoderPTR->Init(EncoderParams, Codec, IsTextureEncoder);

    IsActive.store(true);

    return true;

  } catch (const std::exception &e) {
    error("QSV ERROR: %s", e.what());
    IsActive.store(false);
    throw;
  }
}

void DestroyPluginContext(void *Data) {
  plugin_context *Context = static_cast<plugin_context *>(Data);

  if (Context) {
    os_end_high_performance(Context->PerformanceToken);
    if (Context->EncoderPTR) {
      try {

        {
          std::lock_guard<std::mutex> lock(Mutex);

          Context->EncoderPTR->ClearData();

          IsActive.store(false);
          Context->EncoderPTR = nullptr;
        }

        Context->SEI.first = nullptr;
        Context->SEI.second = 0;
        Context->ExtraData.first = nullptr;
        Context->ExtraData.second = 0;
      } catch (const std::exception &e) {
        error("QSV ERROR: %s", e.what());
      }
    }

    delete Context;
    // bfree(Context);
  }
}

bool UpdateEncoderParams(void *Data, obs_data_t *Params) {
  plugin_context *Context = static_cast<plugin_context *>(Data);
  const char *bitrate_control = obs_data_get_string(Params, "rate_control");
  if (std::strcmp(bitrate_control, "CBR") == 0) {
    Context->EncoderParams.TargetBitRate =
        static_cast<mfxU16>(obs_data_get_int(Params, "bitrate"));
  } else if (std::strcmp(bitrate_control, "VBR") == 0) {
    Context->EncoderParams.TargetBitRate =
        static_cast<mfxU16>(obs_data_get_int(Params, "bitrate"));
    Context->EncoderParams.MaxBitRate =
        static_cast<mfxU16>(obs_data_get_int(Params, "max_bitrate"));
  } else if (std::strcmp(bitrate_control, "CQP") == 0) {
    Context->EncoderParams.QPI =
        static_cast<mfxU16>(obs_data_get_int(Params, "cqp"));
    Context->EncoderParams.QPP =
        static_cast<mfxU16>(obs_data_get_int(Params, "cqp"));
    Context->EncoderParams.QPB =
        static_cast<mfxU16>(obs_data_get_int(Params, "cqp"));
  } else if (std::strcmp(bitrate_control, "ICQ") == 0) {
    Context->EncoderParams.ICQQuality =
        static_cast<mfxU16>(obs_data_get_int(Params, "icq_quality"));
  }

  if (Context->EncoderPTR->UpdateParams(&Context->EncoderParams)) {
    mfxStatus Status = Context->EncoderPTR->ReconfigureEncoder();

    if (Status < MFX_ERR_NONE) {
      warn("Failed to reconfigure \nReset status: %d", Status);
      return false;
    }
  }

  return true;
}

static int qsv_encoder_reconfig(QSVEncoder *EncoderPTR,
                                encoder_params *EncoderParams) {

  EncoderPTR->UpdateParams(EncoderParams);

  if (EncoderPTR->ReconfigureEncoder() < MFX_ERR_NONE) {

    return false;
  }
  return true;
}

bool GetExtraData(void *Data, uint8_t **ExtraData, size_t *Size) {
  plugin_context *Context = static_cast<plugin_context *>(Data);

  if (!Context->EncoderPTR)
    return false;

  *ExtraData = Context->ExtraData.first;
  *Size = Context->ExtraData.second;
  return true;
}

bool GetSEIData(void *Data, uint8_t **SEI, size_t *Size) {
  plugin_context *Context = static_cast<plugin_context *>(Data);

  if (!Context->EncoderPTR)
    return false;

  *SEI = Context->SEI.first;
  *Size = Context->SEI.second;
  return true;
}

void GetVideoInfo(void *Data, video_scale_info *Info) {
  plugin_context *Context = static_cast<plugin_context *>(Data);
  auto pref_format =
      obs_encoder_get_preferred_video_format(Context->EncoderData);
  if (Context->Codec == QSV_CODEC_AVC) {
    if (!(pref_format == VIDEO_FORMAT_NV12)) {
      pref_format = (Info->format == VIDEO_FORMAT_NV12) ? Info->format
                                                        : VIDEO_FORMAT_NV12;
    }
  } else {
    if (!(pref_format == VIDEO_FORMAT_NV12 ||
          pref_format == VIDEO_FORMAT_P010)) {
      pref_format = (Info->format == VIDEO_FORMAT_NV12 ||
                     Info->format == VIDEO_FORMAT_P010)
                        ? Info->format
                        : VIDEO_FORMAT_NV12;
    }
  }
  Info->format = pref_format;
}

mfxU64 ConvertTSOBSMFX(int64_t TS, const video_output_info *VOI) {
  return static_cast<mfxU64>(
      static_cast<float>(TS * 90000 / VOI->fps_num));
}

int64_t ConvertTSMFXOBS(mfxI64 TS, const video_output_info *VOI) {
  int64_t div = 90000 * static_cast<int64_t>(VOI->fps_den);

  if (TS < 0) {
    return (TS * VOI->fps_num - div / 2) / div * VOI->fps_den;
  }
  else {
    return (TS * VOI->fps_num + div / 2) / div * VOI->fps_den;
  }
}

void ParseEncodedPacket(plugin_context *Context, encoder_packet *Packet,
                        mfxBitstream *Bitstream, const video_output_info *VOI,
                        bool *ReceivedPacketStatus) {
  if (Bitstream == nullptr || Bitstream->DataLength == 0) {
    *ReceivedPacketStatus = false;
    return;
  }

  Context->PacketData.resize(0);

  if (!Context->ExtraData.first || Context->ExtraData.second == 0) {
    uint8_t *NewPacket = 0;
    size_t NewPacketSize = 0;
    if (Context->Codec == QSV_CODEC_AVC) {
      obs_extract_avc_headers(*(&Bitstream->Data + *(&Bitstream->DataOffset)),
                              *(&Bitstream->DataLength), &NewPacket,
                              &NewPacketSize, &Context->ExtraData.first,
                              &Context->ExtraData.second, &Context->SEI.first,
                              &Context->SEI.second);
    } else if (Context->Codec == QSV_CODEC_HEVC) {
      obs_extract_hevc_headers(*(&Bitstream->Data + *(&Bitstream->DataOffset)),
                               *(&Bitstream->DataLength), &NewPacket,
                               &NewPacketSize, &Context->ExtraData.first,
                               &Context->ExtraData.second, &Context->SEI.first,
                               &Context->SEI.second);
    } else if (Context->Codec == QSV_CODEC_AV1) {
      obs_extract_av1_headers(*(&Bitstream->Data + *(&Bitstream->DataOffset)),
                              *(&Bitstream->DataLength), &NewPacket,
                              &NewPacketSize, &Context->ExtraData.first,
                              &Context->ExtraData.second);
    }

    Context->PacketData.insert(Context->PacketData.end(), NewPacket,
                               NewPacket + NewPacketSize);
  } else {
    Context->PacketData.insert(Context->PacketData.end(),
                               *(&Bitstream->Data + *(&Bitstream->DataOffset)),
                               *(&Bitstream->Data + *(&Bitstream->DataOffset)) +
                                   *(&Bitstream->DataLength));
  }

  Packet->data = Context->PacketData.data();
  Packet->size = Context->PacketData.size();

  Packet->type = OBS_ENCODER_VIDEO;
  Packet->pts = std::move(
      ConvertTSMFXOBS(static_cast<mfxI64>(Bitstream->TimeStamp), VOI));
  Packet->dts =
      (Context->Codec == QSV_CODEC_AV1)
          ? std::move(Packet->pts)
          : std::move(ConvertTSMFXOBS(Bitstream->DecodeTimeStamp, VOI));
  Packet->keyframe = ((Bitstream->FrameType & MFX_FRAMETYPE_I) ||
                      (Bitstream->FrameType & MFX_FRAMETYPE_IDR) ||
                      (Bitstream->FrameType & MFX_FRAMETYPE_S) ||
                      (Bitstream->FrameType & MFX_FRAMETYPE_xI) ||
                      (Bitstream->FrameType & MFX_FRAMETYPE_xIDR) ||
                      (Bitstream->FrameType & MFX_FRAMETYPE_xS));

  if ((Bitstream->FrameType & MFX_FRAMETYPE_I) ||
      (Bitstream->FrameType & MFX_FRAMETYPE_IDR) ||
      (Bitstream->FrameType & MFX_FRAMETYPE_S) ||
      (Bitstream->FrameType & MFX_FRAMETYPE_xI) ||
      (Bitstream->FrameType & MFX_FRAMETYPE_xIDR) ||
      (Bitstream->FrameType & MFX_FRAMETYPE_xS)) {
    Packet->priority = static_cast<int>(OBS_NAL_PRIORITY_HIGHEST);
    Packet->drop_priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
  } else if ((Bitstream->FrameType & MFX_FRAMETYPE_REF) ||
             (Bitstream->FrameType & MFX_FRAMETYPE_xREF)) {
    Packet->priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
    Packet->drop_priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
  } else if ((Bitstream->FrameType & MFX_FRAMETYPE_P) ||
             (Bitstream->FrameType & MFX_FRAMETYPE_xP)) {
    Packet->priority = static_cast<int>(OBS_NAL_PRIORITY_LOW);
    Packet->drop_priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
  } else {
    Packet->priority = static_cast<int>(OBS_NAL_PRIORITY_DISPOSABLE);
    Packet->drop_priority = static_cast<int>(OBS_NAL_PRIORITY_HIGH);
  }

  *ReceivedPacketStatus = true;

  *Bitstream->Data = 0;
  Bitstream->DataLength = 0;
  Bitstream->DataOffset = 0;
}

bool EncodeTexture(void *Data, encoder_texture *Texture, int64_t PTS,
                   uint64_t LockKey, uint64_t *NextKey, encoder_packet *Packet,
                   bool *ReceivedPacketStatus) {
  plugin_context *Context = static_cast<plugin_context *>(Data);

#if defined(_WIN32) || defined(_WIN64)
  if (!Texture || Texture->handle == static_cast<uint32_t>(-1)) {
#else
  if (!Texture || !Texture->tex[0] || !Texture->tex[1]) {
#endif
    warn("Encode failed: bad texture handle");
    *NextKey = LockKey;
    return false;
  }

  if (!Packet || !ReceivedPacketStatus)
    return false;

  {
    std::lock_guard<std::mutex> lock(Mutex);

    auto *Video = std::move(obs_encoder_video(Context->EncoderData));
    auto *VOI = std::move(video_output_get_info(std::move(Video)));

    auto *Bitstream = static_cast<mfxBitstream *>(nullptr);

    // if (obs_encoder_has_roi(obsqsv->encoder))
    //   obs_qsv_setup_rois(obsqsv);

    try {
      Context->EncoderPTR->EncodeTexture(
          std::move(ConvertTSOBSMFX(PTS, VOI)),
          std::move(static_cast<void *>(Texture)), LockKey, NextKey,
          &Bitstream);
    } catch (const std::exception &e) {
      error("%s", e.what());
      error("encode failed");

      return false;
    }

    ParseEncodedPacket(Context, Packet, std::move(Bitstream), VOI,
                       ReceivedPacketStatus);
  }

  return true;
}

bool EncodeFrame(void *Data, encoder_frame *Frame, encoder_packet *Packet,
                 bool *ReceivedPacketStatus) {

  plugin_context *Context = static_cast<plugin_context *>(Data);

  if (!Frame || !Packet || !ReceivedPacketStatus) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(Mutex);
    auto *video = std::move(obs_encoder_video(Context->EncoderData));
    auto *VOI = std::move(video_output_get_info(std::move(video)));

    auto *Bitstream = static_cast<mfxBitstream *>(nullptr);

    // if (obs_encoder_has_roi(obsqsv->encoder))
    //   obs_qsv_setup_rois(obsqsv);

    try {
      if (Frame->data[0]) {
        Context->EncoderPTR->EncodeFrame(
            std::move(ConvertTSOBSMFX(Frame->pts, VOI)), std::move(Frame->data),
            std::move(Frame->linesize), &Bitstream);
      } else {
        Context->EncoderPTR->EncodeFrame(
            std::move(ConvertTSOBSMFX(Frame->pts, VOI)), nullptr, 0,
            &Bitstream);
      }
    } catch (const std::exception &e) {
      error("%s", e.what());
      error("encode failed");

      return false;
    }

    ParseEncodedPacket(Context, Packet, std::move(Bitstream), VOI,
                       ReceivedPacketStatus);
  }

  return true;
}
