#pragma once
#ifndef __QSV_VPL_EXT_BUF_MANAGER_H__
#define __QSV_VPL_EXT_BUF_MANAGER_H__
#endif

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif
#ifndef _CSTDINT_
#include <cstdint>
#endif
#ifndef _NEW_
#include <new>
#endif
#ifndef _STDEXCEPT_
#include <stdexcept>
#endif
#ifndef _STRING_
#include <string>
#endif
#ifndef _TYPE_TRAITS_
#include <type_traits>
#endif
#ifndef _VECTOR_
#include <vector>
#endif
#ifndef __MFXBRC_H__
#include <vpl/mfxbrc.h>
#endif
#ifndef __MFXCOMMON_H__
#include <vpl/mfxcommon.h>
#endif
#ifndef __MFXDEFS_H__
#include <vpl/mfxdefs.h>
#endif
#ifndef __MFXSTRUCTURES_H__
#include <vpl/mfxstructures.h>
#endif
#ifndef __MFXSURFACE_POOL_H__
#include <vpl/mfxsurfacepool.h>
#endif
#ifndef __QSV_VPL_COMMON_UTILS_H__
#include "helpers/common_utils.hpp"
#endif
#ifndef __MFXAV1_H__
#include <vpl/private/mfxav1.h>
#endif
#ifndef __MFXENCTOOLS_H__
#include <vpl/private/mfxenctools.h>
#endif
#ifndef __MFXEXT_BUFFERS_H__
#include <vpl/private/mfxddi.h>
#endif

template <class T> struct MFXExtBufferID {};

