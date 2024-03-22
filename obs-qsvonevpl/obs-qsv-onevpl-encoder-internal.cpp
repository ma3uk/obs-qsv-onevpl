#include "obs-qsv-onevpl-encoder-internal.hpp"

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal()
    : mfx_Platform(), mfx_Version(), mfx_Loader(), mfx_LoaderConfig(),
      mfx_LoaderVariant(), mfx_Session(nullptr), mfx_EncSurface(),
      mfx_EncSurfaceRefCount(), mfx_VPPSurface(), mfx_VideoEnc(nullptr),
      mfx_VideoVPP(), VPS_Buffer(), SPS_Buffer(), PPS_Buffer(),
      VPS_BufferSize(1024), SPS_BufferSize(1024), PPS_BufferSize(1024),
      mfx_Bitstream(), mfx_TaskPoolSize(0), mfx_TaskPool(), mfx_SyncTaskID(),
      mfx_ResetParams(), ResetParamChanged(false), mfx_EncParams(),
      mfx_EncCtrlParams(), mfx_AllocRequest(), mfx_UseTexAlloc(),
      mfx_MemoryInterface(), hw(), mfx_VPP() {}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal() {
  if (mfx_VideoEnc || mfx_VideoVPP) {
    ClearData();
  }
}

mfxStatus QSV_VPL_Encoder_Internal::GetVPLVersion(mfxVersion &version) {
  mfxStatus sts = MFX_ERR_NONE;
  mfx_Loader = MFXLoad();
  if (mfx_Loader == nullptr) {
    throw std::runtime_error("GetVPLSession(): MFXLoad error");
  }
  sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
  if (sts == MFX_ERR_NONE) {
    MFXQueryVersion(mfx_Session, &mfx_Version);
    version = mfx_Version;
    MFXClose(mfx_Session);
    MFXUnload(mfx_Loader);
  } else {
    throw std::runtime_error("GetVPLSession(): MFXCreateSession error");
  }

  return sts;
}

mfxStatus
QSV_VPL_Encoder_Internal::Initialize([[maybe_unused]] enum qsv_codec codec,
                                     [[maybe_unused]] void **data) {
  mfxStatus sts = MFX_ERR_NONE;

  // Initialize Intel VPL Session
  mfx_Loader = MFXLoad();
  if (mfx_Loader == nullptr) {
    return MFX_ERR_UNDEFINED_BEHAVIOR;
  }

  mfx_LoaderConfig[0] = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant[0].Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig[0],
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.Impl.mfxImplType"),
      mfx_LoaderVariant[0]);

  mfx_LoaderConfig[1] = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant[1].Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant[1].Data.U32 = static_cast<mfxU32>(0x8086);
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig[1],
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.VendorID"),
      mfx_LoaderVariant[1]);

  mfx_LoaderConfig[2] = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant[2].Type = MFX_VARIANT_TYPE_PTR;
  mfx_LoaderVariant[2].Data.Ptr = mfxHDL("mfx-gen");
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig[2],
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.ImplName"),
      mfx_LoaderVariant[2]);

  mfx_LoaderConfig[3] = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant[3].Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant[3].Data.U32 = MFX_PRIORITY_HIGH;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig[3],
      reinterpret_cast<const mfxU8 *>(
          "mfxInitializationParam.mfxExtThreadsParam.Priority"),
      mfx_LoaderVariant[3]);

  if (mfx_UseTexAlloc) {
#if defined(_WIN32) || defined(_WIN64)
    mfx_LoaderConfig[4] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[4].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[4].Data.U32 = MFX_HANDLE_D3D11_DEVICE;
    MFXSetConfigFilterProperty(mfx_LoaderConfig[4],
                               reinterpret_cast<const mfxU8 *>("mfxHandleType"),
                               mfx_LoaderVariant[4]);

    mfx_LoaderConfig[5] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[5].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[5].Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[5],
        reinterpret_cast<const mfxU8 *>("mfxImplDescription.AccelerationMode"),
        mfx_LoaderVariant[5]);

    mfx_LoaderConfig[6] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[6].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[6].Data.U32 = MFX_SURFACE_TYPE_D3D11_TEX2D;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[6],
        reinterpret_cast<const mfxU8 *>(
            "mfxSurfaceTypesSupported.surftype.SurfaceType"),
        mfx_LoaderVariant[6]);
#else
    mfx_LoaderConfig[4] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[4].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[4].Data.U32 = MFX_HANDLE_VA_DISPLAY;
    MFXSetConfigFilterProperty(mfx_LoaderConfig[4],
                               reinterpret_cast<const mfxU8 *>("mfxHandleType"),
                               mfx_LoaderVariant[4]);

    mfx_UseTexAlloc = false;
    mfx_LoaderConfig[5] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[5].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[5].Data.U32 = MFX_ACCEL_MODE_VIA_VAAPI;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[5],
        reinterpret_cast<const mfxU8 *>("mfxImplDescription.AccelerationMode"),
        mfx_LoaderVariant[5]);

    mfx_LoaderConfig[6] = MFXCreateConfig(mfx_Loader);
    mfx_LoaderVariant[6].Type = MFX_VARIANT_TYPE_U32;
    mfx_LoaderVariant[6].Data.U32 = MFX_SURFACE_TYPE_VAAPI;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[6],
        reinterpret_cast<const mfxU8 *>(
            "mfxSurfaceTypesSupported.surftype.SurfaceType"),
        mfx_LoaderVariant[6]);
#endif
    mfx_LoaderVariant[6].Data.U32 = MFX_SURFACE_COMPONENT_ENCODE;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[6],
        reinterpret_cast<const mfxU8 *>(
            "mfxSurfaceTypesSupported.surftype.surfcomp.SurfaceComponent"),
        mfx_LoaderVariant[6]);

    mfx_LoaderVariant[6].Data.U32 = MFX_SURFACE_FLAG_IMPORT_SHARED;
    MFXSetConfigFilterProperty(
        mfx_LoaderConfig[6],
        reinterpret_cast<const mfxU8 *>(
            "mfxSurfaceTypesSupported.surftype.surfcomp.SurfaceFlags"),
        mfx_LoaderVariant[6]);
  }

  sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
  if (sts < MFX_ERR_NONE) {
    error("Error code: %d", sts);
    throw std::runtime_error("Initialize(): MFXCreateSession error");
  }

  if (mfx_UseTexAlloc) {
#if defined(_WIN32) || defined(_WIN64)
    // Create DirectX device context
    if (hw->device_handle == nullptr) {
      try {
        hw->create_device(mfx_Session);
      } catch (std::runtime_error &) {
        throw;
      }
    }

    if (hw->device_handle == nullptr) {
      error("Error code: %d", sts);
      throw std::runtime_error("Initialize(): Handled device is nullptr");
    }

    /*Provide device manager to VPL*/
    sts = MFXVideoCORE_SetHandle(mfx_Session, MFX_HANDLE_D3D11_DEVICE,
                                 hw->device_handle);
    if (sts < MFX_ERR_NONE) {
      error("Error code: %d", sts);
      throw std::runtime_error("Initialize(): SetHandle error");
    }
#else

#endif
  }

  info("\tEncoder type: %s",
       mfx_UseTexAlloc ? "Texture import" : "Frame import");

  MFXVideoCORE_QueryPlatform(mfx_Session, &mfx_Platform);
  info("\tAdapter type: %s", mfx_Platform.MediaAdapterType == MFX_MEDIA_DISCRETE
                                 ? "Discrete"
                                 : "Integrate");

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Open(qsv_param_t *pParams,
                                         enum qsv_codec codec,
                                         bool useTexAlloc) {
  mfxStatus sts = MFX_ERR_NONE;

  hw = new hw_handle();

  mfx_UseTexAlloc = useTexAlloc;
  mfx_VPP = pParams->bVPPEnable;
  try {
    sts = Initialize(codec, nullptr);

    mfx_VideoEnc = new MFXVideoENCODE(mfx_Session);

    sts = InitEncParams(pParams, codec);

    sts = AllocateTextures();

    sts = mfx_VideoEnc->Init(&mfx_EncParams);

    if (mfx_VPP) {
      mfx_VideoVPP = new MFXVideoVPP(mfx_Session);

      sts = InitVPPParams(pParams, codec);

      sts = mfx_VideoVPP->Init(&mfx_VPPParams);
    }

    sts = GetVideoParam(codec);

    sts = InitBitstream(codec);

    sts = InitTaskPool(codec);
  } catch (const std::exception &) {
    ClearData();
    error("Error code: %d", sts);
    throw;
  }

  hw_handle::encoder_counter++;

  return sts;
}

