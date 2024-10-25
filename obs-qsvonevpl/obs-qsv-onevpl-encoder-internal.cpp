#include "obs-qsv-onevpl-encoder-internal.hpp"

QSVEncoder::QSVEncoder()
    : QSVPlatform(), QSVVersion(), QSVLoader(), QSVLoaderConfig(),
      QSVLoaderVariant(), QSVSession(nullptr), QSVImpl(), QSVEncodeSurface(),
      QSVProcessingSurface(), QSVEncode(nullptr), QSVProcessing(nullptr),
      QSVVPSBuffer(), QSVSPSBuffer(), QSVPPSBuffer(), QSVVPSBufferSize(1024),
      QSVSPSBufferSize(1024), QSVPPSBufferSize(1024), QSVBitstream(),
      QSVTaskPool(), QSVSyncTaskID(), QSVResetParams(),
      QSVResetParamsChanged(false), QSVEncodeParams(), QSVEncodeCtrlParams(),
      QSVAllocateRequest(), QSVIsTextureEncoder(), QSVMemoryInterface(),
      HWManager(nullptr), QSVProcessingEnable() {}

QSVEncoder::~QSVEncoder() {
  if (QSVEncode || QSVProcessing) {
    ClearData();
  }
}

mfxStatus QSVEncoder::GetVPLVersion(mfxVersion &Version) {
  mfxStatus Status = MFX_ERR_NONE;
  QSVLoader = MFXLoad();
  if (QSVLoader == nullptr) {
    throw std::runtime_error("GetVPLSession(): MFXLoad error");
  }
  Status = MFXCreateSession(QSVLoader, 0, &QSVSession);
  if (Status >= MFX_ERR_NONE) {
    MFXQueryVersion(QSVSession, &QSVVersion);
    Version = QSVVersion;
    MFXClose(QSVSession);
    MFXUnload(QSVLoader);
  } else {
    throw std::runtime_error("GetVPLSession(): MFXCreateSession error");
  }

  return Status;
}

mfxStatus QSVEncoder::CreateSession([[maybe_unused]] enum codec_enum Codec,
                                    [[maybe_unused]] void **Data, int GPUNum) {
  mfxStatus Status = MFX_ERR_NONE;
  try {
    // Initialize Intel VPL Session
    QSVLoader = MFXLoad();
    if (QSVLoader == nullptr) {
      return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    QSVLoaderConfig[0] = MFXCreateConfig(QSVLoader);
    QSVLoaderVariant[0].Type = MFX_VARIANT_TYPE_U32;
    QSVLoaderVariant[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    MFXSetConfigFilterProperty(
        QSVLoaderConfig[0],
        reinterpret_cast<const mfxU8 *>("mfxImplDescription.Impl.mfxImplType"),
        QSVLoaderVariant[0]);

    QSVLoaderConfig[1] = MFXCreateConfig(QSVLoader);
    QSVLoaderVariant[1].Type = MFX_VARIANT_TYPE_U32;
    QSVLoaderVariant[1].Data.U32 = static_cast<mfxU32>(0x8086);
    MFXSetConfigFilterProperty(
        QSVLoaderConfig[1],
        reinterpret_cast<const mfxU8 *>("mfxImplDescription.VendorID"),
        QSVLoaderVariant[1]);

    QSVLoaderConfig[2] = MFXCreateConfig(QSVLoader);
    QSVLoaderVariant[2].Type = MFX_VARIANT_TYPE_PTR;
    QSVLoaderVariant[2].Data.Ptr = mfxHDL("mfx-gen");
    MFXSetConfigFilterProperty(
        QSVLoaderConfig[2],
        reinterpret_cast<const mfxU8 *>("mfxImplDescription.ImplName"),
        QSVLoaderVariant[2]);

    QSVLoaderConfig[3] = MFXCreateConfig(QSVLoader);
    QSVLoaderVariant[3].Type = MFX_VARIANT_TYPE_U32;
    QSVLoaderVariant[3].Data.U32 = MFX_PRIORITY_HIGH;
    MFXSetConfigFilterProperty(
        QSVLoaderConfig[3],
        reinterpret_cast<const mfxU8 *>(
            "mfxInitializationParam.mfxExtThreadsParam.Priority"),
        QSVLoaderVariant[3]);

    if (QSVIsTextureEncoder) {
#if defined(_WIN32) || defined(_WIN64)
      QSVLoaderConfig[4] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[4].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[4].Data.U32 = MFX_HANDLE_D3D11_DEVICE;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[4], reinterpret_cast<const mfxU8 *>("mfxHandleType"),
          QSVLoaderVariant[4]);

      QSVLoaderConfig[5] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[5].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[5].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
      MFXSetConfigFilterProperty(QSVLoaderConfig[5],
                                 reinterpret_cast<const mfxU8 *>(
                                     "mfxImplDescription.AccelerationMode"),
                                 QSVLoaderVariant[5]);

      QSVLoaderConfig[6] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[6].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[6].Data.U32 = MFX_SURFACE_TYPE_D3D11_TEX2D;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[6],
          reinterpret_cast<const mfxU8 *>(
              "mfxSurfaceTypesSupported.surftype.SurfaceType"),
          QSVLoaderVariant[6]);
#else
      QSVLoaderConfig[4] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[4].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[4].Data.U32 = MFX_HANDLE_VA_DISPLAY;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[4], reinterpret_cast<const mfxU8 *>("mfxHandleType"),
          QSVLoaderVariant[4]);

      QSVIsTextureEncoder = false;
      QSVLoaderConfig[5] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[5].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[5].Data.U32 = MFX_ACCEL_MODE_VIA_VAAPI;
      MFXSetConfigFilterProperty(QSVLoaderConfig[5],
                                 reinterpret_cast<const mfxU8 *>(
                                     "mfxImplDescription.AccelerationMode"),
                                 QSVLoaderVariant[5]);

      QSVLoaderConfig[6] = MFXCreateConfig(QSVLoader);
      QSVLoaderVariant[6].Type = MFX_VARIANT_TYPE_U32;
      QSVLoaderVariant[6].Data.U32 = MFX_SURFACE_TYPE_VAAPI;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[6],
          reinterpret_cast<const mfxU8 *>(
              "mfxSurfaceTypesSupported.surftype.SurfaceType"),
          QSVLoaderVariant[6]);
#endif
      QSVLoaderVariant[6].Data.U32 = MFX_SURFACE_COMPONENT_ENCODE;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[6],
          reinterpret_cast<const mfxU8 *>(
              "mfxSurfaceTypesSupported.surftype.surfcomp.SurfaceComponent"),
          QSVLoaderVariant[6]);

      QSVLoaderVariant[6].Data.U32 = MFX_SURFACE_FLAG_IMPORT_SHARED;
      MFXSetConfigFilterProperty(
          QSVLoaderConfig[6],
          reinterpret_cast<const mfxU8 *>(
              "mfxSurfaceTypesSupported.surftype.surfcomp.SurfaceFlags"),
          QSVLoaderVariant[6]);
    }

    QSVLoaderVariant[7].Type = MFX_VARIANT_TYPE_U32;
    QSVLoaderVariant[7].Data.U32 = (Codec == QSV_CODEC_AV1)    ? MFX_CODEC_AV1
                                   : (Codec == QSV_CODEC_HEVC) ? MFX_CODEC_HEVC
                                                               : MFX_CODEC_AVC;
    MFXSetConfigFilterProperty(
        QSVLoaderConfig[7],
        reinterpret_cast<const mfxU8 *>(
            "mfxImplDescription.mfxEncoderDescription.encoder.CodecID"),
        QSVLoaderVariant[7]);

    Status = MFXCreateSession(QSVLoader, GPUNum, &QSVSession);
    if (Status < MFX_ERR_NONE) {
      error("Error code: %d", Status);
      throw std::runtime_error("CreateSession(): MFXCreateSession error");
    }

    MFXQueryIMPL(QSVSession, &QSVImpl);

    if (QSVIsTextureEncoder) {
#if defined(_WIN32) || defined(_WIN64)
      // HWManager = std::make_unique<class HWManager>();
      //  Create DirectX device context
      if (HWManager->HWDeviceHandle == nullptr) {
        HWManager->CreateDevice(QSVImpl);
      }

      if (HWManager->HWDeviceHandle == nullptr) {
        error("Error code: %d", Status);
        throw std::runtime_error("CreateSession(): Handled device is nullptr");
      }

      /*Provide device manager to VPL*/
      Status = MFXVideoCORE_SetHandle(QSVSession, MFX_HANDLE_D3D11_DEVICE,
                                      HWManager->HWDeviceHandle);
      if (Status < MFX_ERR_NONE) {
        error("Error code: %d", Status);
        throw std::runtime_error("CreateSession(): SetHandle error");
      }
#else

#endif
    }

    MFXVideoCORE_QueryPlatform(QSVSession, &QSVPlatform);
    info("\tAdapter type: %s",
         QSVPlatform.MediaAdapterType == MFX_MEDIA_DISCRETE ? "Discrete"
                                                            : "Integrate");

    return Status;
  } catch (const std::exception &) {
    throw;
  }
}

mfxStatus QSVEncoder::Init(encoder_params *InputParams, enum codec_enum Codec,
                           bool bIsTextureEncoder) {
  mfxStatus Status = MFX_ERR_NONE;

  QSVIsTextureEncoder = std::move(bIsTextureEncoder);
  QSVProcessingEnable = std::move(InputParams->ProcessingEnable);

  info("\tEncoder type: %s",
       QSVIsTextureEncoder ? "Texture import" : "Frame import");
  try {
    if (QSVIsTextureEncoder) {
#if defined(_WIN32) || defined(_WIN64)
      HWManager = std::make_unique<class HWManager>();
#endif
    }

    Status = CreateSession(Codec, nullptr, InputParams->GPUNum);

    QSVEncode = std::make_unique<MFXVideoENCODE>(QSVSession);

    Status = SetEncoderParams(InputParams, Codec);
    Status = QSVEncode->Init(&QSVEncodeParams);

    Status = InitTexturePool();

    if (QSVProcessingEnable) {
      QSVProcessing = std::make_unique<MFXVideoVPP>(QSVSession);

      Status = SetProcessingParams(InputParams, Codec);

      Status = QSVProcessing->Init(&QSVProcessingParams);
    }

    Status = GetVideoParam(Codec);

    Status = InitBitstreamBuffer(Codec);

    Status = InitTaskPool(Codec);
  } catch (const std::exception &e) {
    error("Error code: %d. %s", Status, e.what());
    throw;
  }

  HWManager::HWEncoderCounter++;

  return Status;
}