template <> struct MFXExtBufferID<mfxExtCodingOption> {
  enum { id = MFX_EXTBUFF_CODING_OPTION };
};
template <> struct MFXExtBufferID<mfxExtCodingOption2> {
  enum { id = MFX_EXTBUFF_CODING_OPTION2 };
};
template <> struct MFXExtBufferID<mfxExtCodingOption3> {
  enum { id = MFX_EXTBUFF_CODING_OPTION3 };
};
template <> struct MFXExtBufferID<mfxExtAvcTemporalLayers> {
  enum { id = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS };
};
template <> struct MFXExtBufferID<mfxExtTemporalLayers> {
  enum { id = MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS };
};
template <> struct MFXExtBufferID<mfxExtAVCRefListCtrl> {
  enum { id = MFX_EXTBUFF_AVC_REFLIST_CTRL };
};
template <> struct MFXExtBufferID<mfxExtThreadsParam> {
  enum { id = MFX_EXTBUFF_THREADS_PARAM };
};
template <> struct MFXExtBufferID<mfxExtHEVCRefLists> {
  enum { id = MFX_EXTBUFF_HEVC_REFLISTS };
};
template <> struct MFXExtBufferID<mfxExtBRC> {
  enum { id = MFX_EXTBUFF_BRC };
};
template <> struct MFXExtBufferID<mfxExtHEVCParam> {
  enum { id = MFX_EXTBUFF_HEVC_PARAM };
};
template <> struct MFXExtBufferID<mfxExtDecVideoProcessing> {
  enum { id = MFX_EXTBUFF_DEC_VIDEO_PROCESSING };
};
template <> struct MFXExtBufferID<mfxExtDecodeErrorReport> {
  enum { id = MFX_EXTBUFF_DECODE_ERROR_REPORT };
};
template <> struct MFXExtBufferID<mfxExtVPPDoNotUse> {
  enum { id = MFX_EXTBUFF_VPP_DONOTUSE };
};
template <> struct MFXExtBufferID<mfxExtVPPDoUse> {
  enum { id = MFX_EXTBUFF_VPP_DOUSE };
};
template <> struct MFXExtBufferID<mfxExtVPPDeinterlacing> {
  enum { id = MFX_EXTBUFF_VPP_DEINTERLACING };
};
template <> struct MFXExtBufferID<mfxExtCodingOptionSPSPPS> {
  enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS };
};
template <> struct MFXExtBufferID<mfxExtVppMctf> {
  enum { id = MFX_EXTBUFF_VPP_MCTF };
};
template <> struct MFXExtBufferID<mfxExtVPPComposite> {
  enum { id = MFX_EXTBUFF_VPP_COMPOSITE };
};
template <> struct MFXExtBufferID<mfxExtVPPFieldProcessing> {
  enum { id = MFX_EXTBUFF_VPP_FIELD_PROCESSING };
};
template <> struct MFXExtBufferID<mfxExtVPPDetail> {
  enum { id = MFX_EXTBUFF_VPP_DETAIL };
};
template <> struct MFXExtBufferID<mfxExtVPPFrameRateConversion> {
  enum { id = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION };
};
template <> struct MFXExtBufferID<mfxExtHEVCTiles> {
  enum { id = MFX_EXTBUFF_HEVC_TILES };
};
template <> struct MFXExtBufferID<mfxExtVP9Param> {
  enum { id = MFX_EXTBUFF_VP9_PARAM };
};
template <> struct MFXExtBufferID<mfxExtAV1BitstreamParam> {
  enum { id = MFX_EXTBUFF_AV1_BITSTREAM_PARAM };
};
template <> struct MFXExtBufferID<mfxExtAV1ResolutionParam> {
  enum { id = MFX_EXTBUFF_AV1_RESOLUTION_PARAM };
};
template <> struct MFXExtBufferID<mfxExtAV1TileParam> {
  enum { id = MFX_EXTBUFF_AV1_TILE_PARAM };
};
template <> struct MFXExtBufferID<mfxExtAV1ScreenContentTools> {
  enum { id = MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS };
};
template <> struct MFXExtBufferID<mfxExtVideoSignalInfo> {
  enum { id = MFX_EXTBUFF_VIDEO_SIGNAL_INFO };
};
template <> struct MFXExtBufferID<mfxExtHEVCRegion> {
  enum { id = MFX_EXTBUFF_HEVC_REGION };
};
template <> struct MFXExtBufferID<mfxExtAVCRoundingOffset> {
  enum { id = MFX_EXTBUFF_AVC_ROUNDING_OFFSET };
};
template <> struct MFXExtBufferID<mfxExtPartialBitstreamParam> {
  enum { id = MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM };
};
template <> struct MFXExtBufferID<mfxExtVPPDenoise2> {
  enum { id = MFX_EXTBUFF_VPP_DENOISE2 };
};
template <> struct MFXExtBufferID<mfxExtVPPProcAmp> {
  enum { id = MFX_EXTBUFF_VPP_PROCAMP };
};
template <> struct MFXExtBufferID<mfxExtVPPImageStab> {
  enum { id = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION };
};
template <> struct MFXExtBufferID<mfxExtVPPVideoSignalInfo> {
  enum { id = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO };
};
template <> struct MFXExtBufferID<mfxExtVPPMirroring> {
  enum { id = MFX_EXTBUFF_VPP_MIRRORING };
};
template <> struct MFXExtBufferID<mfxExtVPPColorFill> {
  enum { id = MFX_EXTBUFF_VPP_COLORFILL };
};
template <> struct MFXExtBufferID<mfxExtVPPRotation> {
  enum { id = MFX_EXTBUFF_VPP_ROTATION };
};
template <> struct MFXExtBufferID<mfxExtVPPScaling> {
  enum { id = MFX_EXTBUFF_VPP_SCALING };
};
template <> struct MFXExtBufferID<mfxExtColorConversion> {
  enum { id = MFX_EXTBUFF_VPP_COLOR_CONVERSION };
};
template <> struct MFXExtBufferID<mfxExtPredWeightTable> {
  enum { id = MFX_EXTBUFF_PRED_WEIGHT_TABLE };
};
template <> struct MFXExtBufferID<mfxExtHyperModeParam> {
  enum { id = MFX_EXTBUFF_HYPER_MODE_PARAM };
};
template <> struct MFXExtBufferID<mfxExtAllocationHints> {
  enum { id = MFX_EXTBUFF_ALLOCATION_HINTS };
};
template <> struct MFXExtBufferID<mfxExtVPP3DLut> {
  enum { id = MFX_EXTBUFF_VPP_3DLUT };
};
template <> struct MFXExtBufferID<mfxExtMasteringDisplayColourVolume> {
  enum { id = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME };
};
template <> struct MFXExtBufferID<mfxExtContentLightLevelInfo> {
  enum { id = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO };
};
template <> struct MFXExtBufferID<mfxExtEncToolsConfig> {
  enum { id = MFX_EXTBUFF_ENCTOOLS_CONFIG };
};
template <> struct MFXExtBufferID<mfxExtAV1AuxData> {
  enum { id = MFX_EXTBUFF_AV1_AUXDATA };
};
template <> struct MFXExtBufferID<mfxExtChromaLocInfo> {
  enum { id = MFX_EXTBUFF_CHROMA_LOC_INFO };
};
template <> struct MFXExtBufferID<mfxExtCodingOptionVPS> {
  enum { id = MFX_EXTBUFF_CODING_OPTION_VPS };
};
template <> struct MFXExtBufferID<mfxExtCodingOptionDDI> {
  enum { id = MFX_EXTBUFF_DDI };
};
template <> struct MFXExtBufferID<mfxExtEncoderROI> {
  enum { id = MFX_EXTBUFF_ENCODER_ROI };
};
template <> struct MFXExtBufferID<mfxExtEncoderResetOption> {
  enum { id = MFX_EXTBUFF_ENCODER_RESET_OPTION };
};
template <> struct MFXExtBufferID<mfxExtVppAuxData> {
  enum { id = MFX_EXTBUFF_VPP_AUXDATA };
};