mfxStatus
QSV_VPL_Encoder_Internal::InitVPPParams(struct qsv_param_t *pParams,
                                        [[maybe_unused]] enum qsv_codec codec) {
  mfx_VPPParams.vpp.In.FourCC = static_cast<mfxU32>(pParams->nFourCC);
  mfx_VPPParams.vpp.In.ChromaFormat =
      static_cast<mfxU16>(pParams->nChromaFormat);
  mfx_VPPParams.vpp.In.Width =
      static_cast<mfxU16>((((pParams->nWidth + 15) >> 4) << 4));
  mfx_VPPParams.vpp.In.Height =
      static_cast<mfxU16>((((pParams->nHeight + 15) >> 4) << 4));
  mfx_VPPParams.vpp.In.CropW = static_cast<mfxU16>(pParams->nWidth);
  mfx_VPPParams.vpp.In.CropH = static_cast<mfxU16>(pParams->nHeight);
  mfx_VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  mfx_VPPParams.vpp.In.FrameRateExtN = static_cast<mfxU32>(pParams->nFpsNum);
  mfx_VPPParams.vpp.In.FrameRateExtD = static_cast<mfxU32>(pParams->nFpsDen);
  mfx_VPPParams.vpp.Out.FourCC = static_cast<mfxU32>(pParams->nFourCC);
  mfx_VPPParams.vpp.Out.ChromaFormat =
      static_cast<mfxU16>(pParams->nChromaFormat);
  mfx_VPPParams.vpp.Out.Width =
      static_cast<mfxU16>((((pParams->nWidth + 15) >> 4) << 4));
  mfx_VPPParams.vpp.Out.Height =
      static_cast<mfxU16>((((pParams->nHeight + 15) >> 4) << 4));
  mfx_VPPParams.vpp.Out.CropW = static_cast<mfxU16>(pParams->nWidth);
  mfx_VPPParams.vpp.Out.CropH = static_cast<mfxU16>(pParams->nHeight);
  mfx_VPPParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
  mfx_VPPParams.vpp.Out.FrameRateExtN = static_cast<mfxU32>(pParams->nFpsNum);
  mfx_VPPParams.vpp.Out.FrameRateExtD = static_cast<mfxU32>(pParams->nFpsDen);

  if (pParams->nVPPDenoiseMode.has_value()) {
    auto DenoiseParam = mfx_VPPParams.AddExtBuffer<mfxExtVPPDenoise2>();
    switch (pParams->nVPPDenoiseMode.value()) {
    case 1:
      DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
      info("\tDenoise set: AUTO | BDRATE | PRE ENCODE");
      break;
    case 2:
      DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
      info("\tDenoise set: AUTO | ADJUST | POST ENCODE");
      break;
    case 3:
      DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
      info("\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
      break;
    case 4:
      DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
      DenoiseParam->Strength = static_cast<mfxU16>(pParams->nDenoiseStrength);
      info("\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
           DenoiseParam->Strength);
      break;
    case 5:
      DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
      DenoiseParam->Strength = static_cast<mfxU16>(pParams->nDenoiseStrength);
      info("\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
           DenoiseParam->Strength);
      break;
    default:
      DenoiseParam->Mode = MFX_DENOISE_MODE_DEFAULT;
      info("\tDenoise set: DEFAULT");
      break;
    }
  } else {
    info("\tDenoise set: OFF");
  }

  if (pParams->nVPPDetail.has_value()) {
    auto DetailParam = mfx_VPPParams.AddExtBuffer<mfxExtVPPDetail>();
    DetailParam->DetailFactor =
        static_cast<mfxU16>(pParams->nVPPDetail.value());
    info("\tDetail set: %d", pParams->nVPPDetail.value());
  } else {
    info("\tDetail set: OFF");
  }

  if (pParams->nVPPScalingMode.has_value()) {
    auto ScalingParam = mfx_VPPParams.AddExtBuffer<mfxExtVPPScaling>();
    switch (pParams->nVPPScalingMode.value()) {
    case 1:
      ScalingParam->ScalingMode = MFX_SCALING_MODE_QUALITY;
      ScalingParam->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: QUALITY + ADVANCED");
      break;
    case 2:
      ScalingParam->ScalingMode = MFX_SCALING_MODE_INTEL_GEN_VEBOX;
      ScalingParam->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: VEBOX + ADVANCED");
      break;
    case 3:
      ScalingParam->ScalingMode = MFX_SCALING_MODE_LOWPOWER;
      ScalingParam->InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;
      info("\tScaling set: LOWPOWER + NEAREST NEIGHBOR");
      break;
    case 4:
      ScalingParam->ScalingMode = MFX_SCALING_MODE_LOWPOWER;
      ScalingParam->InterpolationMethod = MFX_INTERPOLATION_ADVANCED;
      info("\tScaling set: LOWPOWER + ADVANCED");
      break;
    default:
      info("\tScaling set: AUTO");
      break;
    }
  } else {
    info("\tScaling set: OFF");
  }

  if (pParams->nVPPImageStabMode.has_value()) {
    auto ImageStabParam = mfx_VPPParams.AddExtBuffer<mfxExtVPPImageStab>();
    switch (pParams->nVPPImageStabMode.value()) {
    case 1:
      ImageStabParam->Mode = MFX_IMAGESTAB_MODE_UPSCALE;
      info("\tImageStab set: UPSCALE");
      break;
    case 2:
      ImageStabParam->Mode = MFX_IMAGESTAB_MODE_BOXING;
      info("\tImageStab set: BOXING");
      break;
    default:
      info("\tImageStab set: AUTO");
      break;
    }
  } else {
    info("\tImageStab set: OFF");
  }

  if (pParams->bPercEncPrefilter == true) {
    auto PercEncPrefilterParam =
        mfx_VPPParams.AddExtBuffer<mfxExtVPPPercEncPrefilter>();
    info("\tPercEncPreFilter set: ON");
  } else {
    info("\tPercEncPreFilter set: OFF");
  }

  mfx_VPPParams.IOPattern =
      MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

  mfxVideoParam validParams = {};
  memcpy(&validParams, &mfx_VPPParams, sizeof(mfxVideoParam));
  mfxStatus sts = mfx_VideoVPP->Query(&mfx_VPPParams, &validParams);
  if (sts < MFX_ERR_NONE) {
    throw std::runtime_error("InitVPPParams(): Query params error");
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitEncParams(struct qsv_param_t *pParams,
                                                  enum qsv_codec codec) {
  /*It's only for debug*/
  bool mfx_Ext_CO_enable = 1;
  bool mfx_Ext_CO2_enable = 1;
  bool mfx_Ext_CO3_enable = 1;
  bool mfx_Ext_CO_DDI_enable = 1;

  switch (codec) {
  case QSV_CODEC_AV1:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_AV1;
    break;
  case QSV_CODEC_HEVC:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_HEVC;
    break;
  case QSV_CODEC_VP9:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_VP9;
    break;
  case QSV_CODEC_AVC:
  default:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_AVC;
    break;
  }

  // Width must be a multiple of 16
  // Height must be a multiple of 16 in case of frame picture and a
  // multiple of 32 in case of field picture
  mfx_EncParams.mfx.FrameInfo.Width =
      static_cast<mfxU16>((((pParams->nWidth + 15) >> 4) << 4));
  info("\tWidth: %d", mfx_EncParams.mfx.FrameInfo.Width);

  mfx_EncParams.mfx.FrameInfo.Height =
      static_cast<mfxU16>((((pParams->nHeight + 15) >> 4) << 4));
  info("\tHeight: %d", mfx_EncParams.mfx.FrameInfo.Height);

  mfx_EncParams.mfx.FrameInfo.ChromaFormat =
      static_cast<mfxU16>(pParams->nChromaFormat);

  mfx_EncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  mfx_EncParams.mfx.FrameInfo.CropX = 0;

  mfx_EncParams.mfx.FrameInfo.CropY = 0;

  mfx_EncParams.mfx.FrameInfo.CropW = static_cast<mfxU16>(pParams->nWidth);

  mfx_EncParams.mfx.FrameInfo.CropH = static_cast<mfxU16>(pParams->nHeight);

  mfx_EncParams.mfx.FrameInfo.FrameRateExtN =
      static_cast<mfxU32>(pParams->nFpsNum);

  mfx_EncParams.mfx.FrameInfo.FrameRateExtD =
      static_cast<mfxU32>(pParams->nFpsDen);

  mfx_EncParams.mfx.FrameInfo.FourCC = static_cast<mfxU32>(pParams->nFourCC);

  mfx_EncParams.mfx.FrameInfo.BitDepthChroma =
      pParams->video_fmt_10bit ? 10 : 8;

  mfx_EncParams.mfx.FrameInfo.BitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;

  if (pParams->video_fmt_10bit) {
    mfx_EncParams.mfx.FrameInfo.Shift = 1;
  }

  mfx_EncParams.mfx.LowPower = GetCodingOpt(pParams->bLowpower);
  info("\tLowpower set: %s",
       GetCodingOptStatus(mfx_EncParams.mfx.LowPower).c_str());

  mfx_EncParams.mfx.RateControlMethod =
      static_cast<mfxU16>(pParams->RateControl);

  if ((pParams->nNumRefFrame > 0) && (pParams->nNumRefFrame < 17)) {
    if (codec == QSV_CODEC_AVC && pParams->bLookahead == true &&
        (pParams->nNumRefFrame < pParams->nGOPRefDist - 1)) {
      pParams->nNumRefFrame = pParams->nGOPRefDist;
      blog(LOG_WARNING, "\tThe AVC codec using Lookahead may be unstable if "
                        "NumRefFrame < GopRefDist. The NumRefFrame value is "
                        "automatically set to the GopRefDist value");
    }
    mfx_EncParams.mfx.NumRefFrame = static_cast<mfxU16>(pParams->nNumRefFrame);
    info("\tNumRefFrame set to: %d", pParams->nNumRefFrame);
  }

  mfx_EncParams.mfx.TargetUsage = static_cast<mfxU16>(pParams->nTargetUsage);
  mfx_EncParams.mfx.CodecProfile = static_cast<mfxU16>(pParams->CodecProfile);
  if (codec == QSV_CODEC_HEVC) {
    mfx_EncParams.mfx.CodecProfile |= pParams->HEVCTier;
  }

  /*This is a multiplier to bypass the limitation of the 16 bit value of
                  variables*/
  mfx_EncParams.mfx.BRCParamMultiplier = 100;

  switch (pParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    mfx_EncParams.mfx.TargetKbps = static_cast<mfxU16>(pParams->nTargetBitRate);

    mfx_EncParams.mfx.BufferSizeInKB =
        (pParams->bLookahead == true)
            ? static_cast<mfxU16>(
                  (mfx_EncParams.mfx.TargetKbps / static_cast<float>(8)) /
                  (static_cast<float>(
                       mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                   mfx_EncParams.mfx.FrameInfo.FrameRateExtD) *
                  (pParams->nLADepth +
                   (static_cast<float>(
                        mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                    mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
            : static_cast<mfxU16>((mfx_EncParams.mfx.TargetKbps / 8) * 1);
    if (pParams->bCustomBufferSize == true && pParams->nBufferSize > 0) {
      mfx_EncParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(pParams->nBufferSize);
      info("\tCustomBufferSize set: ON");
    }
    mfx_EncParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(mfx_EncParams.mfx.BufferSizeInKB / 2);
    info("\tBufferSize set to: %d KB", mfx_EncParams.mfx.BufferSizeInKB * 100);
    mfx_EncParams.mfx.MaxKbps = mfx_EncParams.mfx.TargetKbps;
    break;
  case MFX_RATECONTROL_VBR:
    mfx_EncParams.mfx.TargetKbps = static_cast<mfxU16>(pParams->nTargetBitRate);
    mfx_EncParams.mfx.MaxKbps = static_cast<mfxU16>(pParams->nMaxBitRate);
    mfx_EncParams.mfx.BufferSizeInKB =
        (pParams->bLookahead == true)
            ? static_cast<mfxU16>(
                  (mfx_EncParams.mfx.TargetKbps / static_cast<float>(8)) /
                  (static_cast<float>(
                       mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                   mfx_EncParams.mfx.FrameInfo.FrameRateExtD) *
                  (pParams->nLADepth +
                   (static_cast<float>(
                        mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                    mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
            : static_cast<mfxU16>((mfx_EncParams.mfx.TargetKbps / 8) * 1);
    if (pParams->bCustomBufferSize == true && pParams->nBufferSize > 0) {
      mfx_EncParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(pParams->nBufferSize);
      info("\tCustomBufferSize set: ON");
    }
    mfx_EncParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(mfx_EncParams.mfx.BufferSizeInKB / 2);
    info("\tBufferSize set to: %d KB", mfx_EncParams.mfx.BufferSizeInKB * 100);
    break;
  case MFX_RATECONTROL_CQP:
    mfx_EncParams.mfx.QPI = static_cast<mfxU16>(pParams->nQPI);
    mfx_EncParams.mfx.QPB = static_cast<mfxU16>(pParams->nQPB);
    mfx_EncParams.mfx.QPP = static_cast<mfxU16>(pParams->nQPP);
    break;
  case MFX_RATECONTROL_ICQ:
    mfx_EncParams.mfx.ICQQuality = static_cast<mfxU16>(pParams->nICQQuality);
    break;
  }

  mfx_EncParams.AsyncDepth = static_cast<mfxU16>(pParams->nAsyncDepth);

  mfx_EncParams.mfx.GopPicSize =
      (pParams->nKeyIntSec > 0)
          ? static_cast<mfxU16>(
                pParams->nKeyIntSec *
                mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
                static_cast<float>(mfx_EncParams.mfx.FrameInfo.FrameRateExtD))
          : 240;

  if ((!pParams->bAdaptiveI && !pParams->bAdaptiveB) ||
      (pParams->bAdaptiveI == false && pParams->bAdaptiveB == false)) {
    mfx_EncParams.mfx.GopOptFlag = MFX_GOP_STRICT;
    info("\tGopOptFlag set: STRICT");
  } else {
    mfx_EncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
    info("\tGopOptFlag set: CLOSED");
  }

  switch (codec) {
  case QSV_CODEC_HEVC:
    mfx_EncParams.mfx.IdrInterval = 1;
    mfx_EncParams.mfx.NumSlice = 0;
    break;
  default:
    mfx_EncParams.mfx.NumSlice = 1;
    break;
  }

  mfx_EncParams.mfx.GopRefDist = static_cast<mfxU16>(pParams->nGOPRefDist);

  if (codec == QSV_CODEC_AV1 && pParams->bLookahead == false &&
      pParams->bEncTools == true &&
      (mfx_EncParams.mfx.GopRefDist != 1 && mfx_EncParams.mfx.GopRefDist != 2 &&
       mfx_EncParams.mfx.GopRefDist != 4 && mfx_EncParams.mfx.GopRefDist != 8 &&
       mfx_EncParams.mfx.GopRefDist != 16)) {
    mfx_EncParams.mfx.GopRefDist = 8;
    blog(LOG_WARNING,
         "\tThe AV1 codec without Lookahead cannot be used with EncTools if "
         "GopRefDist does not "
         "match the values 1, 2, 4, 8, 16. GOPRefDist automaticaly set to 8.");
  }
  info("\tGOPRefDist set to: %d frames (%d b-frames)",
       (int)mfx_EncParams.mfx.GopRefDist,
       (int)(mfx_EncParams.mfx.GopRefDist == 0
                 ? 0
                 : mfx_EncParams.mfx.GopRefDist - 1));

  if (mfx_Ext_CO_enable == 1 && codec != QSV_CODEC_VP9) {
    auto CO = mfx_EncParams.AddExtBuffer<mfxExtCodingOption>();
    /*Don't touch it!*/
    CO->CAVLC = MFX_CODINGOPTION_OFF;
    CO->RefPicMarkRep = MFX_CODINGOPTION_ON;
    CO->PicTimingSEI = MFX_CODINGOPTION_ON;
    // CO->AUDelimiter = MFX_CODINGOPTION_OFF;

    CO->ResetRefList = MFX_CODINGOPTION_ON;
    CO->FieldOutput = MFX_CODINGOPTION_ON;
    CO->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    CO->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    CO->MVPrecision = MFX_MVPRECISION_QUARTERPEL;
    CO->MECostType = static_cast<mfxU16>(8);
    CO->MESearchType = static_cast<mfxU16>(16);
    CO->MVSearchWindow.x = (codec == QSV_CODEC_AVC) ? static_cast<mfxI16>(16)
                                                    : static_cast<mfxI16>(32);
    CO->MVSearchWindow.y = (codec == QSV_CODEC_AVC) ? static_cast<mfxI16>(16)
                                                    : static_cast<mfxI16>(32);

    if (pParams->bIntraRefEncoding == true) {
      CO->RecoveryPointSEI = MFX_CODINGOPTION_ON;
    }

    CO->RateDistortionOpt = GetCodingOpt(pParams->bRDO);
    info("\tRDO set: %s", GetCodingOptStatus(CO->RateDistortionOpt).c_str());

    CO->VuiVclHrdParameters = GetCodingOpt(pParams->bHRDConformance);
    CO->VuiNalHrdParameters = GetCodingOpt(pParams->bHRDConformance);
    CO->NalHrdConformance = GetCodingOpt(pParams->bHRDConformance);
    info("\tHRDConformance set: %s",
         GetCodingOptStatus(CO->NalHrdConformance).c_str());
  }

  if (mfx_Ext_CO2_enable == 1) {
    auto CO2 = mfx_EncParams.AddExtBuffer<mfxExtCodingOption2>();

    CO2->BufferingPeriodSEI = MFX_BPSEI_IFRAME;
    CO2->RepeatPPS = MFX_CODINGOPTION_OFF;
    CO2->FixedFrameRate = MFX_CODINGOPTION_ON;

    // if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
    //     mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
    //   CO2->MaxFrameSize = (mfx_EncParams.mfx.TargetKbps *
    //                        mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
    //                        (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
    //                              mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
    //                              *
    //                       10;
    // }

    CO2->ExtBRC = GetCodingOpt(pParams->bExtBRC);
    info("\tExtBRC set: %s", GetCodingOptStatus(CO2->ExtBRC).c_str());

    if (pParams->bIntraRefEncoding == true) {

      CO2->IntRefType = MFX_REFRESH_HORIZONTAL;

      CO2->IntRefCycleSize = static_cast<mfxU16>(
          pParams->nIntraRefCycleSize > 1
              ? pParams->nIntraRefCycleSize
              : (mfx_EncParams.mfx.GopRefDist > 1 ? mfx_EncParams.mfx.GopRefDist
                                                  : 2));
      info("\tIntraRefCycleSize set: %d", CO2->IntRefCycleSize);
      if (pParams->nIntraRefQPDelta > -52 && pParams->nIntraRefQPDelta < 52) {
        CO2->IntRefQPDelta = static_cast<mfxU16>(pParams->nIntraRefQPDelta);
        info("\tIntraRefQPDelta set: %d", CO2->IntRefQPDelta);
      }
    }

    // if (mfx_Platform.CodeName < MFX_PLATFORM_DG2 &&
    //	mfx_Platform.MediaAdapterType == MFX_MEDIA_INTEGRATED &&
    //	pParams->bLookahead == true && pParams->bLowpower == false) {
    //	pParams->bLookahead = false;
    //	pParams->nLADepth = 0;
    //	info("\tIntegrated graphics with Lowpower turned OFF does not "
    //		"\tsupport Lookahead");
    // }

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
      if (pParams->bLookahead == true) {
        CO2->LookAheadDepth = pParams->nLADepth;
        info("\tLookahead set to: ON");
        info("\tLookaheadDepth set to: %d", CO2->LookAheadDepth);
      }
    }

    if (codec != QSV_CODEC_VP9) {
      CO2->MBBRC = GetCodingOpt(pParams->bMBBRC);
      info("\tMBBRC set: %s", GetCodingOptStatus(CO2->MBBRC).c_str());
    }

    if (pParams->nGOPRefDist > 1) {
      CO2->BRefType = MFX_B_REF_PYRAMID;
      info("\tBPyramid set: ON");
    } else {
      CO2->BRefType = MFX_B_REF_UNKNOWN;
      info("\tBPyramid set: AUTO");
    }

    if (pParams->nTrellis.has_value()) {
      switch (pParams->nTrellis.value()) {
      case 0:
        CO2->Trellis = MFX_TRELLIS_OFF;
        info("\tTrellis set: OFF");
        break;
      case 1:
        CO2->Trellis = MFX_TRELLIS_I;
        info("\tTrellis set: I");
        break;
      case 2:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
        info("\tTrellis set: IP");
        break;
      case 3:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B;
        info("\tTrellis set: IPB");
        break;
      case 4:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
        info("\tTrellis set: IB");
        break;
      case 5:
        CO2->Trellis = MFX_TRELLIS_P;
        info("\tTrellis set: P");
        break;
      case 6:
        CO2->Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
        info("\tTrellis set: PB");
        break;
      case 7:
        CO2->Trellis = MFX_TRELLIS_B;
        info("\tTrellis set: B");
        break;
      default:
        info("\tTrellis set: AUTO");
        break;
      }
    }

    CO2->AdaptiveI = GetCodingOpt(pParams->bAdaptiveI);
    info("\tAdaptiveI set: %s", GetCodingOptStatus(CO2->AdaptiveI).c_str());

    CO2->AdaptiveB = GetCodingOpt(pParams->bAdaptiveB);
    info("\tAdaptiveB set: %s", GetCodingOptStatus(CO2->AdaptiveB).c_str());

    if (pParams->RateControl == MFX_RATECONTROL_CBR ||
        pParams->RateControl == MFX_RATECONTROL_VBR) {
      CO2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
      if (pParams->nLookAheadDS.has_value() == true) {
        switch (pParams->nLookAheadDS.value()) {
        case 0:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
          info("\tLookAheadDS set: SLOW");
          break;
        case 1:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
          info("\tLookAheadDS set: MEDIUM");
          break;
        case 2:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_4x;
          info("\tLookAheadDS set: FAST");
          break;
        default:
          info("\tLookAheadDS set: AUTO");
          break;
        }
      }
    }

    CO2->UseRawRef = GetCodingOpt(pParams->bRawRef);
    info("\tUseRawRef set: %s", GetCodingOptStatus(CO2->UseRawRef).c_str());
  }

  if (mfx_Ext_CO3_enable == 1) {
    auto CO3 = mfx_EncParams.AddExtBuffer<mfxExtCodingOption3>();
    CO3->TargetBitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;
    CO3->TargetBitDepthChroma = pParams->video_fmt_10bit ? 10 : 8;
    CO3->TargetChromaFormatPlus1 =
        static_cast<mfxU16>(mfx_EncParams.mfx.FrameInfo.ChromaFormat + 1);

    // if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
    //     mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
    //   CO3->MaxFrameSizeI = (mfx_EncParams.mfx.TargetKbps *
    //                         mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
    //                         (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
    //                               mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
    //                               *
    //                        8;
    //   CO3->MaxFrameSizeP = (mfx_EncParams.mfx.TargetKbps *
    //                         mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
    //                         (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
    //                               mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
    //                               *
    //                        5;
    // }

    CO3->MBDisableSkipMap = MFX_CODINGOPTION_ON;
    CO3->EnableQPOffset = MFX_CODINGOPTION_ON;

    CO3->BitstreamRestriction = MFX_CODINGOPTION_ON;
    CO3->AspectRatioInfoPresent = MFX_CODINGOPTION_ON;
    CO3->TimingInfoPresent = MFX_CODINGOPTION_ON;
    CO3->OverscanInfoPresent = MFX_CODINGOPTION_ON;

    CO3->LowDelayHrd = GetCodingOpt(pParams->bLowDelayHRD);
    info("\tLowDelayHRD set: %s", GetCodingOptStatus(CO3->LowDelayHrd).c_str());

    CO3->WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
    CO3->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;

    if (pParams->bIntraRefEncoding == true) {
      CO3->IntRefCycleDist = 0;
    }

    CO3->ContentInfo = MFX_CONTENT_NOISY_VIDEO;

    if (pParams->bLookahead == true && pParams->bLookaheadLP == true) {
      CO3->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
      info("\tScenario: REMOTE GAMING");
    } else if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_AV1) &&
               pParams->bLookahead == true) {
      CO3->ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
      info("\tScenario: GAME STREAMING");
    } else if (pParams->bLookahead == false ||
               (codec == QSV_CODEC_HEVC && pParams->bLookahead == true)) {
      CO3->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
      info("\tScenario: REMOTE GAMING");
    }

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
      CO3->EnableMBQP = MFX_CODINGOPTION_ON;
    }

    if (codec == QSV_CODEC_HEVC) {
      CO3->GPB = GetCodingOpt(pParams->bGPB);
      info("\tGPB set: %s", GetCodingOptStatus(CO3->GPB).c_str());
    }

    if (pParams->bPPyramid == true) {
      CO3->PRefType = MFX_P_REF_PYRAMID;
      info("\tPPyramid set: PYRAMID");
    } else {
      CO3->PRefType = MFX_P_REF_SIMPLE;
      info("\tPPyramid set: SIMPLE");
    }

    CO3->AdaptiveCQM = GetCodingOpt(pParams->bAdaptiveCQM);
    info("\tAdaptiveCQM set: %s", GetCodingOptStatus(CO3->AdaptiveCQM).c_str());

    CO3->AdaptiveRef = GetCodingOpt(pParams->bAdaptiveRef);
    info("\tAdaptiveRef set: %s", GetCodingOptStatus(CO3->AdaptiveRef).c_str());

    CO3->AdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
    if (pParams->bExtBRC == true && codec == QSV_CODEC_AVC) {
      CO3->ExtBrcAdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
    }
    info("\tAdaptiveLTR set: %s", GetCodingOptStatus(CO3->AdaptiveLTR).c_str());

    if (pParams->nWinBRCMaxAvgSize > 0) {
      CO3->WinBRCMaxAvgKbps = static_cast<mfxU16>(pParams->nWinBRCMaxAvgSize);
      info("\tWinBRCMaxSize set: %d", CO3->WinBRCMaxAvgKbps);
    }

    if (pParams->nWinBRCSize > 0) {
      CO3->WinBRCSize = static_cast<mfxU16>(pParams->nWinBRCSize);
      info("\tWinBRCSize set: %d", CO3->WinBRCSize);
    }

    CO3->MotionVectorsOverPicBoundaries =
        GetCodingOpt(pParams->nMotionVectorsOverPicBoundaries);
    info("\tMotionVectorsOverPicBoundaries set: %s",
         GetCodingOptStatus(CO3->MotionVectorsOverPicBoundaries).c_str());

    if (pParams->bGlobalMotionBiasAdjustment.has_value() &&
        pParams->bGlobalMotionBiasAdjustment.value() == true) {
      CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_ON;
      info("\tGlobalMotionBiasAdjustment set: ON");
      if (pParams->nMVCostScalingFactor.has_value()) {
        switch (pParams->nMVCostScalingFactor.value()) {
        case 1:
          CO3->MVCostScalingFactor = 1;
          info("\tMVCostScalingFactor set: 1/2");
          break;
        case 2:
          CO3->MVCostScalingFactor = 2;
          info("\tMVCostScalingFactor set: 1/4");
          break;
        case 3:
          CO3->MVCostScalingFactor = 3;
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
      CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
    }

    CO3->DirectBiasAdjustment = GetCodingOpt(pParams->bDirectBiasAdjustment);
    info("\tDirectBiasAdjustment set: %s",
         GetCodingOptStatus(CO3->DirectBiasAdjustment).c_str());
  }

#if defined(_WIN32) || defined(_WIN64)
  if (codec != QSV_CODEC_VP9 && pParams->bEncTools == true) {
    auto EncToolsParam = mfx_EncParams.AddExtBuffer<mfxExtEncToolsConfig>();
    switch (codec) {
    case QSV_CODEC_AVC:
      EncToolsParam->AdaptiveLTR = static_cast<mfxU16>(MFX_CODINGOPTION_OFF);
      EncToolsParam->BRC =
          static_cast<mfxU16>((pParams->RateControl == MFX_RATECONTROL_CBR ||
                               pParams->RateControl == MFX_RATECONTROL_VBR)
                                  ? MFX_CODINGOPTION_ON
                                  : 0);
      EncToolsParam->AdaptiveLTR =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveLTR));
      EncToolsParam->AdaptiveRefB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptiveRefP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptivePyramidQuantB = static_cast<mfxU16>(
          (pParams->nGOPRefDist > 1) ? MFX_CODINGOPTION_ON
                                     : MFX_CODINGOPTION_OFF);
      EncToolsParam->AdaptivePyramidQuantP = static_cast<mfxU16>(
          (pParams->bPPyramid == true) ? MFX_CODINGOPTION_ON
                                       : MFX_CODINGOPTION_OFF);
      EncToolsParam->BRCBufferHints =
          static_cast<mfxU16>((pParams->RateControl == MFX_RATECONTROL_CBR ||
                               pParams->RateControl == MFX_RATECONTROL_VBR)
                                  ? MFX_CODINGOPTION_ON
                                  : 0);
      EncToolsParam->SceneChange = static_cast<mfxU16>(MFX_CODINGOPTION_ON);
      EncToolsParam->AdaptiveMBQP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bMBBRC));
      EncToolsParam->AdaptiveQuantMatrices =
          static_cast<mfxU16>(MFX_CODINGOPTION_ON);
      EncToolsParam->AdaptiveB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveB));
      EncToolsParam->AdaptiveI =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveI));
      EncToolsParam->SaliencyMapHint =
          static_cast<mfxU16>(MFX_CODINGOPTION_OFF);

      if (pParams->bLookahead == true) {
        EncToolsParam->BRC = static_cast<mfxU16>(MFX_CODINGOPTION_OFF);
      }
      break;
    case QSV_CODEC_AV1:
      EncToolsParam->BRC =
          static_cast<mfxU16>((pParams->RateControl == MFX_RATECONTROL_CBR ||
                               pParams->RateControl == MFX_RATECONTROL_VBR)
                                  ? MFX_CODINGOPTION_ON
                                  : MFX_CODINGOPTION_OFF);
      EncToolsParam->AdaptiveLTR = static_cast<mfxU16>(MFX_CODINGOPTION_OFF);
      EncToolsParam->AdaptiveRefB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptiveRefP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptivePyramidQuantB = static_cast<mfxU16>(
          (pParams->nGOPRefDist > 1) ? MFX_CODINGOPTION_ON
                                     : MFX_CODINGOPTION_OFF);
      EncToolsParam->AdaptivePyramidQuantP = static_cast<mfxU16>(
          (pParams->bPPyramid == true) ? MFX_CODINGOPTION_ON
                                       : MFX_CODINGOPTION_OFF);
      EncToolsParam->BRCBufferHints = static_cast<mfxU16>(EncToolsParam->BRC);
      EncToolsParam->SceneChange = static_cast<mfxU16>(MFX_CODINGOPTION_ON);
      EncToolsParam->AdaptiveMBQP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bMBBRC));
      EncToolsParam->AdaptiveQuantMatrices =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveCQM));
      EncToolsParam->AdaptiveB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveB));
      EncToolsParam->AdaptiveI =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveI));
      EncToolsParam->SaliencyMapHint = static_cast<mfxU16>(MFX_CODINGOPTION_ON);
      break;
    case QSV_CODEC_HEVC:
      EncToolsParam->BRC =
          static_cast<mfxU16>((pParams->RateControl == MFX_RATECONTROL_CBR ||
                               pParams->RateControl == MFX_RATECONTROL_VBR)
                                  ? MFX_CODINGOPTION_ON
                                  : 0);
      EncToolsParam->AdaptiveLTR =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveLTR));
      EncToolsParam->AdaptiveRefB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptiveRefP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveRef));
      EncToolsParam->AdaptivePyramidQuantB = static_cast<mfxU16>(
          (pParams->nGOPRefDist > 1) ? MFX_CODINGOPTION_ON
                                     : MFX_CODINGOPTION_OFF);
      EncToolsParam->AdaptivePyramidQuantP = static_cast<mfxU16>(
          (pParams->bPPyramid == true) ? MFX_CODINGOPTION_ON
                                       : MFX_CODINGOPTION_OFF);
      EncToolsParam->BRCBufferHints =
          static_cast<mfxU16>((pParams->RateControl == MFX_RATECONTROL_CBR ||
                               pParams->RateControl == MFX_RATECONTROL_VBR)
                                  ? MFX_CODINGOPTION_ON
                                  : 0);
      EncToolsParam->SceneChange = static_cast<mfxU16>(MFX_CODINGOPTION_ON);
      EncToolsParam->AdaptiveMBQP =
          static_cast<mfxU16>(GetCodingOpt(pParams->bMBBRC));
      EncToolsParam->AdaptiveQuantMatrices =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveCQM));
      EncToolsParam->AdaptiveB =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveB));
      EncToolsParam->AdaptiveI =
          static_cast<mfxU16>(GetCodingOpt(pParams->bAdaptiveI));
      EncToolsParam->SaliencyMapHint = static_cast<mfxU16>(MFX_CODINGOPTION_ON);

      if (pParams->bLookahead == true) {
        EncToolsParam->BRC = 0;
        EncToolsParam->AdaptiveLTR = 0;
        EncToolsParam->AdaptiveRefB = 0;
        EncToolsParam->AdaptiveRefP = 0;
        EncToolsParam->AdaptivePyramidQuantB = 0;
        EncToolsParam->AdaptivePyramidQuantP = 0;
        EncToolsParam->BRCBufferHints = 0;
        EncToolsParam->SceneChange = 0;
        EncToolsParam->AdaptiveQuantMatrices = 0;
        EncToolsParam->AdaptiveB = 0;
        EncToolsParam->AdaptiveI = 0;
      }
      break;
    }
    info("\tEncTools set: ON");
  }

  /*Don't touch it! Magic beyond the control of mere mortals takes place
   * here*/
  if (mfx_Ext_CO_DDI_enable == 1 && codec != QSV_CODEC_AV1) {
    auto CODDIParam = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionDDI>();
    CODDIParam->WriteIVFHeaders = MFX_CODINGOPTION_OFF;
    CODDIParam->IBC = MFX_CODINGOPTION_ON;
    CODDIParam->BRCPrecision = 3;
    CODDIParam->BiDirSearch = MFX_CODINGOPTION_ON;
    CODDIParam->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
    CODDIParam->GlobalSearch = 1;
    CODDIParam->IntraPredCostType = 8;
    CODDIParam->MEFractionalSearchType = 16;
    CODDIParam->MVPrediction = MFX_CODINGOPTION_ON;
    // CODDIParam->MaxMVs =
    //     static_cast<mfxU16>((pParams->nWidth * pParams->nHeight) /* / 4*/);
    CODDIParam->WeightedBiPredIdc = 2;
    CODDIParam->WeightedPrediction = MFX_CODINGOPTION_ON;
    CODDIParam->FieldPrediction = MFX_CODINGOPTION_ON;
    // CO_DDI->CabacInitIdcPlus1 = (mfxU16)1;
    CODDIParam->DirectCheck = MFX_CODINGOPTION_ON;
    CODDIParam->FractionalQP = 1;
    CODDIParam->Hme = MFX_CODINGOPTION_ON;
    CODDIParam->LocalSearch = 6;
    CODDIParam->MBAFF = MFX_CODINGOPTION_ON;
    CODDIParam->DDI.InterPredBlockSize = 64;
    CODDIParam->DDI.IntraPredBlockSize = 1;
    CODDIParam->RefOppositeField = MFX_CODINGOPTION_ON;
    CODDIParam->RefRaw = GetCodingOpt(pParams->bRawRef);
    CODDIParam->TMVP = MFX_CODINGOPTION_ON;
    CODDIParam->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDIParam->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDIParam->QpAdjust = MFX_CODINGOPTION_ON;
    CODDIParam->Transform8x8Mode = MFX_CODINGOPTION_ON;
    CODDIParam->EarlySkip = 0;
    CODDIParam->RefreshFrameContext = MFX_CODINGOPTION_ON;
    CODDIParam->ChangeFrameContextIdxForTS = MFX_CODINGOPTION_ON;
    CODDIParam->SuperFrameForTS = MFX_CODINGOPTION_ON;
  }