mfxStatus
QSVEncoder::SetProcessingParams(struct encoder_params *InputParams,
                                [[maybe_unused]] enum codec_enum Codec) {
  QSVProcessingParams.vpp.In.FourCC = static_cast<mfxU32>(InputParams->FourCC);
  QSVProcessingParams.vpp.In.ChromaFormat =
      static_cast<mfxU16>(InputParams->ChromaFormat);
  QSVProcessingParams.vpp.In.Width =
      static_cast<mfxU16>((((InputParams->Width + 15) >> 4) << 4));
  QSVProcessingParams.vpp.In.Height =
      static_cast<mfxU16>((((InputParams->Height + 15) >> 4) << 4));
  QSVProcessingParams.vpp.In.CropW = static_cast<mfxU16>(InputParams->Width);
  QSVProcessingParams.vpp.In.CropH = static_cast<mfxU16>(InputParams->Height);
  QSVProcessingParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  QSVProcessingParams.vpp.In.FrameRateExtN =
      static_cast<mfxU32>(InputParams->FpsNum);
  QSVProcessingParams.vpp.In.FrameRateExtD =
      static_cast<mfxU32>(InputParams->FpsDen);
  QSVProcessingParams.vpp.Out.FourCC = static_cast<mfxU32>(InputParams->FourCC);
  QSVProcessingParams.vpp.Out.ChromaFormat =
      static_cast<mfxU16>(InputParams->ChromaFormat);
  QSVProcessingParams.vpp.Out.Width =
      static_cast<mfxU16>((((InputParams->Width + 15) >> 4) << 4));
  QSVProcessingParams.vpp.Out.Height =
      static_cast<mfxU16>((((InputParams->Height + 15) >> 4) << 4));
  QSVProcessingParams.vpp.Out.CropW = static_cast<mfxU16>(InputParams->Width);
  QSVProcessingParams.vpp.Out.CropH = static_cast<mfxU16>(InputParams->Height);
  QSVProcessingParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  QSVProcessingParams.vpp.Out.FrameRateExtN =
      static_cast<mfxU32>(InputParams->FpsNum);
  QSVProcessingParams.vpp.Out.FrameRateExtD =
      static_cast<mfxU32>(InputParams->FpsDen);

  if (InputParams->VPPDenoiseMode.has_value()) {
    auto DenoiseParams = QSVProcessingParams.AddExtBuffer<mfxExtVPPDenoise2>();
    DenoiseParams->Header.BufferId = MFX_EXTBUFF_VPP_DENOISE2;
    DenoiseParams->Header.BufferSz = sizeof(mfxExtVPPDenoise2);
    switch (InputParams->VPPDenoiseMode.value()) {
    case 1:
      DenoiseParams->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
      info("\tDenoise set: AUTO | BDRATE | PRE ENCODE");
      break;
    case 2:
      DenoiseParams->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
      info("\tDenoise set: AUTO | ADJUST | POST ENCODE");
      break;
    case 3:
      DenoiseParams->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
      info("\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
      break;
    case 4:
      DenoiseParams->Mode = MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
      DenoiseParams->Strength =
          static_cast<mfxU16>(InputParams->DenoiseStrength);
      info("\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
           DenoiseParams->Strength);
      break;
    case 5:
      DenoiseParams->Mode = MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
      DenoiseParams->Strength =
          static_cast<mfxU16>(InputParams->DenoiseStrength);
      info("\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
           DenoiseParams->Strength);
      break;
    default:
      DenoiseParams->Mode = MFX_DENOISE_MODE_DEFAULT;
      info("\tDenoise set: DEFAULT");
      break;
    }
  } else {
    info("\tDenoise set: OFF");
  }

  if (InputParams->VPPDetail.has_value()) {
    auto DetailParams = QSVProcessingParams.AddExtBuffer<mfxExtVPPDetail>();
    DetailParams->Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
    DetailParams->Header.BufferSz = sizeof(mfxExtVPPDetail);
    DetailParams->DetailFactor =
        static_cast<mfxU16>(InputParams->VPPDetail.value());
    info("\tDetail set: %d", InputParams->VPPDetail.value());
  } else {
    info("\tDetail set: OFF");
  }

  if (InputParams->VPPScalingMode.has_value()) {
    auto ScalingParams = QSVProcessingParams.AddExtBuffer<mfxExtVPPScaling>();
    ScalingParams->Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
    ScalingParams->Header.BufferSz = sizeof(mfxExtVPPScaling);
    switch (InputParams->VPPScalingMode.value()) {
    case 1:
      ScalingParams->ScalingMode = MFX_SCALING_MODE_QUALITY;
      ScalingParams->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: QUALITY + ADVANCED");
      break;
    case 2:
      ScalingParams->ScalingMode = MFX_SCALING_MODE_INTEL_GEN_VEBOX;
      ScalingParams->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: VEBOX + ADVANCED");
      break;
    case 3:
      ScalingParams->ScalingMode = MFX_SCALING_MODE_LOWPOWER;
      ScalingParams->InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;
      info("\tScaling set: LOWPOWER + NEAREST NEIGHBOR");
      break;
    case 4:
      ScalingParams->ScalingMode = MFX_SCALING_MODE_LOWPOWER;
      ScalingParams->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: LOWPOWER + ADVANCED");
      break;
    default:
      info("\tScaling set: AUTO");
      break;
    }
  } else {
    info("\tScaling set: OFF");
  }

  if (InputParams->VPPImageStabMode.has_value()) {
    auto ImageStabParams =
        QSVProcessingParams.AddExtBuffer<mfxExtVPPImageStab>();
    ImageStabParams->Header.BufferId = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
    ImageStabParams->Header.BufferSz = sizeof(mfxExtVPPImageStab);
    switch (InputParams->VPPImageStabMode.value()) {
    case 1:
      ImageStabParams->Mode = MFX_IMAGESTAB_MODE_UPSCALE;
      info("\tImageStab set: UPSCALE");
      break;
    case 2:
      ImageStabParams->Mode = MFX_IMAGESTAB_MODE_BOXING;
      info("\tImageStab set: BOXING");
      break;
    default:
      info("\tImageStab set: AUTO");
      break;
    }
  } else {
    info("\tImageStab set: OFF");
  }

  if (InputParams->PercEncPrefilter == true) {
    auto PercEncPrefilterParams =
        QSVProcessingParams.AddExtBuffer<mfxExtVPPPercEncPrefilter>();
    PercEncPrefilterParams->Header.BufferId =
        MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER;
    PercEncPrefilterParams->Header.BufferSz = sizeof(mfxExtVPPPercEncPrefilter);
    info("\tPercEncPreFilter set: ON");
  } else {
    info("\tPercEncPreFilter set: OFF");
  }

  QSVProcessingParams.IOPattern =
      MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

  mfxVideoParam ValidParams = {};
  memcpy(&ValidParams, &QSVProcessingParams, sizeof(mfxVideoParam));
  mfxStatus Status = QSVProcessing->Query(&QSVProcessingParams, &ValidParams);
  if (Status < MFX_ERR_NONE) {
    throw std::runtime_error("SetProcessingParams(): Query params error");
  }

  return Status;
}

mfxStatus QSVEncoder::SetEncoderParams(struct encoder_params *InputParams,
                                       enum codec_enum Codec) {
  /*It's only for debug*/
  bool COEnabled = 1;
  bool CO2Enabled = 1;
  bool CO3Enabled = 1;
  bool CODDIEnabled = 1;

  switch (Codec) {
  case QSV_CODEC_AV1:
    QSVEncodeParams.mfx.CodecId = MFX_CODEC_AV1;
    break;
  case QSV_CODEC_HEVC:
    QSVEncodeParams.mfx.CodecId = MFX_CODEC_HEVC;
    break;
  case QSV_CODEC_AVC:
  default:
    QSVEncodeParams.mfx.CodecId = MFX_CODEC_AVC;
    break;
  }

  // Width must be a multiple of 16
  // Height must be a multiple of 16 in case of frame picture and a
  // multiple of 32 in case of field picture
  QSVEncodeParams.mfx.FrameInfo.Width =
      static_cast<mfxU16>((((InputParams->Width + 15) >> 4) << 4));
  info("\tWidth: %d", QSVEncodeParams.mfx.FrameInfo.Width);

  QSVEncodeParams.mfx.FrameInfo.Height =
      static_cast<mfxU16>((((InputParams->Height + 15) >> 4) << 4));
  info("\tHeight: %d", QSVEncodeParams.mfx.FrameInfo.Height);

  QSVEncodeParams.mfx.FrameInfo.ChromaFormat =
      static_cast<mfxU16>(InputParams->ChromaFormat);

  QSVEncodeParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  QSVEncodeParams.mfx.FrameInfo.CropX = 0;

  QSVEncodeParams.mfx.FrameInfo.CropY = 0;

  QSVEncodeParams.mfx.FrameInfo.CropW = static_cast<mfxU16>(InputParams->Width);

  QSVEncodeParams.mfx.FrameInfo.CropH =
      static_cast<mfxU16>(InputParams->Height);

  QSVEncodeParams.mfx.FrameInfo.FrameRateExtN =
      static_cast<mfxU32>(InputParams->FpsNum);

  QSVEncodeParams.mfx.FrameInfo.FrameRateExtD =
      static_cast<mfxU32>(InputParams->FpsDen);

  QSVEncodeParams.mfx.FrameInfo.FourCC =
      static_cast<mfxU32>(InputParams->FourCC);

  QSVEncodeParams.mfx.FrameInfo.BitDepthChroma =
      InputParams->VideoFormat10bit ? 10 : 8;

  QSVEncodeParams.mfx.FrameInfo.BitDepthLuma =
      InputParams->VideoFormat10bit ? 10 : 8;

  if (InputParams->VideoFormat10bit) {
    QSVEncodeParams.mfx.FrameInfo.Shift = 1;
  }

  QSVEncodeParams.mfx.LowPower = GetCodingOpt(InputParams->Lowpower);
  info("\tLowpower set: %s",
       GetCodingOptStatus(QSVEncodeParams.mfx.LowPower).c_str());

  QSVEncodeParams.mfx.RateControlMethod =
      static_cast<mfxU16>(InputParams->RateControl);

  if ((InputParams->NumRefFrame > 0) && (InputParams->NumRefFrame < 17)) {
    if (Codec == QSV_CODEC_AVC && InputParams->Lookahead == true &&
        (InputParams->NumRefFrame < InputParams->GOPRefDist - 1)) {
      InputParams->NumRefFrame = InputParams->GOPRefDist;
      blog(LOG_WARNING, "\tThe AVC Codec using Lookahead may be unstable if "
                        "NumRefFrame < GopRefDist. The NumRefFrame value is "
                        "automatically set to the GopRefDist value");
    }
    QSVEncodeParams.mfx.NumRefFrame =
        static_cast<mfxU16>(InputParams->NumRefFrame);
    info("\tNumRefFrame set to: %d", InputParams->NumRefFrame);
  }

  QSVEncodeParams.mfx.TargetUsage =
      static_cast<mfxU16>(InputParams->TargetUsage);
  QSVEncodeParams.mfx.CodecProfile =
      static_cast<mfxU16>(InputParams->CodecProfile);
  if (Codec == QSV_CODEC_HEVC) {
    QSVEncodeParams.mfx.CodecProfile |= InputParams->CodecProfileTier;
  }

  /*This is a multiplier to bypass the limitation of the 16 bit value of
                  variables*/
  QSVEncodeParams.mfx.BRCParamMultiplier = 100;

  switch (InputParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    QSVEncodeParams.mfx.TargetKbps =
        static_cast<mfxU16>(InputParams->TargetBitRate);

    QSVEncodeParams.mfx.BufferSizeInKB =
        (InputParams->Lookahead == true)
            ? static_cast<mfxU16>((QSVEncodeParams.mfx.TargetKbps / 8) * 2)
            : static_cast<mfxU16>((QSVEncodeParams.mfx.TargetKbps / 8) * 1);
    if (InputParams->CustomBufferSize == true && InputParams->BufferSize > 0) {
      QSVEncodeParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(InputParams->BufferSize);
      info("\tCustomBufferSize set: ON");
    }
    QSVEncodeParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(QSVEncodeParams.mfx.BufferSizeInKB / 2);
    info("\tBufferSize set to: %d KB",
         QSVEncodeParams.mfx.BufferSizeInKB * 100);
    QSVEncodeParams.mfx.MaxKbps = QSVEncodeParams.mfx.TargetKbps;
    break;
  case MFX_RATECONTROL_VBR:
    QSVEncodeParams.mfx.TargetKbps =
        static_cast<mfxU16>(InputParams->TargetBitRate);
    QSVEncodeParams.mfx.MaxKbps = static_cast<mfxU16>(InputParams->MaxBitRate);
    QSVEncodeParams.mfx.BufferSizeInKB =
        (InputParams->Lookahead == true)
            ? static_cast<mfxU16>(
                  (QSVEncodeParams.mfx.TargetKbps / static_cast<float>(8)) /
                  (static_cast<float>(
                       QSVEncodeParams.mfx.FrameInfo.FrameRateExtN) /
                   QSVEncodeParams.mfx.FrameInfo.FrameRateExtD) *
                  (InputParams->LADepth +
                   (static_cast<float>(
                        QSVEncodeParams.mfx.FrameInfo.FrameRateExtN) /
                    QSVEncodeParams.mfx.FrameInfo.FrameRateExtD)))
            : static_cast<mfxU16>((QSVEncodeParams.mfx.TargetKbps / 8) * 1);
    if (InputParams->CustomBufferSize == true && InputParams->BufferSize > 0) {
      QSVEncodeParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(InputParams->BufferSize);
      info("\tCustomBufferSize set: ON");
    }
    QSVEncodeParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(QSVEncodeParams.mfx.BufferSizeInKB / 2);
    info("\tBufferSize set to: %d KB",
         QSVEncodeParams.mfx.BufferSizeInKB * 100);
    break;
  case MFX_RATECONTROL_CQP:
    QSVEncodeParams.mfx.QPI = static_cast<mfxU16>(InputParams->QPI);
    QSVEncodeParams.mfx.QPB = static_cast<mfxU16>(InputParams->QPB);
    QSVEncodeParams.mfx.QPP = static_cast<mfxU16>(InputParams->QPP);
    break;
  case MFX_RATECONTROL_ICQ:
    QSVEncodeParams.mfx.ICQQuality =
        static_cast<mfxU16>(InputParams->ICQQuality);
    break;
  }

  QSVEncodeParams.AsyncDepth = static_cast<mfxU16>(InputParams->AsyncDepth);

  QSVEncodeParams.mfx.GopPicSize =
      (InputParams->KeyIntSec > 0)
          ? static_cast<mfxU16>(
                InputParams->KeyIntSec *
                static_cast<float>(QSVEncodeParams.mfx.FrameInfo.FrameRateExtN /
                                   QSVEncodeParams.mfx.FrameInfo.FrameRateExtD))
          : 240;

  if ((!InputParams->AdaptiveI && !InputParams->AdaptiveB) ||
      (InputParams->AdaptiveI == false && InputParams->AdaptiveB == false)) {
    QSVEncodeParams.mfx.GopOptFlag = MFX_GOP_STRICT;
    info("\tGopOptFlag set: STRICT");
  } else {
    QSVEncodeParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
    info("\tGopOptFlag set: CLOSED");
  }

  switch (Codec) {
  case QSV_CODEC_HEVC:
    QSVEncodeParams.mfx.IdrInterval = 1;
    QSVEncodeParams.mfx.NumSlice = 0;
    break;
  default:
    QSVEncodeParams.mfx.IdrInterval = 0;
    QSVEncodeParams.mfx.NumSlice = 1;
    break;
  }

  QSVEncodeParams.mfx.GopRefDist = static_cast<mfxU16>(InputParams->GOPRefDist);

  if (Codec == QSV_CODEC_AV1 && InputParams->Lookahead == false &&
      InputParams->EncTools == true &&
      std::find(
          std::begin(ListAllowedGopRefDist), std::end(ListAllowedGopRefDist),
          QSVEncodeParams.mfx.GopRefDist) == std::end(ListAllowedGopRefDist)) {
    QSVEncodeParams.mfx.GopRefDist = 8;
    blog(LOG_WARNING,
         "\tThe AV1 Codec without Lookahead cannot be used with EncTools if "
         "GopRefDist does not "
         "match the values 1, 2, 4, 8, 16. GOPRefDist automaticaly set to 8.");
  }
  info("\tGOPRefDist set to: %d frames (%d b-frames)",
       (int)QSVEncodeParams.mfx.GopRefDist,
       (int)(QSVEncodeParams.mfx.GopRefDist == 0
                 ? 0
                 : QSVEncodeParams.mfx.GopRefDist - 1));

  if (COEnabled == 1) {
    auto COParams = QSVEncodeParams.AddExtBuffer<mfxExtCodingOption>();
    COParams->Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    COParams->Header.BufferSz = sizeof(mfxExtCodingOption);
    /*Don't touch it!*/
    COParams->CAVLC = MFX_CODINGOPTION_OFF;
    COParams->RefPicListReordering = MFX_CODINGOPTION_ON;
    COParams->RefPicMarkRep = MFX_CODINGOPTION_ON;
    COParams->PicTimingSEI = MFX_CODINGOPTION_ON;
    // COParams->AUDelimiter = MFX_CODINGOPTION_OFF;
    COParams->MaxDecFrameBuffering = InputParams->NumRefFrame;
    COParams->ResetRefList = MFX_CODINGOPTION_ON;
    COParams->FieldOutput = MFX_CODINGOPTION_ON;
    COParams->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    COParams->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    COParams->MVPrecision = MFX_MVPRECISION_QUARTERPEL;
    COParams->MECostType = static_cast<mfxU16>(8);
    COParams->MESearchType = static_cast<mfxU16>(16);
    COParams->MVSearchWindow.x = (Codec == QSV_CODEC_AVC)
                                     ? static_cast<mfxI16>(16)
                                     : static_cast<mfxI16>(32);
    COParams->MVSearchWindow.y = (Codec == QSV_CODEC_AVC)
                                     ? static_cast<mfxI16>(16)
                                     : static_cast<mfxI16>(32);

    if (InputParams->IntraRefEncoding == true) {
      COParams->RecoveryPointSEI = MFX_CODINGOPTION_ON;
    }

    COParams->RateDistortionOpt = GetCodingOpt(InputParams->RDO);
    info("\tRDO set: %s",
         GetCodingOptStatus(COParams->RateDistortionOpt).c_str());

    COParams->VuiVclHrdParameters = GetCodingOpt(InputParams->HRDConformance);
    COParams->VuiNalHrdParameters = GetCodingOpt(InputParams->HRDConformance);
    COParams->NalHrdConformance = GetCodingOpt(InputParams->HRDConformance);
    info("\tHRDConformance set: %s",
         GetCodingOptStatus(COParams->NalHrdConformance).c_str());
  }

  if (CO2Enabled == 1) {
    auto CO2Params = QSVEncodeParams.AddExtBuffer<mfxExtCodingOption2>();
    CO2Params->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    CO2Params->Header.BufferSz = sizeof(mfxExtCodingOption2);
    CO2Params->BufferingPeriodSEI = MFX_BPSEI_IFRAME;
    CO2Params->RepeatPPS = MFX_CODINGOPTION_OFF;
    CO2Params->FixedFrameRate = MFX_CODINGOPTION_ON;
    CO2Params->DisableDeblockingIdc = MFX_CODINGOPTION_OFF;
    CO2Params->EnableMAD = MFX_CODINGOPTION_ON;
    // if (QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
    //     QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
    //   CO2Params->MaxFrameSize = (QSVEncodeParams.mfx.TargetKbps *
    //                        QSVEncodeParams.mfx.BRCParamMultiplier * 1000 /
    //                        (8 * (QSVEncodeParams.mfx.FrameInfo.FrameRateExtN
    //                        /
    //                              QSVEncodeParams.mfx.FrameInfo.FrameRateExtD)))
    //                              *
    //                       10;
    // }

    CO2Params->ExtBRC = GetCodingOpt(InputParams->ExtBRC);
    info("\tExtBRC set: %s", GetCodingOptStatus(CO2Params->ExtBRC).c_str());

    if (InputParams->IntraRefEncoding == true) {

      CO2Params->IntRefType = MFX_REFRESH_HORIZONTAL;

      CO2Params->IntRefCycleSize =
          static_cast<mfxU16>(InputParams->IntraRefCycleSize > 1
                                  ? InputParams->IntraRefCycleSize
                                  : (QSVEncodeParams.mfx.GopRefDist > 1
                                         ? QSVEncodeParams.mfx.GopRefDist
                                         : 2));
      info("\tIntraRefCycleSize set: %d", CO2Params->IntRefCycleSize);
      if (InputParams->IntraRefQPDelta > -52 &&
          InputParams->IntraRefQPDelta < 52) {
        CO2Params->IntRefQPDelta =
            static_cast<mfxU16>(InputParams->IntraRefQPDelta);
        info("\tIntraRefQPDelta set: %d", CO2Params->IntRefQPDelta);
      }
    }

    if (QSVPlatform.CodeName < MFX_PLATFORM_DG2 &&
        QSVPlatform.MediaAdapterType == MFX_MEDIA_INTEGRATED &&
        InputParams->Lookahead == true && InputParams->Lowpower == false) {
      InputParams->Lookahead = false;
      InputParams->LADepth = 0;
      info("\tIntegrated graphics with Lowpower turned OFF does not "
           "\tsupport Lookahead");
    }

    if (QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
      if (InputParams->Lookahead == true) {
        CO2Params->LookAheadDepth = InputParams->LADepth;
        info("\tLookahead set to: ON");
        info("\tLookaheadDepth set to: %d", CO2Params->LookAheadDepth);
      }
    }

    CO2Params->MBBRC = GetCodingOpt(InputParams->MBBRC);
    info("\tMBBRC set: %s", GetCodingOptStatus(CO2Params->MBBRC).c_str());

    if (InputParams->GOPRefDist > 1) {
      CO2Params->BRefType = MFX_B_REF_PYRAMID;
      info("\tBPyramid set: ON");
    } else {
      CO2Params->BRefType = MFX_B_REF_UNKNOWN;
      info("\tBPyramid set: AUTO");
    }

    if (InputParams->Trellis.has_value()) {
      switch (InputParams->Trellis.value()) {
      case 0:
        CO2Params->Trellis = MFX_TRELLIS_OFF;
        info("\tTrellis set: OFF");
        break;
      case 1:
        CO2Params->Trellis = MFX_TRELLIS_I;
        info("\tTrellis set: I");
        break;
      case 2:
        CO2Params->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
        info("\tTrellis set: IP");
        break;
      case 3:
        CO2Params->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B;
        info("\tTrellis set: IPB");
        break;
      case 4:
        CO2Params->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
        info("\tTrellis set: IB");
        break;
      case 5:
        CO2Params->Trellis = MFX_TRELLIS_P;
        info("\tTrellis set: P");
        break;
      case 6:
        CO2Params->Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
        info("\tTrellis set: PB");
        break;
      case 7:
        CO2Params->Trellis = MFX_TRELLIS_B;
        info("\tTrellis set: B");
        break;
      default:
        info("\tTrellis set: AUTO");
        break;
      }
    }

    CO2Params->AdaptiveI = GetCodingOpt(InputParams->AdaptiveI);
    info("\tAdaptiveI set: %s",
         GetCodingOptStatus(CO2Params->AdaptiveI).c_str());

    CO2Params->AdaptiveB = GetCodingOpt(InputParams->AdaptiveB);
    info("\tAdaptiveB set: %s",
         GetCodingOptStatus(CO2Params->AdaptiveB).c_str());

    if (InputParams->RateControl == MFX_RATECONTROL_CBR ||
        InputParams->RateControl == MFX_RATECONTROL_VBR) {
      CO2Params->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
      if (InputParams->LookAheadDS.has_value() == true) {
        switch (InputParams->LookAheadDS.value()) {
        case 0:
          CO2Params->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
          info("\tLookAheadDS set: SLOW");
          break;
        case 1:
          CO2Params->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
          info("\tLookAheadDS set: MEDIUM");
          break;
        case 2:
          CO2Params->LookAheadDS = MFX_LOOKAHEAD_DS_4x;
          info("\tLookAheadDS set: FAST");
          break;
        default:
          info("\tLookAheadDS set: AUTO");
          break;
        }
      }
    }

    CO2Params->UseRawRef = GetCodingOpt(InputParams->RawRef);
    info("\tUseRawRef set: %s",
         GetCodingOptStatus(CO2Params->UseRawRef).c_str());
  }

  if (CO3Enabled == 1) {
    auto CO3Params = QSVEncodeParams.AddExtBuffer<mfxExtCodingOption3>();
    CO3Params->Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    CO3Params->Header.BufferSz = sizeof(mfxExtCodingOption3);
    CO3Params->TargetBitDepthLuma = InputParams->VideoFormat10bit ? 10 : 8;
    CO3Params->TargetBitDepthChroma = InputParams->VideoFormat10bit ? 10 : 8;
    CO3Params->TargetChromaFormatPlus1 =
        static_cast<mfxU16>(QSVEncodeParams.mfx.FrameInfo.ChromaFormat + 1);

    // if (QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
    //     QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
    //   CO3Params->MaxFrameSizeI = (QSVEncodeParams.mfx.TargetKbps *
    //                         QSVEncodeParams.mfx.BRCParamMultiplier * 1000 /
    //                         (8 * (QSVEncodeParams.mfx.FrameInfo.FrameRateExtN
    //                         /
    //                               QSVEncodeParams.mfx.FrameInfo.FrameRateExtD)))
    //                               *
    //                        8;
    //   CO3Params->MaxFrameSizeP = (QSVEncodeParams.mfx.TargetKbps *
    //                         QSVEncodeParams.mfx.BRCParamMultiplier * 1000 /
    //                         (8 * (QSVEncodeParams.mfx.FrameInfo.FrameRateExtN
    //                         /
    //                               QSVEncodeParams.mfx.FrameInfo.FrameRateExtD)))
    //                               *
    //                        5;
    // }

    CO3Params->MBDisableSkipMap = MFX_CODINGOPTION_ON;
    CO3Params->EnableQPOffset = MFX_CODINGOPTION_ON;

    CO3Params->BitstreamRestriction = MFX_CODINGOPTION_ON;
    CO3Params->AspectRatioInfoPresent = MFX_CODINGOPTION_ON;
    CO3Params->TimingInfoPresent = MFX_CODINGOPTION_ON;
    CO3Params->OverscanInfoPresent = MFX_CODINGOPTION_ON;

    CO3Params->LowDelayHrd = GetCodingOpt(InputParams->LowDelayHRD);
    info("\tLowDelayHRD set: %s",
         GetCodingOptStatus(CO3Params->LowDelayHrd).c_str());

    CO3Params->WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
    CO3Params->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;

    CO3Params->RepartitionCheckEnable = MFX_CODINGOPTION_ON;

    if (InputParams->NumRefActiveP.has_value() &&
        InputParams->NumRefActiveP > 0) {
      if (InputParams->NumRefActiveP.value() > InputParams->NumRefFrame) {
        InputParams->NumRefActiveP = InputParams->NumRefFrame;
        warn("\tThe NumRefActiveP value cannot exceed the NumRefFrame value");
      }
      std::fill(CO3Params->NumRefActiveP, CO3Params->NumRefActiveP + 8,
                InputParams->NumRefActiveP.value());
      info("\tNumRefActiveP set: %d", InputParams->NumRefActiveP.value());
    }

    if (InputParams->NumRefActiveBL0.has_value() &&
        InputParams->NumRefActiveBL0 > 0) {
      if (InputParams->NumRefActiveBL0.value() > InputParams->NumRefFrame) {
        InputParams->NumRefActiveBL0 = InputParams->NumRefFrame;
        warn("\tThe NumRefActiveBL0 value cannot exceed the NumRefFrame value");
      }
      std::fill(CO3Params->NumRefActiveBL0, CO3Params->NumRefActiveBL0 + 8,
                InputParams->NumRefActiveBL0.value());
      info("\tNumRefActiveBL0 set: %d", InputParams->NumRefActiveBL0.value());
    }

    if (InputParams->NumRefActiveBL1.has_value() &&
        InputParams->NumRefActiveBL1 > 0) {
      if (InputParams->NumRefActiveBL1.value() > InputParams->NumRefFrame) {
        InputParams->NumRefActiveBL1 = InputParams->NumRefFrame;
        warn("\tThe NumRefActiveBL1 value cannot exceed the NumRefFrame value");
      }
      std::fill(CO3Params->NumRefActiveBL1, CO3Params->NumRefActiveBL1 + 8,
                InputParams->NumRefActiveBL1.value());
      info("\tNumRefActiveBL0 set: %d", InputParams->NumRefActiveBL1.value());
    }

    if (InputParams->IntraRefEncoding == true) {
      CO3Params->IntRefCycleDist = 0;
    }

    CO3Params->ContentInfo = MFX_CONTENT_NOISY_VIDEO;

    if (InputParams->Lookahead == true && InputParams->LADepth < 9) {
      CO3Params->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
      info("\tScenario: REMOTE GAMING");
    } else if ((Codec == QSV_CODEC_AVC || Codec == QSV_CODEC_AV1) &&
               InputParams->Lookahead == true) {
      CO3Params->ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
      info("\tScenario: GAME STREAMING");
    } else if (InputParams->Lookahead == false ||
               (Codec == QSV_CODEC_HEVC && InputParams->Lookahead == true)) {
      CO3Params->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
      info("\tScenario: REMOTE GAMING");
    }

    if (QSVEncodeParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
      CO3Params->EnableMBQP = MFX_CODINGOPTION_ON;
    }

    if (Codec == QSV_CODEC_HEVC) {
      CO3Params->GPB = GetCodingOpt(InputParams->GPB);
      info("\tGPB set: %s", GetCodingOptStatus(CO3Params->GPB).c_str());
    }

    if (InputParams->PPyramid == true) {
      CO3Params->PRefType = MFX_P_REF_PYRAMID;
      info("\tPPyramid set: PYRAMID");
    } else {
      CO3Params->PRefType = MFX_P_REF_SIMPLE;
      info("\tPPyramid set: SIMPLE");
    }

    CO3Params->AdaptiveCQM = GetCodingOpt(InputParams->AdaptiveCQM);
    info("\tAdaptiveCQM set: %s",
         GetCodingOptStatus(CO3Params->AdaptiveCQM).c_str());

    CO3Params->AdaptiveRef = GetCodingOpt(InputParams->AdaptiveRef);
    info("\tAdaptiveRef set: %s",
         GetCodingOptStatus(CO3Params->AdaptiveRef).c_str());

    CO3Params->AdaptiveLTR = GetCodingOpt(InputParams->AdaptiveLTR);
    if (InputParams->ExtBRC == true && Codec == QSV_CODEC_AVC) {
      CO3Params->ExtBrcAdaptiveLTR = GetCodingOpt(InputParams->AdaptiveLTR);
    }
    info("\tAdaptiveLTR set: %s",
         GetCodingOptStatus(CO3Params->AdaptiveLTR).c_str());

    if (InputParams->WinBRCMaxAvgSize > 0) {
      CO3Params->WinBRCMaxAvgKbps =
          static_cast<mfxU16>(InputParams->WinBRCMaxAvgSize *
                              QSVEncodeParams.mfx.BRCParamMultiplier);
      info("\tWinBRCMaxSize set: %d", CO3Params->WinBRCMaxAvgKbps);
    }

    if (InputParams->WinBRCSize > 0) {
      CO3Params->WinBRCSize = static_cast<mfxU16>(InputParams->WinBRCSize);
      info("\tWinBRCSize set: %d", CO3Params->WinBRCSize);
    }

    CO3Params->MotionVectorsOverPicBoundaries =
        GetCodingOpt(InputParams->MotionVectorsOverPicBoundaries);
    info("\tMotionVectorsOverPicBoundaries set: %s",
         GetCodingOptStatus(CO3Params->MotionVectorsOverPicBoundaries).c_str());

    if (InputParams->GlobalMotionBiasAdjustment.has_value() &&
        InputParams->GlobalMotionBiasAdjustment.value() == true) {
      CO3Params->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_ON;
      info("\tGlobalMotionBiasAdjustment set: ON");
      if (InputParams->MVCostScalingFactor.has_value()) {
        switch (InputParams->MVCostScalingFactor.value()) {
        case 1:
          CO3Params->MVCostScalingFactor = 1;
          info("\tMVCostScalingFactor set: 1/2");
          break;
        case 2:
          CO3Params->MVCostScalingFactor = 2;
          info("\tMVCostScalingFactor set: 1/4");
          break;
        case 3:
          CO3Params->MVCostScalingFactor = 3;
          info("\tMVCostScalingFactor set: 1/8");
          break;
        default:
          info("\tMVCostScalingFactor set: DEFAULT");
          break;
        }
      } else {
        info("\tMVCostScalingFactor set: AUTO");
      }
    } else {
      CO3Params->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
    }

    CO3Params->DirectBiasAdjustment =
        GetCodingOpt(InputParams->DirectBiasAdjustment);
    info("\tDirectBiasAdjustment set: %s",
         GetCodingOptStatus(CO3Params->DirectBiasAdjustment).c_str());
  }

#if defined(_WIN32) || defined(_WIN64)
  if (InputParams->EncTools == true) {
    auto EncToolsParams = QSVEncodeParams.AddExtBuffer<mfxExtEncToolsConfig>();
    EncToolsParams->Header.BufferId = MFX_EXTBUFF_ENCTOOLS_CONFIG;
    EncToolsParams->Header.BufferSz = sizeof(mfxExtEncToolsConfig);

    info("\tEncTools set: ON");
  }

  /*Don't touch it! Magic beyond the control of mere mortals takes place
   * here*/
  if (CODDIEnabled == 1 && Codec != QSV_CODEC_AV1) {
    auto CODDIParams = QSVEncodeParams.AddExtBuffer<mfxExtCodingOptionDDI>();
    CODDIParams->Header.BufferId = MFX_EXTBUFF_DDI;
    CODDIParams->Header.BufferSz = sizeof(mfxExtCodingOptionDDI);
    CODDIParams->WriteIVFHeaders = MFX_CODINGOPTION_OFF;
    CODDIParams->IBC = MFX_CODINGOPTION_ON;
    CODDIParams->Palette = MFX_CODINGOPTION_ON;
    CODDIParams->BRCPrecision = 3;
    CODDIParams->BiDirSearch = MFX_CODINGOPTION_ON;
    CODDIParams->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
    CODDIParams->GlobalSearch = 1;
    CODDIParams->IntraPredCostType = 8;
    CODDIParams->MEFractionalSearchType = 16;
    CODDIParams->MEInterpolationMethod = 8;
    CODDIParams->MVPrediction = MFX_CODINGOPTION_ON;
    CODDIParams->WeightedBiPredIdc = 2;
    CODDIParams->WeightedPrediction = MFX_CODINGOPTION_ON;
    CODDIParams->FieldPrediction = MFX_CODINGOPTION_ON;
    CODDIParams->DirectCheck = MFX_CODINGOPTION_ON;
    CODDIParams->FractionalQP = 1;
    CODDIParams->Hme = MFX_CODINGOPTION_ON;
    CODDIParams->LocalSearch = 6;
    CODDIParams->MBAFF = MFX_CODINGOPTION_ON;
    CODDIParams->DDI.InterPredBlockSize = 64;
    CODDIParams->DDI.IntraPredBlockSize = 1;
    CODDIParams->RefOppositeField = MFX_CODINGOPTION_ON;
    CODDIParams->RefRaw = GetCodingOpt(InputParams->RawRef);
    CODDIParams->TMVP = MFX_CODINGOPTION_ON;
    CODDIParams->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDIParams->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDIParams->QpAdjust = MFX_CODINGOPTION_ON;
    CODDIParams->Transform8x8Mode = MFX_CODINGOPTION_ON;
    CODDIParams->EarlySkip = 0;
    CODDIParams->RefreshFrameContext = MFX_CODINGOPTION_ON;
    CODDIParams->ChangeFrameContextIdxForTS = MFX_CODINGOPTION_ON;
    CODDIParams->SuperFrameForTS = MFX_CODINGOPTION_ON;
     if (Codec == QSV_CODEC_AVC) {
       if (InputParams->NumRefActiveP.has_value() &&
           InputParams->NumRefActiveP > 0) {
         if (InputParams->NumRefActiveP.value() > InputParams->NumRefFrame) {
           InputParams->NumRefActiveP = InputParams->NumRefFrame;
           warn("\tThe NumActiveRefP value cannot exceed the NumRefFrame value");
         }

        CODDIParams->NumActiveRefP = InputParams->NumRefActiveP.value();
      }

      if (InputParams->NumRefActiveBL0.has_value() &&
          InputParams->NumRefActiveBL0 > 0) {
        if (InputParams->NumRefActiveBL0.value() > InputParams->NumRefFrame) {
          InputParams->NumRefActiveBL0 = InputParams->NumRefFrame;
          warn("\tThe NumActiveRefP value cannot exceed the NumRefFrame value");
        }

        CODDIParams->NumActiveRefBL0 = InputParams->NumRefActiveBL0.value();
      }

      if (InputParams->NumRefActiveBL1.has_value() &&
          InputParams->NumRefActiveBL1 > 0) {
        if (InputParams->NumRefActiveBL1.value() > InputParams->NumRefFrame) {
          InputParams->NumRefActiveBL1 = InputParams->NumRefFrame;
          warn("\tThe NumActiveRefP value cannot exceed the NumRefFrame value");
        }

        CODDIParams->NumActiveRefBL1 = InputParams->NumRefActiveBL1.value();
      }
    }
  }
#endif

  if (Codec == QSV_CODEC_HEVC) {
    auto ChromaLocParams = QSVEncodeParams.AddExtBuffer<mfxExtChromaLocInfo>();
    ChromaLocParams->Header.BufferId = MFX_EXTBUFF_CHROMA_LOC_INFO;
    ChromaLocParams->Header.BufferSz = sizeof(mfxExtChromaLocInfo);
    ChromaLocParams->ChromaLocInfoPresentFlag = 1;
    ChromaLocParams->ChromaSampleLocTypeTopField =
        static_cast<mfxU16>(InputParams->ChromaSampleLocTypeTopField);
    ChromaLocParams->ChromaSampleLocTypeBottomField =
        static_cast<mfxU16>(InputParams->ChromaSampleLocTypeBottomField);

    auto HevcParams = QSVEncodeParams.AddExtBuffer<mfxExtHEVCParam>();
    HevcParams->Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
    HevcParams->Header.BufferSz = sizeof(mfxExtHEVCParam);
    HevcParams->PicWidthInLumaSamples = QSVEncodeParams.mfx.FrameInfo.Width;
    HevcParams->PicHeightInLumaSamples = QSVEncodeParams.mfx.FrameInfo.Height;

    if (InputParams->SAO.has_value()) {
      switch (InputParams->SAO.value()) {
      case 0:
        HevcParams->SampleAdaptiveOffset = MFX_SAO_DISABLE;
        info("\tSAO set: DISABLE");
        break;
      case 1:
        HevcParams->SampleAdaptiveOffset = MFX_SAO_ENABLE_LUMA;
        info("\tSAO set: LUMA");
        break;
      case 2:
        HevcParams->SampleAdaptiveOffset = MFX_SAO_ENABLE_CHROMA;
        info("\tSAO set: CHROMA");
        break;
      case 3:
        HevcParams->SampleAdaptiveOffset =
            MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA;
        info("\tSAO set: ALL");
        break;
      }
    } else {
      info("\tSAO set: AUTO");
    }

    auto HevcTilesParams = QSVEncodeParams.AddExtBuffer<mfxExtHEVCTiles>();
    HevcTilesParams->Header.BufferId = MFX_EXTBUFF_HEVC_TILES;
    HevcTilesParams->Header.BufferSz = sizeof(mfxExtHEVCTiles);
    HevcTilesParams->NumTileColumns = 1;
    HevcTilesParams->NumTileRows = 1;
  }

  if (Codec == QSV_CODEC_AV1) {
    if (QSVVersion.Major >= 2 && QSVVersion.Minor >= 12 ||
        QSVVersion.Major > 2) {
      if (QSVPlatform.CodeName >= MFX_PLATFORM_LUNARLAKE &&
          QSVPlatform.CodeName != MFX_PLATFORM_ALDERLAKE_N &&
          QSVPlatform.CodeName != MFX_PLATFORM_ARROWLAKE) {
        auto AV1ScreenContentTools =
            QSVEncodeParams.AddExtBuffer<mfxExtAV1ScreenContentTools>();
        AV1ScreenContentTools->Header.BufferId =
            MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS;
        AV1ScreenContentTools->Header.BufferSz =
            sizeof(mfxExtAV1ScreenContentTools);
        AV1ScreenContentTools->Palette = MFX_CODINGOPTION_ON;

        info("\AV1ScreenContentTools set: ON");
      }
    }

    auto AV1BitstreamParams =
        QSVEncodeParams.AddExtBuffer<mfxExtAV1BitstreamParam>();
    AV1BitstreamParams->Header.BufferId = MFX_EXTBUFF_AV1_BITSTREAM_PARAM;
    AV1BitstreamParams->Header.BufferSz = sizeof(mfxExtAV1BitstreamParam);
    AV1BitstreamParams->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    auto AV1TileParams = QSVEncodeParams.AddExtBuffer<mfxExtAV1TileParam>();
    AV1TileParams->Header.BufferId = MFX_EXTBUFF_AV1_TILE_PARAM;
    AV1TileParams->Header.BufferSz = sizeof(mfxExtAV1TileParam);
    AV1TileParams->NumTileGroups = 1;
    if ((InputParams->Height * InputParams->Width) >= 8294400) {
      AV1TileParams->NumTileColumns = 2;
      AV1TileParams->NumTileRows = 2;
    } else {
      AV1TileParams->NumTileColumns = 1;
      AV1TileParams->NumTileRows = 1;
    }

    if (InputParams->TuneQualityMode.has_value()) {
      auto TuneQualityParams =
          QSVEncodeParams.AddExtBuffer<mfxExtTuneEncodeQuality>();
      TuneQualityParams->Header.BufferId = MFX_EXTBUFF_TUNE_ENCODE_QUALITY;
      TuneQualityParams->Header.BufferSz = sizeof(mfxExtTuneEncodeQuality);
      switch ((int)InputParams->TuneQualityMode.value()) {
      default:
      case 0:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_OFF;
        info("\tTuneQualityMode set: DEFAULT");
        break;
      case 1:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_PSNR;
        info("\tTuneQualityMode set: PSNR");
        break;
      case 2:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_SSIM;
        info("\tTuneQualityMode set: SSIM");
        break;
      case 3:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
        info("\tTuneQualityMode set: MS SSIM");
        break;
      case 4:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_VMAF;
        info("\tTuneQualityMode set: VMAF");
        break;
      case 5:
        TuneQualityParams->TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
        info("\tTuneQualityMode set: PERCEPTUAL");
        break;
      }
    } else {
      info("\tTuneQualityMode set: OFF");
    }
  }

#if defined(_WIN32) || defined(_WIN64)
  auto VideoSignalParams =
      QSVEncodeParams.AddExtBuffer<mfxExtVideoSignalInfo>();
  VideoSignalParams->Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
  VideoSignalParams->Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
  VideoSignalParams->VideoFormat =
      static_cast<mfxU16>(InputParams->VideoFormat);
  VideoSignalParams->VideoFullRange =
      static_cast<mfxU16>(InputParams->VideoFullRange);
  VideoSignalParams->ColourDescriptionPresent = 1;
  VideoSignalParams->ColourPrimaries =
      static_cast<mfxU16>(InputParams->ColourPrimaries);
  VideoSignalParams->TransferCharacteristics =
      static_cast<mfxU16>(InputParams->TransferCharacteristics);
  VideoSignalParams->MatrixCoefficients =
      static_cast<mfxU16>(InputParams->MatrixCoefficients);
#endif

  if (InputParams->MaxContentLightLevel > 0) {
    auto ColourVolumeParams =
        QSVEncodeParams.AddExtBuffer<mfxExtMasteringDisplayColourVolume>();
    ColourVolumeParams->Header.BufferId =
        MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
    ColourVolumeParams->Header.BufferSz =
        sizeof(mfxExtMasteringDisplayColourVolume);
    ColourVolumeParams->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ColourVolumeParams->DisplayPrimariesX[0] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesX[0]);
    ColourVolumeParams->DisplayPrimariesX[1] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesX[1]);
    ColourVolumeParams->DisplayPrimariesX[2] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesX[2]);
    ColourVolumeParams->DisplayPrimariesY[0] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesY[0]);
    ColourVolumeParams->DisplayPrimariesY[1] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesY[1]);
    ColourVolumeParams->DisplayPrimariesY[2] =
        static_cast<mfxU16>(InputParams->DisplayPrimariesY[2]);
    ColourVolumeParams->WhitePointX =
        static_cast<mfxU16>(InputParams->WhitePointX);
    ColourVolumeParams->WhitePointY =
        static_cast<mfxU16>(InputParams->WhitePointY);
    ColourVolumeParams->MaxDisplayMasteringLuminance =
        static_cast<mfxU32>(InputParams->MaxDisplayMasteringLuminance);
    ColourVolumeParams->MinDisplayMasteringLuminance =
        static_cast<mfxU32>(InputParams->MinDisplayMasteringLuminance);

    auto ContentLightLevelParams =
        QSVEncodeParams.AddExtBuffer<mfxExtContentLightLevelInfo>();
    ContentLightLevelParams->Header.BufferId =
        MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
    ContentLightLevelParams->Header.BufferSz =
        sizeof(mfxExtContentLightLevelInfo);
    ContentLightLevelParams->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ContentLightLevelParams->MaxContentLightLevel =
        static_cast<mfxU16>(InputParams->MaxContentLightLevel);
    ContentLightLevelParams->MaxPicAverageLightLevel =
        static_cast<mfxU16>(InputParams->MaxPicAverageLightLevel);
  }

  QSVEncodeParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  info("\tFeature extended buffer size: %d", QSVEncodeParams.NumExtParam);

  // We dont check what was valid or invalid here, just try changing lower
  // power. Ensure set values are not overwritten so in case it wasnt lower
  // power we fail during the parameter check.
  mfxVideoParam ValidParams = {};
  memcpy(&ValidParams, &QSVEncodeParams, sizeof(mfxVideoParam));
  mfxStatus Status = QSVEncode->Query(&QSVEncodeParams, &ValidParams);
  if (Status == MFX_ERR_UNSUPPORTED || Status == MFX_ERR_UNDEFINED_BEHAVIOR) {
    auto CO3Params = QSVEncodeParams.GetExtBuffer<mfxExtCodingOption3>();
    if (CO3Params->AdaptiveLTR == MFX_CODINGOPTION_ON) {
      CO3Params->AdaptiveLTR = MFX_CODINGOPTION_OFF;
    }
  } else if (Status < MFX_ERR_NONE) {
    throw std::runtime_error("SetEncoderParams(): Query params error");
  }

  return Status;
}