#ifdef ONEVPL_EXPERIMENTAL
template <> struct MFXExtBufferID<mfxExtVPPPercEncPrefilter> {
  enum { id = MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER };
};
template <> struct MFXExtBufferID<mfxExtTuneEncodeQuality> {
  enum { id = MFX_EXTBUFF_TUNE_ENCODE_QUALITY };
};
#endif

constexpr uint16_t MaxNumExtBuffers =
    63 * 2; // '*2' is for max estimation if all extBuffer were 'paired'

// helper function to initialize mfx ext Bufferfer structure
template <class T> void InitExtBuffer(T &ExtBuffer) {
  memset(&ExtBuffer, 0, sizeof(ExtBuffer));
  static_cast<mfxExtBuffer *>(&ExtBuffer)->BufferId = MFXExtBufferID<T>::ID;
  static_cast<mfxExtBuffer *>(&ExtBuffer)->BufferSz = sizeof(ExtBuffer);
}

template <typename T> struct IsPairedMFXExtBuffer : std::false_type {};
template <>
struct IsPairedMFXExtBuffer<mfxExtAVCRefListCtrl> : std::true_type {};
template <>
struct IsPairedMFXExtBuffer<mfxExtAVCRoundingOffset> : std::true_type {};
template <>
struct IsPairedMFXExtBuffer<mfxExtPredWeightTable> : std::true_type {};
template <typename R> struct ExtParamAccessor {
private:
  using MFXExtBufferDoublePTR = mfxExtBuffer **;

public:
  mfxU16 &NumExtParam;
  MFXExtBufferDoublePTR &ExtParam;
  ExtParamAccessor(const R &Buffer)
      : NumExtParam(const_cast<mfxU16 &>(Buffer.NumExtParam)),
        ExtParam(const_cast<MFXExtBufferDoublePTR &>(Buffer.ExtParam)) {}
};

template <> struct ExtParamAccessor<mfxFrameSurface1> {
private:
  using MFXExtBufferDoublePTR = mfxExtBuffer **;

public:
  mfxU16 &NumExtParam;
  MFXExtBufferDoublePTR &ExtParam;
  ExtParamAccessor(const mfxFrameSurface1 &Buffer)
      : NumExtParam(const_cast<mfxU16 &>(Buffer.Data.NumExtParam)),
        ExtParam(const_cast<MFXExtBufferDoublePTR &>(Buffer.Data.ExtParam)) {}
};

/** ExtBufManager is an utility class which
 *  provide interface for mfxExtBuffer objects management in any mfx structure
 * (e.g. mfxVideoParam)
 */