#endif

  if (codec == QSV_CODEC_HEVC) {

    auto ChromaLocParam = mfx_EncParams.AddExtBuffer<mfxExtChromaLocInfo>();
    ChromaLocParam->ChromaLocInfoPresentFlag = 1;
    ChromaLocParam->ChromaSampleLocTypeTopField =
        static_cast<mfxU16>(pParams->ChromaSampleLocTypeTopField);
    ChromaLocParam->ChromaSampleLocTypeBottomField =
        static_cast<mfxU16>(pParams->ChromaSampleLocTypeBottomField);

    auto HevcParam = mfx_EncParams.AddExtBuffer<mfxExtHEVCParam>();
    HevcParam->PicWidthInLumaSamples = mfx_EncParams.mfx.FrameInfo.Width;
    HevcParam->PicHeightInLumaSamples = mfx_EncParams.mfx.FrameInfo.Height;

    if (pParams->nSAO.has_value()) {
      switch (pParams->nSAO.value()) {
      case 0:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_DISABLE;
        info("\tSAO set: DISABLE");
        break;
      case 1:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_LUMA;
        info("\tSAO set: LUMA");
        break;
      case 2:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_CHROMA;
        info("\tSAO set: CHROMA");
        break;
      case 3:
        HevcParam->SampleAdaptiveOffset =
            MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA;
        info("\tSAO set: ALL");
        break;
      }
    } else {
      info("\tSAO set: AUTO");
    }

    auto HevcTilesParam = mfx_EncParams.AddExtBuffer<mfxExtHEVCTiles>();
    HevcTilesParam->NumTileColumns = 1;
    HevcTilesParam->NumTileRows = 1;
  }

  if (codec == QSV_CODEC_AV1) {
    auto AV1BitstreamParam =
        mfx_EncParams.AddExtBuffer<mfxExtAV1BitstreamParam>();
    AV1BitstreamParam->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    auto AV1TileParam = mfx_EncParams.AddExtBuffer<mfxExtAV1TileParam>();
    AV1TileParam->NumTileGroups = 1;
    if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
      AV1TileParam->NumTileColumns = 2;
      AV1TileParam->NumTileRows = 2;
    } else {
      AV1TileParam->NumTileColumns = 1;
      AV1TileParam->NumTileRows = 1;
    }

    if (pParams->nTuneQualityMode.has_value()) {
      auto TuneQualityParam =
          mfx_EncParams.AddExtBuffer<mfxExtTuneEncodeQuality>();

      switch ((int)pParams->nTuneQualityMode.value()) {
      default:
      case 0:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_OFF;
        info("\tTuneQualityMode set: DEFAULT");
        break;
      case 1:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_PSNR;
        info("\tTuneQualityMode set: PSNR");
        break;
      case 2:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_SSIM;
        info("\tTuneQualityMode set: SSIM");
        break;
      case 3:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
        info("\tTuneQualityMode set: MS SSIM");
        break;
      case 4:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_VMAF;
        info("\tTuneQualityMode set: VMAF");
        break;
      case 5:
        TuneQualityParam->TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
        info("\tTuneQualityMode set: PERCEPTUAL");
        break;
      }
    } else {
      info("\tTuneQualityMode set: OFF");
    }
  }

  if (codec == QSV_CODEC_VP9) {
    auto VP9Param = mfx_EncParams.AddExtBuffer<mfxExtVP9Param>();

    VP9Param->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
      VP9Param->NumTileColumns = 2;
      VP9Param->NumTileRows = 2;
    } else {
      VP9Param->NumTileColumns = 1;
      VP9Param->NumTileRows = 1;
    }

  }