bool QSVEncoder::UpdateParams(struct encoder_params *InputParams) {
  QSVResetParamsChanged = false;

  mfxStatus Status = QSVEncode->GetVideoParam(&QSVResetParams);
  if (Status < MFX_ERR_NONE) {
    return false;
  }

  QSVResetParams.NumExtParam = 0;
  switch (InputParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    if (QSVResetParams.mfx.TargetKbps != InputParams->TargetBitRate) {
      QSVResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(InputParams->TargetBitRate);
      QSVResetParamsChanged = true;
    }
    break;
  case MFX_RATECONTROL_VBR:
    if (QSVResetParams.mfx.TargetKbps != InputParams->TargetBitRate) {
      QSVResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(InputParams->TargetBitRate);
      QSVResetParamsChanged = true;
    }
    if (QSVResetParams.mfx.MaxKbps != InputParams->MaxBitRate) {
      QSVResetParams.mfx.MaxKbps = static_cast<mfxU16>(InputParams->MaxBitRate);
      QSVResetParamsChanged = true;
    }
    if (QSVResetParams.mfx.MaxKbps < QSVResetParams.mfx.TargetKbps) {
      QSVResetParams.mfx.MaxKbps = QSVResetParams.mfx.TargetKbps;
      QSVResetParamsChanged = true;
    }
    break;
  case MFX_RATECONTROL_CQP:
    if (QSVResetParams.mfx.QPI != InputParams->QPI) {
      QSVResetParams.mfx.QPI = static_cast<mfxU16>(InputParams->QPI);
      QSVResetParams.mfx.QPB = static_cast<mfxU16>(InputParams->QPB);
      QSVResetParams.mfx.QPP = static_cast<mfxU16>(InputParams->QPP);
      QSVResetParamsChanged = true;
    }
    break;
  case MFX_RATECONTROL_ICQ:
    if (QSVResetParams.mfx.ICQQuality != InputParams->ICQQuality) {
      QSVResetParams.mfx.ICQQuality =
          static_cast<mfxU16>(InputParams->ICQQuality);
      QSVResetParamsChanged = true;
    }
  }
  if (QSVResetParamsChanged == true) {
    auto ResetParams = QSVEncodeParams.AddExtBuffer<mfxExtEncoderResetOption>();
    ResetParams->Header.BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
    ResetParams->Header.BufferSz = sizeof(mfxExtEncoderResetOption);
    ResetParams->StartNewSequence = MFX_CODINGOPTION_ON;
    QSVEncode->Query(&QSVResetParams, &QSVResetParams);
    return true;
  } else {
    return false;
  }
}