template <typename T> class ExtBufManager : public T {
public:
  ExtBufManager() : T() { MFXExtBufferPool.reserve(MaxNumExtBuffers); }

  ~ExtBufManager() // only Bufferfers allocated by wrapper can be released
  {
    for (auto it = MFXExtBufferPool.begin(); it != MFXExtBufferPool.end(); it++) {
      delete[] reinterpret_cast<mfxU8 *>((*it));
    }
  }

  ExtBufManager(const ExtBufManager &Ref) {
    MFXExtBufferPool.reserve(MaxNumExtBuffers);
    *this = Ref; // call to operator=
  }

  ExtBufManager &operator=(const ExtBufManager &Ref) {
    const T *SRCBase = &Ref;
    this->operator=(*SRCBase);
    return *this;
  }

  ExtBufManager(const T &Ref) {
    *this = Ref; // call to operator=
  }

  ExtBufManager &operator=(const T &Ref) {
    // copy content of main structure type T
    T *DSTBase = this;
    const T *SRCBase = &Ref;
    *DSTBase = *SRCBase;

    // remove all existing extension Bufferfers
    ClearBuffers();

    const auto Ref_ = ExtParamAccessor<T>(Ref);

    // reproduce list of extension Bufferfers and copy its content
    for (size_t i = 0; i < Ref_.NumExtParam; ++i) {
      const auto SRCBuffer = Ref_.ExtParam[i];
      if (!SRCBuffer)
        throw "Null pointer attached to source ExtParam";
      if (!IsCopyAllowed(SRCBuffer->BufferId)) {
        auto msg = "Deep copy of '" + Fourcc2Str(SRCBuffer->BufferId) +
                   "' extBuffer is not allowed";
        throw msg;
      }

      // 'false' below is because here we just copy extBuffer's one by one
      auto DSTBuffer = AddExtBuffer(SRCBuffer->BufferId, SRCBuffer->BufferSz, false);
      // copy Bufferfer content w/o restoring its type
      memcpy(static_cast<void *>(DSTBuffer), static_cast<void *>(SRCBuffer),
             SRCBuffer->BufferSz);
    }

    return *this;
  }

  ExtBufManager(ExtBufManager &&) = default;
  ExtBufManager &operator=(ExtBufManager &&) = default;

  mfxExtBuffer *AddExtBuffer(mfxU32 ID, mfxU32 Size) {
    return AddExtBuffer(ID, Size, false);
  }

  // Always returns a valid pointer or throws an exception
  template <typename TB> TB *AddExtBuffer() {
    mfxExtBuffer *Buffer = AddExtBuffer(MFXExtBufferID<TB>::id, sizeof(TB),
                                   IsPairedMFXExtBuffer<TB>::value);
    return reinterpret_cast<TB *>(Buffer);
  }

  template <typename TB> void RemoveExtBuffer() {
    auto it = std::find_if(MFXExtBufferPool.begin(), MFXExtBufferPool.end(),
                           CmpExtBufByID(MFXExtBufferID<TB>::id));
    if (it != MFXExtBufferPool.end()) {
      delete[] static_cast<mfxU8 *>((*it));
      it = MFXExtBufferPool.erase(it);

      if (IsPairedMFXExtBuffer<TB>::Value) {
        if (it == MFXExtBufferPool.end() ||
            (*it)->BufferId != MFXExtBufferID<TB>::id)
          throw "RemoveExtBuffer: ExtBuffer's parity has been broken";

        delete[] static_cast<mfxU8 *>((*it));
        MFXExtBufferPool.erase(it);
      }

      RefreshBuffers();
    }
  }

  template <typename TB> TB *GetExtBuffer(uint32_t FieldID = 0) const {
    return reinterpret_cast<TB *>(FindExtBuffer(MFXExtBufferID<TB>::id, FieldID));
  }

  template <typename TB> operator TB *() {
    return reinterpret_cast<TB *>(FindExtBuffer(MFXExtBufferID<TB>::id, 0));
  }

  template <typename TB> operator TB *() const {
    return reinterpret_cast<TB *>(FindExtBuffer(MFXExtBufferID<TB>::id, 0));
  }

private:
  mfxExtBuffer *AddExtBuffer(mfxU32 ID, mfxU32 Size, bool IsPairedExtBuffer) {
    if (!Size || !ID)
      throw "AddExtBuffer: wrong size or id!";

    auto it =
        std::find_if(MFXExtBufferPool.begin(), MFXExtBufferPool.end(), CmpExtBufByID(ID));
    if (it == MFXExtBufferPool.end()) {
      auto Buffer = reinterpret_cast<mfxExtBuffer *>(new mfxU8[Size]);
      memset(Buffer, 0, Size);
      MFXExtBufferPool.push_back(Buffer);

      Buffer->BufferId = ID;
      Buffer->BufferSz = Size;

      if (IsPairedExtBuffer) {
        // Allocate the other mfxExtBuffer _right_after_ the first one ...
        Buffer = reinterpret_cast<mfxExtBuffer *>(new mfxU8[Size]);
        memset(Buffer, 0, Size);
        MFXExtBufferPool.push_back(Buffer);

        Buffer->BufferId = ID;
        Buffer->BufferSz = Size;

        RefreshBuffers();
        return MFXExtBufferPool[MFXExtBufferPool.size() -
                         2]; // ... and return a pointer to the first one
      }

      RefreshBuffers();
      return MFXExtBufferPool.back();
    }

    return *it;
  }

  mfxExtBuffer *FindExtBuffer(mfxU32 ID, uint32_t FieldID) const {
    auto it =
        std::find_if(MFXExtBufferPool.begin(), MFXExtBufferPool.end(), CmpExtBufByID(ID));
    if (FieldID && it != MFXExtBufferPool.end()) {
      ++it;
      return it != MFXExtBufferPool.end() ? *it : nullptr;
    }
    return it != MFXExtBufferPool.end() ? *it : nullptr;
  }

  void RefreshBuffers() {
    auto this_ = ExtParamAccessor<T>(*this);
    this_.NumExtParam = static_cast<mfxU16>(MFXExtBufferPool.size());
    this_.ExtParam = this_.NumExtParam ? MFXExtBufferPool.data() : nullptr;
  }

  void ClearBuffers() {
    if (MFXExtBufferPool.size()) {
      for (auto it = MFXExtBufferPool.begin(); it != MFXExtBufferPool.end(); it++) {
        delete[] static_cast<mfxU8 *>((*it));
      }
      MFXExtBufferPool.clear();
    }
    RefreshBuffers();
  }

  bool IsCopyAllowed(mfxU32 ID) {
    static const mfxU32 Allowed[] = {
        MFX_EXTBUFF_CODING_OPTION,       MFX_EXTBUFF_CODING_OPTION2,
        MFX_EXTBUFF_CODING_OPTION3,      MFX_EXTBUFF_BRC,
        MFX_EXTBUFF_HEVC_PARAM,          MFX_EXTBUFF_VP9_PARAM,
        MFX_EXTBUFF_AV1_BITSTREAM_PARAM, MFX_EXTBUFF_AV1_RESOLUTION_PARAM,
        MFX_EXTBUFF_AV1_TILE_PARAM,      MFX_EXTBUFF_DEC_VIDEO_PROCESSING,
        MFX_EXTBUFF_ALLOCATION_HINTS};

    auto it = std::find_if(
        std::begin(Allowed), std::end(Allowed),
        [&ID](const mfxU32 AllowedID) { return AllowedID == ID; });
    return it != std::end(Allowed);
  }

  struct CmpExtBufByID {
    mfxU32 ID;

    CmpExtBufByID(mfxU32 ID_) : ID(ID_){};

    bool operator()(mfxExtBuffer *Buffer) { return (Buffer && Buffer->BufferId == ID); };
  };

  static std::string Fourcc2Str(mfxU32 FourCC) {
    std::string String;
    for (size_t i = 0; i < 4; i++) {
      String.push_back(*(i + reinterpret_cast<char *>(&FourCC)));
    }
    return String;
  }

  std::vector<mfxExtBuffer *> MFXExtBufferPool;
};

using MFXVideoParam = ExtBufManager<mfxVideoParam>;
using MFXEncodeCtrl = ExtBufManager<mfxEncodeCtrl>;
using MFXInitParam = ExtBufManager<mfxInitParam>;
using MFXFrameSurface = ExtBufManager<mfxFrameSurface1>;