#if defined(_WIN32) || defined(_WIN64)
  else {
    auto VideoSignalParam = mfx_EncParams.AddExtBuffer<mfxExtVideoSignalInfo>();
    VideoSignalParam->VideoFormat = static_cast<mfxU16>(pParams->VideoFormat);
    VideoSignalParam->VideoFullRange =
        static_cast<mfxU16>(pParams->VideoFullRange);
    VideoSignalParam->ColourDescriptionPresent = 1;
    VideoSignalParam->ColourPrimaries =
        static_cast<mfxU16>(pParams->ColourPrimaries);
    VideoSignalParam->TransferCharacteristics =
        static_cast<mfxU16>(pParams->TransferCharacteristics);
    VideoSignalParam->MatrixCoefficients =
        static_cast<mfxU16>(pParams->MatrixCoefficients);
  }
#endif

  if (pParams->MaxContentLightLevel > 0) {
    auto ColourVolumeParam =
        mfx_EncParams.AddExtBuffer<mfxExtMasteringDisplayColourVolume>();
    ColourVolumeParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ColourVolumeParam->DisplayPrimariesX[0] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[0]);
    ColourVolumeParam->DisplayPrimariesX[1] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[1]);
    ColourVolumeParam->DisplayPrimariesX[2] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[2]);
    ColourVolumeParam->DisplayPrimariesY[0] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[0]);
    ColourVolumeParam->DisplayPrimariesY[1] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[1]);
    ColourVolumeParam->DisplayPrimariesY[2] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[2]);
    ColourVolumeParam->WhitePointX = static_cast<mfxU16>(pParams->WhitePointX);
    ColourVolumeParam->WhitePointY = static_cast<mfxU16>(pParams->WhitePointY);
    ColourVolumeParam->MaxDisplayMasteringLuminance =
        static_cast<mfxU32>(pParams->MaxDisplayMasteringLuminance);
    ColourVolumeParam->MinDisplayMasteringLuminance =
        static_cast<mfxU32>(pParams->MinDisplayMasteringLuminance);

    auto ContentLightLevelParam =
        mfx_EncParams.AddExtBuffer<mfxExtContentLightLevelInfo>();
    ContentLightLevelParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ContentLightLevelParam->MaxContentLightLevel =
        static_cast<mfxU16>(pParams->MaxContentLightLevel);
    ContentLightLevelParam->MaxPicAverageLightLevel =
        static_cast<mfxU16>(pParams->MaxPicAverageLightLevel);
  }

  mfx_EncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  info("\tFeature extended buffer size: %d", mfx_EncParams.NumExtParam);

  // We dont check what was valid or invalid here, just try changing lower
  // power. Ensure set values are not overwritten so in case it wasnt lower
  // power we fail during the parameter check.
  mfxVideoParam validParams = {};
  memcpy(&validParams, &mfx_EncParams, sizeof(mfxVideoParam));
  mfxStatus sts = mfx_VideoEnc->Query(&mfx_EncParams, &validParams);
  if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
    auto CO3 = mfx_EncParams.GetExtBuffer<mfxExtCodingOption3>();
    if (CO3->AdaptiveLTR == MFX_CODINGOPTION_ON) {
      CO3->AdaptiveLTR = MFX_CODINGOPTION_OFF;
    }
  } else if (sts < MFX_ERR_NONE) {
    throw std::runtime_error("InitEncParams(): Query params error");
  }

  return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(struct qsv_param_t *pParams) {
  ResetParamChanged = false;

  mfxStatus sts = mfx_VideoEnc->GetVideoParam(&mfx_ResetParams);
  if (sts < MFX_ERR_NONE) {
    return false;
  }

  mfx_ResetParams.NumExtParam = 0;
  switch (pParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    if (mfx_ResetParams.mfx.TargetKbps != pParams->nTargetBitRate) {
      mfx_ResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(pParams->nTargetBitRate);
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_VBR:
    if (mfx_ResetParams.mfx.TargetKbps != pParams->nTargetBitRate) {
      mfx_ResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(pParams->nTargetBitRate);
      ResetParamChanged = true;
    }
    if (mfx_ResetParams.mfx.MaxKbps != pParams->nMaxBitRate) {
      mfx_ResetParams.mfx.MaxKbps = static_cast<mfxU16>(pParams->nMaxBitRate);
      ResetParamChanged = true;
    }
    if (mfx_ResetParams.mfx.MaxKbps < mfx_ResetParams.mfx.TargetKbps) {
      mfx_ResetParams.mfx.MaxKbps = mfx_ResetParams.mfx.TargetKbps;
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_CQP:
    if (mfx_ResetParams.mfx.QPI != pParams->nQPI) {
      mfx_ResetParams.mfx.QPI = static_cast<mfxU16>(pParams->nQPI);
      mfx_ResetParams.mfx.QPB = static_cast<mfxU16>(pParams->nQPB);
      mfx_ResetParams.mfx.QPP = static_cast<mfxU16>(pParams->nQPP);
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_ICQ:
    if (mfx_ResetParams.mfx.ICQQuality != pParams->nICQQuality) {
      mfx_ResetParams.mfx.ICQQuality =
          static_cast<mfxU16>(pParams->nICQQuality);
      ResetParamChanged = true;
    }
  }
  if (ResetParamChanged == true) {
    auto ResetParams = mfx_EncParams.AddExtBuffer<mfxExtEncoderResetOption>();
    ResetParams->StartNewSequence = MFX_CODINGOPTION_ON;
    mfx_VideoEnc->Query(&mfx_ResetParams, &mfx_ResetParams);
    return true;
  } else {
    return false;
  }
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder() {
  if (ResetParamChanged == true) {
    return mfx_VideoEnc->Reset(&mfx_ResetParams);
  } else {
    return MFX_ERR_NONE;
  }
}

mfxStatus QSV_VPL_Encoder_Internal::AllocateTextures() {
  mfxStatus sts = MFX_ERR_NONE;

  if (mfx_UseTexAlloc) {
    // Query number of required surfaces for encoder
    sts = mfx_VideoEnc->QueryIOSurf(&mfx_EncParams, &mfx_AllocRequest);
    if (sts != MFX_ERR_NONE) {
      error("Error code: %d", sts);
      throw std::runtime_error("AllocateTextures(): QueryIOSurf error");
    }

    mfx_AllocRequest.NumFrameSuggested +=
        static_cast<mfxU16>(static_cast<mfxU32>(mfx_EncParams.AsyncDepth) +
                            mfx_EncParams.mfx.FrameInfo.FrameRateExtN);
    // Allocate textures
    try {
      sts = hw->allocate_tex(&mfx_AllocRequest);
    } catch (const std::exception &) {
      error("Error code: %d", sts);
      throw;
    }

    sts = MFXGetMemoryInterface(mfx_Session, &mfx_MemoryInterface);
    if (sts < MFX_ERR_NONE) {
      throw std::runtime_error(
          "AllocateTextures(): MFXGetMemoryInterface error");
    }
  }

  return sts;
}

mfxStatus
QSV_VPL_Encoder_Internal::InitBitstream([[maybe_unused]] enum qsv_codec codec) {
  try {
    mfx_Bitstream.MaxLength =
        static_cast<mfxU32>(mfx_EncParams.mfx.BufferSizeInKB * 1000 * 10);
    mfx_Bitstream.DataOffset = 0;
    ;
    mfx_Bitstream.DataLength = 0;
#if defined(_WIN32) || defined(_WIN64)
    if (nullptr == (mfx_Bitstream.Data = static_cast<mfxU8 *>(
                        _aligned_malloc(mfx_Bitstream.MaxLength, 32)))) {
      throw std::runtime_error(
          "InitBitstream(): Bitstream memory allocation error");
    }
#elif defined(__linux__)
    if (nullptr == (mfx_Bitstream.Data = static_cast<mfxU8 *>(
                        aligned_alloc(32, mfx_Bitstream.MaxLength)))) {
      throw std::runtime_error(
          "InitBitstream(): Bitstream memory allocation error");
    }
#endif

    info("\tBitstream size: %d Kb", mfx_Bitstream.MaxLength / 1000);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }
  return MFX_ERR_NONE;
}

mfxStatus
QSV_VPL_Encoder_Internal::InitTaskPool([[maybe_unused]] enum qsv_codec codec) {
  try {
    mfx_TaskPoolSize = mfx_EncParams.AsyncDepth;
    mfx_SyncTaskID = 0;

    mfx_TaskPool = new Task[mfx_TaskPoolSize];
    memset(mfx_TaskPool, 0, sizeof(Task) * mfx_TaskPoolSize);

    for (int i = 0; i < mfx_TaskPoolSize; i++) {
      mfx_TaskPool[i].mfxBS.MaxLength =
          static_cast<mfxU32>(mfx_EncParams.mfx.BufferSizeInKB * 1000 * 10);
      mfx_TaskPool[i].mfxBS.DataOffset = 0;
      mfx_TaskPool[i].mfxBS.DataLength = 0;
#if defined(_WIN32) || defined(_WIN64)
      if (nullptr ==
          (mfx_TaskPool[i].mfxBS.Data = static_cast<mfxU8 *>(
               _aligned_malloc(mfx_TaskPool[i].mfxBS.MaxLength, 32)))) {
        throw std::runtime_error(
            "InitTaskPool(): Task memory allocation error");
      }
#elif defined(__linux__)
      if (nullptr ==
          (mfx_TaskPool[i].mfxBS.Data = static_cast<mfxU8 *>(
               aligned_alloc(32, mfx_TaskPool[i].mfxBS.MaxLength)))) {
        throw std::runtime_error(
            "InitTaskPool(): Task memory allocation error");
      }
#endif

      info("\tTask #%d bitstream size: %d", i,
           mfx_TaskPool[i].mfxBS.MaxLength / 1000);
    }

    info("\tTaskPool count: %d", mfx_TaskPoolSize);

  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }

  return MFX_ERR_NONE;
}

void QSV_VPL_Encoder_Internal::ReleaseBitstream() {
  if (mfx_Bitstream.Data) {
    try {
#if defined(_WIN32) || defined(_WIN64)
      _aligned_free(mfx_Bitstream.Data);
#elif defined(__linux__)
      free(mfx_Bitstream.Data);
#endif
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
  mfx_Bitstream.Data = nullptr;
}

void QSV_VPL_Encoder_Internal::ReleaseTask(int TaskID) {
  if (mfx_TaskPool[TaskID].mfxBS.Data) {
    try {
#if defined(_WIN32) || defined(_WIN64)
      _aligned_free(mfx_TaskPool[TaskID].mfxBS.Data);
#elif defined(__linux__)
      free(mfx_TaskPool[TaskID].mfxBS.Data);
#endif
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
  mfx_TaskPool[TaskID].mfxBS.Data = nullptr;
}

void QSV_VPL_Encoder_Internal::ReleaseTaskPool() {
  if (mfx_TaskPool != nullptr && mfx_TaskPoolSize > 0) {
    try {
      for (int i = 0; i < mfx_TaskPoolSize; i++) {
        ReleaseTask(i);
      }
      delete[] mfx_TaskPool;
      mfx_TaskPool = nullptr;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (const std::exception &) {
      throw;
    }
  }
}

mfxStatus QSV_VPL_Encoder_Internal::ChangeBitstreamSize(mfxU32 NewSize) {
  try {
#if defined(_WIN32) || defined(_WIN64)
    mfxU8 *pData = static_cast<mfxU8 *>(_aligned_malloc(NewSize, 32));
#elif defined(__linux__)
    mfxU8 *pData = static_cast<mfxU8 *>(aligned_alloc(32, NewSize));
#endif
    if (pData == nullptr) {
      throw std::runtime_error(
          "ChangeBitstreamSize(): Bitstream memory allocation error");
    }

    mfxU32 nDataLen = mfx_Bitstream.DataLength;
    if (mfx_Bitstream.DataLength) {
      memcpy(pData, mfx_Bitstream.Data + mfx_Bitstream.DataOffset,
             std::min(nDataLen, NewSize));
    }
    ReleaseBitstream();

    mfx_Bitstream.Data = pData;
    mfx_Bitstream.DataOffset = 0;
    mfx_Bitstream.DataLength = static_cast<mfxU32>(nDataLen);
    mfx_Bitstream.MaxLength = static_cast<mfxU32>(NewSize);

    for (int i = 0; i < mfx_TaskPoolSize; i++) {
#if defined(_WIN32) || defined(_WIN64)
      mfxU8 *pTaskData = static_cast<mfxU8 *>(_aligned_malloc(NewSize, 32));
#elif defined(__linux__)
      mfxU8 *pTaskData = static_cast<mfxU8 *>(aligned_alloc(32, NewSize));
#endif
      if (pTaskData == nullptr) {
        throw std::runtime_error(
            "ChangeBitstreamSize(): Task memory allocation error");
      }

      mfxU32 nTaskDataLen = mfx_TaskPool[i].mfxBS.DataLength;
      if (mfx_TaskPool[i].mfxBS.DataLength) {
        memcpy(pTaskData,
               mfx_TaskPool[i].mfxBS.Data + mfx_TaskPool[i].mfxBS.DataOffset,
               std::min(nTaskDataLen, NewSize));
      }
      ReleaseTask(i);

      mfx_TaskPool[i].mfxBS.Data = pTaskData;
      mfx_TaskPool[i].mfxBS.DataOffset = 0;
      mfx_TaskPool[i].mfxBS.DataLength = static_cast<mfxU32>(nTaskDataLen);
      mfx_TaskPool[i].mfxBS.MaxLength = static_cast<mfxU32>(NewSize);
    }

  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    throw;
  }

  return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec) {
  if (codec != QSV_CODEC_VP9) {
    auto SPSPPSParams = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionSPSPPS>();
    SPSPPSParams->SPSBuffer = SPS_Buffer;
    SPSPPSParams->PPSBuffer = PPS_Buffer;
    SPSPPSParams->SPSBufSize = 1024;
    SPSPPSParams->PPSBufSize = 1024;
  }
  if (codec == QSV_CODEC_HEVC) {
    auto VPSParams = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionVPS>();

    VPSParams->VPSBuffer = VPS_Buffer;
    VPSParams->VPSBufSize = 1024;
  }

  mfxStatus sts = mfx_VideoEnc->GetVideoParam(&mfx_EncParams);

  info("\tGetVideoParam status:     %d", sts);
  if (sts < MFX_ERR_NONE) {
    error("Error code: %d", sts);
    throw std::runtime_error("GetVideoParam(): Get video parameters error");
  }

  if (codec == QSV_CODEC_HEVC) {
    auto VPSParams = mfx_EncParams.GetExtBuffer<mfxExtCodingOptionVPS>();
    VPS_BufferSize = VPSParams->VPSBufSize;
  }

  if (codec != QSV_CODEC_VP9) {
    auto SPSPPSParams = mfx_EncParams.GetExtBuffer<mfxExtCodingOptionSPSPPS>();
    SPS_BufferSize = SPSPPSParams->SPSBufSize;
    PPS_BufferSize = SPSPPSParams->PPSBufSize;
  }

  if (codec == QSV_CODEC_AV1) {
    mfx_EncParams.mfx.BufferSizeInKB *= 25;
  } else if (codec == QSV_CODEC_HEVC) {
    mfx_EncParams.mfx.BufferSizeInKB *= 100;
  }

  return sts;
}

void QSV_VPL_Encoder_Internal::GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
                                            mfxU8 **pPPSBuf, mfxU16 *pnVPSBuf,
                                            mfxU16 *pnSPSBuf,
                                            mfxU16 *pnPPSBuf) {
  *pVPSBuf = VPS_Buffer;
  *pnVPSBuf = VPS_BufferSize;

  *pSPSBuf = SPS_Buffer;
  *pnSPSBuf = SPS_BufferSize;

  *pPPSBuf = PPS_Buffer;
  *pnPPSBuf = PPS_BufferSize;
}

void QSV_VPL_Encoder_Internal::LoadFrameData(mfxFrameSurface1 *&surface,
                                             uint8_t **frame_data,
                                             uint32_t *frame_linesize) {
  mfxU16 w, h, i, pitch;
  mfxU8 *ptr;
  const mfxFrameInfo *surface_info = &surface->Info;
  const mfxFrameData *surface_data = &surface->Data;

  if (surface_info->CropH > 0 && surface_info->CropW > 0) {
    w = surface_info->CropW;
    h = surface_info->CropH;
  } else {
    w = surface_info->Width;
    h = surface_info->Height;
  }
  pitch = surface_data->Pitch;

  if (surface->Info.FourCC == MFX_FOURCC_NV12) {
    ptr = static_cast<mfxU8 *>(surface_data->Y + surface_info->CropX +
                               surface_info->CropY * pitch);

    // load Y plane
    for (i = 0; i < h; i++) {
      avx2_memcpy(ptr + i * pitch, frame_data[0] + i * frame_linesize[0], w);
    }

    // load UV plane
    h /= 2;
    ptr = static_cast<mfxU8 *>((surface_data->UV + surface_info->CropX +
                                (surface_info->CropY / 2) * pitch));

    for (i = 0; i < h; i++) {
      avx2_memcpy(ptr + i * pitch, frame_data[1] + i * frame_linesize[1], w);
    }
  } else if (surface->Info.FourCC == MFX_FOURCC_P010) {
    ptr = static_cast<mfxU8 *>(surface_data->Y + surface_info->CropX +
                               surface_info->CropY * pitch);
    const size_t line_size = static_cast<size_t>(w) * 2;
    // load Y plane
    for (i = 0; i < h; i++) {
      avx2_memcpy(ptr + i * pitch, frame_data[0] + i * frame_linesize[0],
                  line_size);
    }
    // load UV plane
    h /= 2;
    ptr = static_cast<mfxU8 *>((surface_data->UV + surface_info->CropX +
                                (surface_info->CropY / 2) * pitch));
    for (i = 0; i < h; i++) {
      avx2_memcpy(ptr + i * pitch, frame_data[1] + i * frame_linesize[1],
                  line_size);
    }
  } else if (surface->Info.FourCC == MFX_FOURCC_RGB4) {
    const size_t line_size = static_cast<size_t>(w) * 4;
    // load B plane
    for (i = 0; i < h; i++) {
      avx2_memcpy(surface_data->B + i * pitch,
                  frame_data[0] + i * frame_linesize[0], line_size);
    }
  }
}

/*It is necessary to solve the problem of incompatibility of ExtBRC and
  EncTools with ROI*/
void QSV_VPL_Encoder_Internal::AddROI(mfxU32 left, mfxU32 top, mfxU32 right,
                                      mfxU32 bottom, mfxI16 delta) {
  // auto ROIParams = mfx_EncCtrlParams.AddExtBuffer<mfxExtEncoderROI>();

  // if (ROIParams->NumROI == 256) {
  //   warn( "Maximum number of ROIs hit, ignoring additional
  //   ROI!"); return;
  // }
  //// auto ROIParams = mfx_EncCtrlParams.AddExtBuffer<mfxExtEncoderROI>();
  // ROIParams->ROIMode = MFX_ROI_MODE_QP_DELTA;
  ///* The SDK will automatically align the values to block sizes so we
  // * don't have to do any maths here. */
  // ROIParams->ROI[ROIParams->NumROI].Left =
  //    static_cast<mfxU32>((((left + 15) >> 4) << 4));
  // ROIParams->ROI[ROIParams->NumROI].Top =
  //    static_cast<mfxU32>((((top + 15) >> 4) << 4));
  // ROIParams->ROI[ROIParams->NumROI].Right =
  //    static_cast<mfxU32>((((right + 15) >> 4) << 4));
  // ROIParams->ROI[ROIParams->NumROI].Bottom =
  //    static_cast<mfxU32>((((bottom + 15) >> 4) << 4));
  // ROIParams->ROI[ROIParams->NumROI].DeltaQP = (mfxI16)delta;
  // ROIParams->NumROI++;
}

void QSV_VPL_Encoder_Internal::ClearROI() {
  // mfx_EncCtrlParams.RemoveExtBuffer<mfxExtEncoderROI>();
}

mfxStatus QSV_VPL_Encoder_Internal::GetFreeTaskIndex(int *TaskID) {
  if (mfx_TaskPool != nullptr) {
    for (int i = 0; i < mfx_TaskPoolSize; i++) {
      if (static_cast<mfxSyncPoint>(nullptr) == mfx_TaskPool[i].syncp) {
        mfx_SyncTaskID = (i + 1) % static_cast<int>(mfx_TaskPoolSize);
        *TaskID = i;
        return MFX_ERR_NONE;
      }
    }
  }
  return MFX_ERR_NOT_FOUND;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode_tex(mfxU64 ts, void *tex_handle,
                                               uint64_t lock_key,
                                               uint64_t *next_key,
                                               mfxBitstream **pBS) {
  mfxStatus sts = MFX_ERR_NONE, sync_sts = MFX_ERR_NONE,
            refcounter_sts = MFX_ERR_NONE;
  *pBS = nullptr;
  int nTaskIdx = 0;
  mfxU32 CurrentRefCount = 0;
  if (mfx_EncSurface != nullptr) {
    refcounter_sts = mfx_EncSurface->FrameInterface->GetRefCounter(
        mfx_EncSurface, &CurrentRefCount);
  }
  if (refcounter_sts < MFX_ERR_NONE || mfx_EncSurface == nullptr ||
      CurrentRefCount == 0) {
    sts = mfx_VideoEnc->GetSurface(&mfx_EncSurface);
    if (sts < MFX_ERR_NONE) {
      error("Error code: %d", sts);
      throw std::runtime_error("Encode(): Get encode surface error");
    }
    mfx_EncSurface->FrameInterface->GetRefCounter(mfx_EncSurface,
                                                  &mfx_EncSurfaceRefCount);
  }

#if defined(_WIN32) || defined(_WIN64)
  mfxSurfaceD3D11Tex2D Texture = {};
  Texture.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_D3D11_TEX2D;
  Texture.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
  Texture.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceD3D11Tex2D);
#else
  mfxSurfaceVAAPI Texture{};
  Texture.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_VAAPI;
  Texture.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_COPY;
  Texture.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceVAAPI);
#endif

  while (GetFreeTaskIndex(&nTaskIdx) == MFX_ERR_NOT_FOUND) {
    do {
      sync_sts = MFXVideoCORE_SyncOperation(
          mfx_Session, mfx_TaskPool[mfx_SyncTaskID].syncp, 100);
      if (sync_sts < MFX_ERR_NONE) {
        error("Error code: %d", sync_sts);
        throw std::runtime_error("Encode(): Syncronization error");
      }
    } while (sync_sts == MFX_WRN_IN_EXECUTION);

    mfxU8 *pTemp = std::move(mfx_Bitstream.Data);
    mfx_Bitstream = std::move(mfx_TaskPool[mfx_SyncTaskID].mfxBS);

    mfx_TaskPool[mfx_SyncTaskID].mfxBS.Data = std::move(pTemp);
    mfx_TaskPool[mfx_SyncTaskID].mfxBS.DataLength = 0;
    mfx_TaskPool[mfx_SyncTaskID].mfxBS.DataOffset = 0;
    mfx_TaskPool[mfx_SyncTaskID].syncp = nullptr;
    nTaskIdx = std::move(mfx_SyncTaskID);
    *pBS = std::move(&mfx_Bitstream);
  }

  try {
    hw->copy_tex(Texture, std::move(tex_handle), lock_key,
                 static_cast<mfxU64 *>(next_key));
  } catch (const std::exception &) {
    error("Error code: %d", sts);
    throw;
  }

  sts = mfx_MemoryInterface->ImportFrameSurface(
      mfx_MemoryInterface, MFX_SURFACE_COMPONENT_ENCODE,
      reinterpret_cast<mfxSurfaceHeader *>(&Texture), &mfx_EncSurface);
  if (sts < MFX_ERR_NONE) {
    error("Error code: %d", sts);
    throw std::runtime_error("Encode(): Texture import error");
  }

  mfx_TaskPool[nTaskIdx].mfxBS.TimeStamp = ts;
  mfx_EncSurface->Data.TimeStamp = std::move(ts);

  for (;;) {
    if (mfx_VPP) {
      mfxSyncPoint VPPSyncPoint = nullptr;

      sts = mfx_VideoVPP->GetSurfaceOut(&mfx_VPPSurface);
      if (sts < MFX_ERR_NONE) {
        error("Error code: %d", sts);
        throw std::runtime_error("Encode(): Get VPP out surface error");
      }

      sts = mfx_VideoVPP->RunFrameVPPAsync(mfx_EncSurface, mfx_VPPSurface,
                                           nullptr, &VPPSyncPoint);
      if (sts < MFX_ERR_NONE) {
        error("Error code: %d", sts);
        throw std::runtime_error("Encode(): VPP processing error");
      }
    }

    sts = mfx_VideoEnc->EncodeFrameAsync(
        nullptr, (mfx_VPP ? mfx_VPPSurface : mfx_EncSurface),
        &mfx_TaskPool[nTaskIdx].mfxBS, &mfx_TaskPool[nTaskIdx].syncp);

    if (MFX_ERR_NONE == sts) {
      break;
    } else if (MFX_ERR_NONE < sts && !mfx_TaskPool[nTaskIdx].syncp) {
      if (MFX_WRN_DEVICE_BUSY == sts)
        Sleep(1);
    } else if (MFX_ERR_NONE < sts && mfx_TaskPool[nTaskIdx].syncp) {
      sts = MFX_ERR_NONE;
      break;
    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts ||
               MFX_ERR_MORE_BITSTREAM == sts) {
      ChangeBitstreamSize(((mfx_EncParams.mfx.BufferSizeInKB * 1000 * 5) * 2));
      warn("The buffer size is too small. The size has been increased by 2 "
           "times. New value: %d KB",
           (((mfx_EncParams.mfx.BufferSizeInKB * 1000 * 5) * 2) / 1000));
      mfx_EncParams.mfx.BufferSizeInKB *= 2;
    } else if (MFX_ERR_MORE_DATA == sts) {
      break;
    } else {
      error("Error code: %d", sts);
      throw std::runtime_error("Encode(): Encode processing error");
      break;
    }
  }

  if (mfx_VPP) {
    mfx_VPPSurface->FrameInterface->Release(mfx_VPPSurface);
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(mfxU64 ts, uint8_t **frame_data,
                                           uint32_t *frame_linesize,
                                           mfxBitstream **pBS) {
  mfxStatus sts = MFX_ERR_NONE, sync_sts = MFX_ERR_NONE,
            refcounter_sts = MFX_ERR_NONE;
  *pBS = nullptr;
  int nTaskIdx = 0;
  mfxU32 CurrentRefCount = 0;

  if (mfx_EncSurface != nullptr) {
    refcounter_sts = mfx_EncSurface->FrameInterface->GetRefCounter(
        mfx_EncSurface, &CurrentRefCount);
  }

  mfx_VideoEnc->GetSurface(&mfx_EncSurface);
  mfx_EncSurface->FrameInterface->GetRefCounter(mfx_EncSurface,
                                                &mfx_EncSurfaceRefCount);

  while (GetFreeTaskIndex(&nTaskIdx) == MFX_ERR_NOT_FOUND) {
    do {
      sync_sts = MFXVideoCORE_SyncOperation(
          mfx_Session, mfx_TaskPool[mfx_SyncTaskID].syncp, 100);
      if (sync_sts < MFX_ERR_NONE) {
        warn("Encode.Sync error: %d", sync_sts);
      }
    } while (sync_sts == MFX_WRN_IN_EXECUTION);

    mfxU8 *pTemp = std::move(mfx_Bitstream.Data);
    mfx_Bitstream = std::move(mfx_TaskPool[mfx_SyncTaskID].mfxBS);

    mfx_TaskPool[mfx_SyncTaskID].mfxBS.Data = std::move(pTemp);
    mfx_TaskPool[mfx_SyncTaskID].mfxBS.DataLength = 0;
    mfx_TaskPool[mfx_SyncTaskID].mfxBS.DataOffset = 0;
    mfx_TaskPool[mfx_SyncTaskID].syncp = nullptr;
    nTaskIdx = std::move(mfx_SyncTaskID);
    *pBS = std::move(&mfx_Bitstream);
  }

  sts = mfx_EncSurface->FrameInterface->Map(mfx_EncSurface, MFX_MAP_WRITE);
  if (sts < MFX_ERR_NONE) {
    warn("Surface.Map.Write error: %d", sts);
    return sts;
  }

  mfx_EncSurface->Data.TimeStamp = ts;
  LoadFrameData(mfx_EncSurface, std::move(frame_data),
                std::move(frame_linesize));

  mfx_TaskPool[nTaskIdx].mfxBS.TimeStamp = std::move(ts);

  sts = mfx_EncSurface->FrameInterface->Unmap(mfx_EncSurface);
  if (sts < MFX_ERR_NONE) {
    warn("Surface.Unmap.Write error: %d", sts);
    return sts;
  }

  /*Encode a frame asynchronously (returns immediately)*/
  for (;;) {
    if (mfx_VPP) {
      mfxSyncPoint VPPSyncPoint = nullptr;

      sts = mfx_VideoVPP->GetSurfaceOut(&mfx_VPPSurface);
      if (sts < MFX_ERR_NONE) {
        warn("VPP surface error: %d", sts);
        return MFX_ERR_NOT_INITIALIZED;
      }
      sts = mfx_VideoVPP->RunFrameVPPAsync(mfx_EncSurface, mfx_VPPSurface,
                                           nullptr, &VPPSyncPoint);
      if (sts < MFX_ERR_NONE) {
        warn("VPP error: %d", sts);
      }

      mfx_EncSurface->FrameInterface->Release(mfx_EncSurface);
    }

    sts = mfx_VideoEnc->EncodeFrameAsync(
        nullptr, (mfx_VPP ? mfx_VPPSurface : mfx_EncSurface),
        &mfx_TaskPool[nTaskIdx].mfxBS, &mfx_TaskPool[nTaskIdx].syncp);

    if (MFX_ERR_NONE == sts) {
      break;
    } else if (MFX_ERR_NONE < sts && !mfx_TaskPool[nTaskIdx].syncp) {
      if (MFX_WRN_DEVICE_BUSY == sts)
        Sleep(1);
    } else if (MFX_ERR_NONE < sts && mfx_TaskPool[nTaskIdx].syncp) {
      sts = MFX_ERR_NONE;
      break;
    } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts ||
               MFX_ERR_MORE_BITSTREAM == sts) {
      ChangeBitstreamSize(((mfx_EncParams.mfx.BufferSizeInKB * 1000 * 5) * 2));
      warn("The buffer size is too small. The size has been increased by 2 "
           "times. New value: %d KB",
           (((mfx_EncParams.mfx.BufferSizeInKB * 1000 * 5) * 2) / 1000));
      mfx_EncParams.mfx.BufferSizeInKB *= 2;
    } else if (MFX_ERR_MORE_DATA == sts) {
      break;
    } else {
      warn("Encode error: %d", sts);
      break;
    }
  }

  if (mfx_VPP) {
    mfx_VPPSurface->FrameInterface->Release(mfx_VPPSurface);
  } else {
    mfx_EncSurface->FrameInterface->Release(mfx_EncSurface);
  }

  return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain() {
  mfxStatus sts = MFX_ERR_NONE;

  for (int i = 0; i < mfx_TaskPoolSize; i++) {
    if (mfx_TaskPool[i].mfxBS.Data != nullptr) {
      for (;;) {
        sts = mfx_VideoEnc->EncodeFrameAsync(
            nullptr, nullptr, &mfx_TaskPool[i].mfxBS, &mfx_TaskPool[i].syncp);
        if (sts == MFX_ERR_NONE) {
          // We continue until we get MFX_ERR_MORE_DATA
        } else if (sts == MFX_ERR_MORE_DATA) {
          mfx_TaskPool[i].mfxBS.Data = nullptr;
          break;
        } else {
          warn("Drain(): The Task #%d buffer was released with an error %d", i,
               sts);
          break;
        }
      }
    }
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::ClearData() {
  mfxStatus sts = MFX_ERR_NONE;
  try {
    Drain();

    if (mfx_VideoEnc) {
      mfxU32 CurrentRefCount = 0;
      sts = mfx_EncSurface->FrameInterface->GetRefCounter(mfx_EncSurface,
                                                          &CurrentRefCount);
      if (sts >= MFX_ERR_NONE && CurrentRefCount > 0) {
        for (size_t i = 0; i < CurrentRefCount; i++) {
          mfx_EncSurface->FrameInterface->Release(mfx_EncSurface);
        }
      }

      mfx_EncParams.~ExtBufManager();
      sts = mfx_VideoEnc->Close();
      if (sts >= MFX_ERR_NONE) {
        try {
          delete mfx_VideoEnc;
          mfx_VideoEnc = nullptr;
        } catch (const std::bad_alloc &) {
          throw;
        }
      }
    }

    if (mfx_VideoVPP) {
      mfx_VPPParams.~ExtBufManager();
      sts = mfx_VideoVPP->Close();
      if (sts >= MFX_ERR_NONE) {
        try {
          delete mfx_VideoVPP;
          mfx_VideoVPP = nullptr;
        } catch (const std::bad_alloc &) {
          throw;
        }
      }
    }

    ReleaseTaskPool();
    ReleaseBitstream();

    if (sts >= MFX_ERR_NONE) {
      hw_handle::encoder_counter--;
    }

    if (mfx_Session) {
      sts = MFXClose(mfx_Session);
      if (sts >= MFX_ERR_NONE) {
        MFXDispReleaseImplDescription(mfx_Loader, nullptr);
        MFXUnload(mfx_Loader);
        mfx_Session = nullptr;
        mfx_Loader = nullptr;
      }
    }

#if defined(__linux__)
    ReleaseSessionData(mfx_SessionData);
    mfx_SessionData = nullptr;
#endif

    if (mfx_UseTexAlloc) {
      hw->release_tex();
    }

    if (hw_handle::encoder_counter <= 0) {
      try {
        hw->release_device();
        delete hw;
        hw = nullptr;
        hw_handle::encoder_counter = 0;
      } catch (const std::bad_alloc &) {
        throw;
      }
    }
  } catch (const std::exception &) {
    throw;
  }

  return sts;
}