mfxStatus QSVEncoder::ReconfigureEncoder() {
  if (QSVResetParamsChanged == true) {
    return QSVEncode->Reset(&QSVResetParams);
  } else {
    return MFX_ERR_NONE;
  }
}

mfxStatus QSVEncoder::InitTexturePool() {
  mfxStatus Status = MFX_ERR_NONE;

  if (QSVIsTextureEncoder) {
    // Allocate textures
    try {
      Status = HWManager->AllocateTexturePool(QSVEncodeParams);
    } catch (const std::exception &e) {
      error("Error code: %d, %s", Status, e.what());
      throw;
    }

    Status = MFXGetMemoryInterface(QSVSession, &QSVMemoryInterface);
    if (Status < MFX_ERR_NONE) {
      throw std::runtime_error(
          "AllocateTexturePool(): MFXGetMemoryInterface error");
    }
  }

  return Status;
}

mfxStatus
QSVEncoder::InitBitstreamBuffer([[maybe_unused]] enum codec_enum Codec) {
  try {
    QSVBitstream.MaxLength =
        static_cast<mfxU32>(QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 10);
    QSVBitstream.DataOffset = 0;
    QSVBitstream.DataLength = 0;
#if defined(_WIN32) || defined(_WIN64)
    if (nullptr == (QSVBitstream.Data = static_cast<mfxU8 *>(
                        _aligned_malloc(QSVBitstream.MaxLength, 32)))) {
      throw std::runtime_error(
          "InitBitstreamBuffer(): Bitstream memory allocation error");
    }
#elif defined(__linux__)
    if (nullptr == (QSVBitstream.Data = static_cast<mfxU8 *>(
                        aligned_alloc(32, QSVBitstream.MaxLength)))) {
      throw std::runtime_error(
          "InitBitstreamBuffer(): Bitstream memory allocation error");
    }
#endif

    info("\tBitstream size: %d Kb", QSVBitstream.MaxLength / 1000);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }
  return MFX_ERR_NONE;
}

mfxStatus QSVEncoder::InitTaskPool([[maybe_unused]] enum codec_enum Codec) {
  try {
    QSVSyncTaskID = 0;
    Task NewTask = {};
    QSVTaskPool.reserve(QSVEncodeParams.AsyncDepth);

    for (int i = 0; i < QSVEncodeParams.AsyncDepth; i++) {
      NewTask.Bitstream.MaxLength =
          static_cast<mfxU32>(QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 10);
      NewTask.Bitstream.DataOffset = 0;
      NewTask.Bitstream.DataLength = 0;
#if defined(_WIN32) || defined(_WIN64)
      if (nullptr == (NewTask.Bitstream.Data = static_cast<mfxU8 *>(
                          _aligned_malloc(NewTask.Bitstream.MaxLength, 32)))) {
        throw std::runtime_error(
            "InitTaskPool(): Task memory allocation error");
      }
#elif defined(__linux__)
      if (nullptr == (NewTask.Bitstream.Data = static_cast<mfxU8 *>(
                          aligned_alloc(32, NewTask.Bitstream.MaxLength)))) {
        throw std::runtime_error(
            "InitTaskPool(): Task memory allocation error");
      }
#endif
      info("\tTask #%d bitstream size: %d", i,
           NewTask.Bitstream.MaxLength / 1000);

      QSVTaskPool.push_back(NewTask);
    }

    info("\tTaskPool count: %d", QSVTaskPool.size());

  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }

  return MFX_ERR_NONE;
}

void QSVEncoder::ReleaseBitstream() {
  if (QSVBitstream.Data) {
    try {
#if defined(_WIN32) || defined(_WIN64)
      _aligned_free(QSVBitstream.Data);
#elif defined(__linux__)
      free(QSVBitstream.Data);
#endif
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
  QSVBitstream.Data = nullptr;
}

void QSVEncoder::ReleaseTask(int TaskID) {
  if (QSVTaskPool[TaskID].Bitstream.Data) {
    try {
#if defined(_WIN32) || defined(_WIN64)
      _aligned_free(QSVTaskPool[TaskID].Bitstream.Data);
#elif defined(__linux__)
      free(QSVTaskPool[TaskID].Bitstream.Data);
#endif
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
  QSVTaskPool[TaskID].Bitstream.Data = nullptr;
}

void QSVEncoder::ReleaseTaskPool() {
  if (!QSVTaskPool.empty()) {
    try {
      for (int i = 0; i < QSVTaskPool.size(); i++) {
        ReleaseTask(i);
      }

      QSVTaskPool.clear();
      QSVTaskPool.shrink_to_fit();

    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
}

mfxStatus QSVEncoder::ChangeBitstreamSize(mfxU32 NewSize) {
  try {
#if defined(_WIN32) || defined(_WIN64)
    mfxU8 *Data = static_cast<mfxU8 *>(_aligned_malloc(NewSize, 32));
#elif defined(__linux__)
    mfxU8 *Data = static_cast<mfxU8 *>(aligned_alloc(32, NewSize));
#endif
    if (Data == nullptr) {
      throw std::runtime_error(
          "ChangeBitstreamSize(): Bitstream memory allocation error");
    }

    mfxU32 DataLen = std::move(QSVBitstream.DataLength);
    if (QSVBitstream.DataLength) {
      memcpy(Data, QSVBitstream.Data + QSVBitstream.DataOffset,
             std::min(DataLen, NewSize));
    }
    ReleaseBitstream();

    QSVBitstream.Data = std::move(Data);
    QSVBitstream.DataOffset = 0;
    QSVBitstream.DataLength = std::move(static_cast<mfxU32>(DataLen));
    QSVBitstream.MaxLength = std::move(static_cast<mfxU32>(NewSize));

    for (int i = 0; i < QSVTaskPool.size(); i++) {
#if defined(_WIN32) || defined(_WIN64)
      mfxU8 *TaskData = static_cast<mfxU8 *>(_aligned_malloc(NewSize, 32));
#elif defined(__linux__)
      mfxU8 *TaskData = static_cast<mfxU8 *>(aligned_alloc(32, NewSize));
#endif
      if (TaskData == nullptr) {
        throw std::runtime_error(
            "ChangeBitstreamSize(): Task memory allocation error");
      }

      mfxU32 TaskDataLen = std::move(QSVTaskPool[i].Bitstream.DataLength);
      if (QSVTaskPool[i].Bitstream.DataLength) {
        memcpy(TaskData,
               QSVTaskPool[i].Bitstream.Data +
                   QSVTaskPool[i].Bitstream.DataOffset,
               std::min(TaskDataLen, NewSize));
      }
      ReleaseTask(i);

      QSVTaskPool[i].Bitstream.Data = std::move(TaskData);
      QSVTaskPool[i].Bitstream.DataOffset = 0;
      QSVTaskPool[i].Bitstream.DataLength =
          std::move(static_cast<mfxU32>(TaskDataLen));
      QSVTaskPool[i].Bitstream.MaxLength =
          std::move(static_cast<mfxU32>(NewSize));
    }

  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }

  return MFX_ERR_NONE;
}

mfxStatus QSVEncoder::GetVideoParam(enum codec_enum Codec) {
  auto SPSPPSParams = QSVEncodeParams.AddExtBuffer<mfxExtCodingOptionSPSPPS>();
  SPSPPSParams->Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
  SPSPPSParams->Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
  SPSPPSParams->SPSBuffer = QSVSPSBuffer;
  SPSPPSParams->PPSBuffer = QSVPPSBuffer;
  SPSPPSParams->SPSBufSize = 1024;
  SPSPPSParams->PPSBufSize = 1024;

  if (Codec == QSV_CODEC_HEVC) {
    auto VPSParams = QSVEncodeParams.AddExtBuffer<mfxExtCodingOptionVPS>();
    VPSParams->Header.BufferId = MFX_EXTBUFF_CODING_OPTION_VPS;
    VPSParams->Header.BufferSz = sizeof(mfxExtCodingOptionVPS);
    VPSParams->VPSBuffer = QSVVPSBuffer;
    VPSParams->VPSBufSize = 1024;
  }

  mfxStatus Status = QSVEncode->GetVideoParam(&QSVEncodeParams);

  info("\tGetVideoParam status:     %d", Status);
  if (Status < MFX_ERR_NONE) {
    error("Error code: %d", Status);
    throw std::runtime_error("GetVideoParam(): Get video parameters error");
  }

  if (Codec == QSV_CODEC_AV1) {
    QSVEncodeParams.mfx.BufferSizeInKB *= 25;
  } else if (Codec == QSV_CODEC_HEVC) {
    QSVEncodeParams.mfx.BufferSizeInKB *= 100;
  } else {
    QSVEncodeParams.mfx.BufferSizeInKB *= 75;
  }

  return Status;
}

void QSVEncoder::LoadFrameData(mfxFrameSurface1 *&Surface, uint8_t **FrameData,
                               uint32_t *FrameLinesize) {
  mfxU16 Width, Height, i, Pitch;
  mfxU8 *PTR;
  const mfxFrameInfo *SurfaceInfo = &Surface->Info;
  const mfxFrameData *SurfaceData = &Surface->Data;

  if (SurfaceInfo->CropH > 0 && SurfaceInfo->CropW > 0) {
    Width = SurfaceInfo->CropW;
    Height = SurfaceInfo->CropH;
  } else {
    Width = SurfaceInfo->Width;
    Height = SurfaceInfo->Height;
  }
  Pitch = SurfaceData->Pitch;

  if (Surface->Info.FourCC == MFX_FOURCC_NV12) {
    PTR = static_cast<mfxU8 *>(SurfaceData->Y + SurfaceInfo->CropX +
                               SurfaceInfo->CropY * Pitch);

    // load Y plane
    for (i = 0; i < Height; i++) {
      avx2_memcpy(PTR + i * Pitch, FrameData[0] + i * FrameLinesize[0], Width);
    }

    // load UV plane
    Height /= 2;
    PTR = static_cast<mfxU8 *>((SurfaceData->UV + SurfaceInfo->CropX +
                                (SurfaceInfo->CropY / 2) * Pitch));

    for (i = 0; i < Height; i++) {
      avx2_memcpy(PTR + i * Pitch, FrameData[1] + i * FrameLinesize[1], Width);
    }
  } else if (Surface->Info.FourCC == MFX_FOURCC_P010) {
    PTR = static_cast<mfxU8 *>(SurfaceData->Y + SurfaceInfo->CropX +
                               SurfaceInfo->CropY * Pitch);
    const size_t line_size = static_cast<size_t>(Width) * 2;
    // load Y plane
    for (i = 0; i < Height; i++) {
      avx2_memcpy(PTR + i * Pitch, FrameData[0] + i * FrameLinesize[0],
                  line_size);
    }
    // load UV plane
    Height /= 2;
    PTR = static_cast<mfxU8 *>((SurfaceData->UV + SurfaceInfo->CropX +
                                (SurfaceInfo->CropY / 2) * Pitch));
    for (i = 0; i < Height; i++) {
      avx2_memcpy(PTR + i * Pitch, FrameData[1] + i * FrameLinesize[1],
                  line_size);
    }
  } else if (Surface->Info.FourCC == MFX_FOURCC_RGB4) {
    const size_t line_size = static_cast<size_t>(Width) * 4;
    // load B plane
    for (i = 0; i < Height; i++) {
      avx2_memcpy(SurfaceData->B + i * Pitch,
                  FrameData[0] + i * FrameLinesize[0], line_size);
    }
  }
}

mfxStatus QSVEncoder::GetFreeTaskIndex(int *TaskID) {
  if (!QSVTaskPool.empty()) {
    for (int i = 0; i < QSVTaskPool.size(); i++) {
      if (static_cast<mfxSyncPoint>(nullptr) == QSVTaskPool[i].SyncPoint) {
        QSVSyncTaskID = (i + 1) % static_cast<int>(QSVTaskPool.size());
        *TaskID = i;
        return MFX_ERR_NONE;
      }
    }
  }
  return MFX_ERR_NOT_FOUND;
}

mfxStatus QSVEncoder::EncodeTexture(mfxU64 TS, void *TextureHandle,
                                    uint64_t LockKey, uint64_t *NextKey,
                                    mfxBitstream **Bitstream) {
  mfxStatus Status = MFX_ERR_NONE, SyncStatus = MFX_ERR_NONE;
  *Bitstream = nullptr;
  int TaskID = 0;

#if defined(_WIN32) || defined(_WIN64)
  mfxSurfaceD3D11Tex2D Texture = {};
  Texture.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_D3D11_TEX2D;
  Texture.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
  Texture.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceD3D11Tex2D);
#else
  mfxSurfaceVAAPI Texture{};
  Texture.SurfaceInterface.Header.SurfaceType = MFXSURFACE_TYPE_VAAPI;
  Texture.SurfaceInterface.Header.SurfaceFlags = MFXSURFACE_FLAG_IMPORT_COPY;
  Texture.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceVAAPI);
#endif

  while (GetFreeTaskIndex(&TaskID) == MFX_ERR_NOT_FOUND) {
    do {
      SyncStatus = MFXVideoCORE_SyncOperation(
          QSVSession, QSVTaskPool[QSVSyncTaskID].SyncPoint, 100);
      if (SyncStatus < MFX_ERR_NONE) {
        error("Error code: %d", SyncStatus);
        throw std::runtime_error("Encode(): Syncronization error");
      }
    } while (SyncStatus == MFX_WRN_IN_EXECUTION);

    mfxU8 *DataTemp = std::move(QSVBitstream.Data);
    QSVBitstream = std::move(QSVTaskPool[QSVSyncTaskID].Bitstream);

    QSVTaskPool[QSVSyncTaskID].Bitstream.Data = std::move(DataTemp);
    QSVTaskPool[QSVSyncTaskID].Bitstream.DataLength = 0;
    QSVTaskPool[QSVSyncTaskID].Bitstream.DataOffset = 0;
    QSVTaskPool[QSVSyncTaskID].SyncPoint = nullptr;
    TaskID = std::move(QSVSyncTaskID);
    *Bitstream = std::move(&QSVBitstream);
  }

  try {
    HWManager->CopyTexture(Texture, std::move(TextureHandle), LockKey,
                           static_cast<mfxU64 *>(NextKey));
  } catch (const std::exception &e) {
    error("Error code: %d. %s", Status, e.what());
    throw;
  }

  Status = QSVMemoryInterface->ImportFrameSurface(
      QSVMemoryInterface, MFX_SURFACE_COMPONENT_ENCODE,
      reinterpret_cast<mfxSurfaceHeader *>(&Texture), &QSVEncodeSurface);
  if (Status < MFX_ERR_NONE) {
    error("Error code: %d", Status);
    throw std::runtime_error("Encode(): Texture import error");
  }

  QSVTaskPool[TaskID].Bitstream.TimeStamp = TS;
  QSVEncodeSurface->Data.TimeStamp = std::move(TS);

  for (;;) {
    if (QSVProcessingEnable) {
      mfxSyncPoint VPPSyncPoint = nullptr;

      Status = QSVProcessing->GetSurfaceOut(&QSVProcessingSurface);
      if (Status < MFX_ERR_NONE) {
        error("Error code: %d", Status);
        throw std::runtime_error("Encode(): Get VPP out Surface error");
      }

      Status = QSVProcessing->RunFrameVPPAsync(
          QSVEncodeSurface, QSVProcessingSurface, nullptr, &VPPSyncPoint);
      if (Status < MFX_ERR_NONE) {
        error("Error code: %d", Status);
        throw std::runtime_error("Encode(): VPP processing error");
      }

      QSVEncodeSurface->FrameInterface->Release(QSVEncodeSurface);
    }

    Status = QSVEncode->EncodeFrameAsync(
        nullptr,
        (QSVProcessingEnable ? QSVProcessingSurface : QSVEncodeSurface),
        &QSVTaskPool[TaskID].Bitstream, &QSVTaskPool[TaskID].SyncPoint);

    if (MFX_ERR_NONE == Status) {
      break;
    } else if (MFX_ERR_NONE < Status && !QSVTaskPool[TaskID].SyncPoint) {
      if (MFX_WRN_DEVICE_BUSY == Status)
        Sleep(1);
    } else if (MFX_ERR_NONE < Status && QSVTaskPool[TaskID].SyncPoint) {
      Status = MFX_ERR_NONE;
      break;
    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == Status ||
               MFX_ERR_MORE_BITSTREAM == Status) {
      ChangeBitstreamSize(static_cast<mfxU32>(
          (QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 5) * 2));
      warn("The buffer size is too small. The size has been increased by 2 "
           "times. New value: %d KB",
           (((QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 5) * 2) / 1000));
      QSVEncodeParams.mfx.BufferSizeInKB *= 2;
    } else if (MFX_ERR_MORE_DATA == Status) {
      break;
    } else {
      error("Error code: %d", Status);
      break;
    }
  }

  if (QSVProcessingEnable) {
    QSVProcessingSurface->FrameInterface->Release(QSVProcessingSurface);
  } else {
    QSVEncodeSurface->FrameInterface->Release(QSVEncodeSurface);
  }

  return Status;
}

mfxStatus QSVEncoder::EncodeFrame(mfxU64 TS, uint8_t **FrameData,
                                  uint32_t *FrameLinesize,
                                  mfxBitstream **Bitstream) {
  mfxStatus Status = MFX_ERR_NONE, SyncStatus = MFX_ERR_NONE;
  *Bitstream = nullptr;
  int TaskID = 0;

  QSVEncode->GetSurface(&QSVEncodeSurface);

  while (GetFreeTaskIndex(&TaskID) == MFX_ERR_NOT_FOUND) {
    do {
      SyncStatus = MFXVideoCORE_SyncOperation(
          QSVSession, QSVTaskPool[QSVSyncTaskID].SyncPoint, 100);
      if (SyncStatus < MFX_ERR_NONE) {
        warn("Encode.Sync error: %d", SyncStatus);
      }
    } while (SyncStatus == MFX_WRN_IN_EXECUTION);

    mfxU8 *DataTemp = std::move(QSVBitstream.Data);
    QSVBitstream = std::move(QSVTaskPool[QSVSyncTaskID].Bitstream);

    QSVTaskPool[QSVSyncTaskID].Bitstream.Data = std::move(DataTemp);
    QSVTaskPool[QSVSyncTaskID].Bitstream.DataLength = 0;
    QSVTaskPool[QSVSyncTaskID].Bitstream.DataOffset = 0;
    QSVTaskPool[QSVSyncTaskID].SyncPoint = nullptr;
    TaskID = std::move(QSVSyncTaskID);
    *Bitstream = std::move(&QSVBitstream);
  }

  Status =
      QSVEncodeSurface->FrameInterface->Map(QSVEncodeSurface, MFX_MAP_WRITE);
  if (Status < MFX_ERR_NONE) {
    warn("Surface.Map.Write error: %d", Status);
    return Status;
  }

  QSVEncodeSurface->Data.TimeStamp = TS;
  LoadFrameData(QSVEncodeSurface, std::move(FrameData),
                std::move(FrameLinesize));

  QSVTaskPool[TaskID].Bitstream.TimeStamp = std::move(TS);

  Status = QSVEncodeSurface->FrameInterface->Unmap(QSVEncodeSurface);
  if (Status < MFX_ERR_NONE) {
    warn("Surface.Unmap.Write error: %d", Status);
    return Status;
  }

  /*Encode a frame asynchronously (returns immediately)*/
  for (;;) {
    if (QSVProcessingEnable) {
      mfxSyncPoint VPPSyncPoint = nullptr;

      Status = QSVProcessing->GetSurfaceOut(&QSVProcessingSurface);
      if (Status < MFX_ERR_NONE) {
        warn("VPP Surface error: %d", Status);
        return MFX_ERR_NOT_INITIALIZED;
      }
      Status = QSVProcessing->RunFrameVPPAsync(
          QSVEncodeSurface, QSVProcessingSurface, nullptr, &VPPSyncPoint);
      if (Status < MFX_ERR_NONE) {
        warn("VPP error: %d", Status);
      }

      QSVEncodeSurface->FrameInterface->Release(QSVEncodeSurface);
    }

    Status = QSVEncode->EncodeFrameAsync(
        nullptr,
        (QSVProcessingEnable ? QSVProcessingSurface : QSVEncodeSurface),
        &QSVTaskPool[TaskID].Bitstream, &QSVTaskPool[TaskID].SyncPoint);

    if (MFX_ERR_NONE == Status) {
      break;
    } else if (MFX_ERR_NONE < Status && !QSVTaskPool[TaskID].SyncPoint) {
      if (MFX_WRN_DEVICE_BUSY == Status)
        Sleep(1);
    } else if (MFX_ERR_NONE < Status && QSVTaskPool[TaskID].SyncPoint) {
      Status = MFX_ERR_NONE;
      break;
    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == Status ||
               MFX_ERR_MORE_BITSTREAM == Status) {
      ChangeBitstreamSize(
          ((QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 5) * 2));
      warn("The buffer size is too small. The size has been increased by 2 "
           "times. New value: %d KB",
           (((QSVEncodeParams.mfx.BufferSizeInKB * 1000 * 5) * 2) / 1000));
      QSVEncodeParams.mfx.BufferSizeInKB *= 2;
    } else if (MFX_ERR_MORE_DATA == Status) {
      break;
    } else {
      warn("Encode error: %d", Status);
      break;
    }
  }

  if (QSVProcessingEnable) {
    QSVProcessingSurface->FrameInterface->Release(QSVProcessingSurface);
  } else {
    QSVEncodeSurface->FrameInterface->Release(QSVEncodeSurface);
  }

  return MFX_ERR_NONE;
}

mfxStatus QSVEncoder::Drain() {
  mfxStatus Status = MFX_ERR_NONE;

  if (QSVTaskPool[QSVSyncTaskID].SyncPoint != nullptr) {
    Status = MFXVideoCORE_SyncOperation(
        QSVSession, QSVTaskPool[QSVSyncTaskID].SyncPoint, INFINITE);

    QSVTaskPool[QSVSyncTaskID].SyncPoint = nullptr;
  }

  return Status;
}

mfxStatus QSVEncoder::ClearData() {
  mfxStatus Status = MFX_ERR_NONE;
  try {

    if (QSVEncode) {
      // Status = Drain();

      // QSVEncodeParams.~ExtBufManager();
      Status = QSVEncode->Close();
      if (Status >= MFX_ERR_NONE) {
        QSVEncode = nullptr;
      }
    }

    if (QSVProcessing) {
      // QSVProcessingEnableParams.~ExtBufManager();
      Status = QSVProcessing->Close();
      if (Status >= MFX_ERR_NONE) {
        QSVProcessing = nullptr;
      }
    }

    ReleaseTaskPool();
    ReleaseBitstream();

    if (Status >= MFX_ERR_NONE) {
      HWManager::HWEncoderCounter--;
    }

    if (QSVSession) {
      Status = MFXClose(QSVSession);
      if (Status >= MFX_ERR_NONE) {
        MFXDispReleaseImplDescription(QSVLoader, nullptr);
        MFXUnload(QSVLoader);
        QSVSession = nullptr;
        QSVLoader = nullptr;
      }
    }

#if defined(__linux__)
    ReleaseSessionData(QSVSessionData);
    QSVSessionData = nullptr;
#endif

    if (QSVIsTextureEncoder) {
      HWManager->ReleaseTexturePool();

      if (HWManager::HWEncoderCounter <= 0) {
        // delete HWManager;
        HWManager = nullptr;
        HWManager::HWEncoderCounter = 0;
      }
    }
  } catch (const std::exception &) {
    throw;
  }

  return Status;
}
