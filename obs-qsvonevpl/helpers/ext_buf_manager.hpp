#pragma once
#include "common_utils.hpp"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <vpl/mfxbrc.h>
#include <vpl/mfxcommon.h>
#include <vpl/mfxdefs.h>
#include <vpl/mfxstructures.h>
#include <vpl/mfxsurfacepool.h>

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

#ifndef __MFXAV1_H__
#include <vpl/private/mfxav1.h>
#endif
#ifndef __MFXENCTOOLS_H__
#include <vpl/private/mfxenctools.h>
#endif
#ifndef __MFX_EXT_BUFFERS_H__
#include <vpl/private/mfxddi.h>
#endif

class mfxError : public std::runtime_error {
public:
  mfxError(mfxStatus status = MFX_ERR_UNKNOWN, std::string msg = "")
      : runtime_error(msg), m_Status(status) {}

  mfxStatus GetStatus() const { return m_Status; }

private:
  mfxStatus m_Status;
};

template <class T> struct mfx_ext_buffer_id {};

template <> struct mfx_ext_buffer_id<mfxExtCodingOption> {
  enum { id = MFX_EXTBUFF_CODING_OPTION };
};
template <> struct mfx_ext_buffer_id<mfxExtCodingOption2> {
  enum { id = MFX_EXTBUFF_CODING_OPTION2 };
};
template <> struct mfx_ext_buffer_id<mfxExtCodingOption3> {
  enum { id = MFX_EXTBUFF_CODING_OPTION3 };
};
template <> struct mfx_ext_buffer_id<mfxExtAvcTemporalLayers> {
  enum { id = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS };
};
template <> struct mfx_ext_buffer_id<mfxExtTemporalLayers> {
  enum { id = MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS };
};
template <> struct mfx_ext_buffer_id<mfxExtAVCRefListCtrl> {
  enum { id = MFX_EXTBUFF_AVC_REFLIST_CTRL };
};
template <> struct mfx_ext_buffer_id<mfxExtThreadsParam> {
  enum { id = MFX_EXTBUFF_THREADS_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtHEVCRefLists> {
  enum { id = MFX_EXTBUFF_HEVC_REFLISTS };
};
template <> struct mfx_ext_buffer_id<mfxExtBRC> {
  enum { id = MFX_EXTBUFF_BRC };
};
template <> struct mfx_ext_buffer_id<mfxExtHEVCParam> {
  enum { id = MFX_EXTBUFF_HEVC_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtDecVideoProcessing> {
  enum { id = MFX_EXTBUFF_DEC_VIDEO_PROCESSING };
};
template <> struct mfx_ext_buffer_id<mfxExtDecodeErrorReport> {
  enum { id = MFX_EXTBUFF_DECODE_ERROR_REPORT };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPDoNotUse> {
  enum { id = MFX_EXTBUFF_VPP_DONOTUSE };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPDoUse> {
  enum { id = MFX_EXTBUFF_VPP_DOUSE };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPDeinterlacing> {
  enum { id = MFX_EXTBUFF_VPP_DEINTERLACING };
};
template <> struct mfx_ext_buffer_id<mfxExtCodingOptionSPSPPS> {
  enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS };
};
template <> struct mfx_ext_buffer_id<mfxExtVppMctf> {
  enum { id = MFX_EXTBUFF_VPP_MCTF };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPComposite> {
  enum { id = MFX_EXTBUFF_VPP_COMPOSITE };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPFieldProcessing> {
  enum { id = MFX_EXTBUFF_VPP_FIELD_PROCESSING };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPDetail> {
  enum { id = MFX_EXTBUFF_VPP_DETAIL };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPFrameRateConversion> {
  enum { id = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION };
};
template <> struct mfx_ext_buffer_id<mfxExtHEVCTiles> {
  enum { id = MFX_EXTBUFF_HEVC_TILES };
};
template <> struct mfx_ext_buffer_id<mfxExtVP9Param> {
  enum { id = MFX_EXTBUFF_VP9_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtAV1BitstreamParam> {
  enum { id = MFX_EXTBUFF_AV1_BITSTREAM_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtAV1ResolutionParam> {
  enum { id = MFX_EXTBUFF_AV1_RESOLUTION_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtAV1TileParam> {
  enum { id = MFX_EXTBUFF_AV1_TILE_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtVideoSignalInfo> {
  enum { id = MFX_EXTBUFF_VIDEO_SIGNAL_INFO };
};
template <> struct mfx_ext_buffer_id<mfxExtHEVCRegion> {
  enum { id = MFX_EXTBUFF_HEVC_REGION };
};
template <> struct mfx_ext_buffer_id<mfxExtAVCRoundingOffset> {
  enum { id = MFX_EXTBUFF_AVC_ROUNDING_OFFSET };
};
template <> struct mfx_ext_buffer_id<mfxExtPartialBitstreamParam> {
  enum { id = MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPDenoise2> {
  enum { id = MFX_EXTBUFF_VPP_DENOISE2 };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPProcAmp> {
  enum { id = MFX_EXTBUFF_VPP_PROCAMP };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPImageStab> {
  enum { id = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPVideoSignalInfo> {
  enum { id = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPMirroring> {
  enum { id = MFX_EXTBUFF_VPP_MIRRORING };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPColorFill> {
  enum { id = MFX_EXTBUFF_VPP_COLORFILL };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPRotation> {
  enum { id = MFX_EXTBUFF_VPP_ROTATION };
};
template <> struct mfx_ext_buffer_id<mfxExtVPPScaling> {
  enum { id = MFX_EXTBUFF_VPP_SCALING };
};
template <> struct mfx_ext_buffer_id<mfxExtColorConversion> {
  enum { id = MFX_EXTBUFF_VPP_COLOR_CONVERSION };
};
template <> struct mfx_ext_buffer_id<mfxExtPredWeightTable> {
  enum { id = MFX_EXTBUFF_PRED_WEIGHT_TABLE };
};
template <> struct mfx_ext_buffer_id<mfxExtHyperModeParam> {
  enum { id = MFX_EXTBUFF_HYPER_MODE_PARAM };
};
template <> struct mfx_ext_buffer_id<mfxExtAllocationHints> {
  enum { id = MFX_EXTBUFF_ALLOCATION_HINTS };
};
template <> struct mfx_ext_buffer_id<mfxExtVPP3DLut> {
  enum { id = MFX_EXTBUFF_VPP_3DLUT };
};
template <> struct mfx_ext_buffer_id<mfxExtMasteringDisplayColourVolume> {
  enum { id = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME };
};
template <> struct mfx_ext_buffer_id<mfxExtContentLightLevelInfo> {
  enum { id = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO };
};
template <> struct mfx_ext_buffer_id<mfxExtEncToolsConfig> {
  enum { id = MFX_EXTBUFF_ENCTOOLS_CONFIG };
};
template <> struct mfx_ext_buffer_id<mfxExtAV1AuxData> {
  enum { id = MFX_EXTBUFF_AV1_AUXDATA };
};
template <> struct mfx_ext_buffer_id<mfxExtChromaLocInfo> {
  enum { id = MFX_EXTBUFF_CHROMA_LOC_INFO };
};
template <> struct mfx_ext_buffer_id<mfxExtCodingOptionVPS> {
  enum { id = MFX_EXTBUFF_CODING_OPTION_VPS };
};
template <> struct mfx_ext_buffer_id<mfxExtCodingOptionDDI> {
  enum { id = MFX_EXTBUFF_DDI };
};
template <> struct mfx_ext_buffer_id<mfxExtEncoderROI> {
  enum { id = MFX_EXTBUFF_ENCODER_ROI };
};
template <> struct mfx_ext_buffer_id<mfxExtEncoderResetOption> {
  enum { id = MFX_EXTBUFF_ENCODER_RESET_OPTION };
};
template <> struct mfx_ext_buffer_id<mfxExtVppAuxData> {
  enum { id = MFX_EXTBUFF_VPP_AUXDATA };
};

#ifdef ONEVPL_EXPERIMENTAL
template <> struct mfx_ext_buffer_id<mfxExtVPPPercEncPrefilter> {
  enum { id = MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER };
};
template <> struct mfx_ext_buffer_id<mfxExtTuneEncodeQuality> {
  enum { id = MFX_EXTBUFF_TUNE_ENCODE_QUALITY };
};
#endif

constexpr uint16_t max_num_ext_buffers =
    63 * 2; // '*2' is for max estimation if all extBuffer were 'paired'

// helper function to initialize mfx ext buffer structure
template <class T> void init_ext_buffer(T &ext_buffer) {
  memset(&ext_buffer, 0, sizeof(ext_buffer));
  static_cast<mfxExtBuffer *>(&ext_buffer)->BufferId = mfx_ext_buffer_id<T>::id;
  static_cast<mfxExtBuffer *>(&ext_buffer)->BufferSz = sizeof(ext_buffer);
}

template <typename T> struct IsPairedMfxExtBuffer : std::false_type {};
template <>
struct IsPairedMfxExtBuffer<mfxExtAVCRefListCtrl> : std::true_type {};
template <>
struct IsPairedMfxExtBuffer<mfxExtAVCRoundingOffset> : std::true_type {};
template <>
struct IsPairedMfxExtBuffer<mfxExtPredWeightTable> : std::true_type {};
template <typename R> struct ExtParamAccessor {
private:
  using mfxExtBufferDoublePtr = mfxExtBuffer **;

public:
  mfxU16 &NumExtParam;
  mfxExtBufferDoublePtr &ExtParam;
  ExtParamAccessor(const R &r)
      : NumExtParam(const_cast<mfxU16 &>(r.NumExtParam)),
        ExtParam(const_cast<mfxExtBufferDoublePtr &>(r.ExtParam)) {}
};

template <> struct ExtParamAccessor<mfxFrameSurface1> {
private:
  using mfxExtBufferDoublePtr = mfxExtBuffer **;

public:
  mfxU16 &NumExtParam;
  mfxExtBufferDoublePtr &ExtParam;
  ExtParamAccessor(const mfxFrameSurface1 &r)
      : NumExtParam(const_cast<mfxU16 &>(r.Data.NumExtParam)),
        ExtParam(const_cast<mfxExtBufferDoublePtr &>(r.Data.ExtParam)) {}
};

/** ExtBufManager is an utility class which
 *  provide interface for mfxExtBuffer objects management in any mfx structure
 * (e.g. mfxVideoParam)
 */
template <typename T> class ExtBufManager : public T {
public:
  ExtBufManager() : T() { m_ext_buf.reserve(max_num_ext_buffers); }

  ~ExtBufManager() // only buffers allocated by wrapper can be released
  {
    for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++) {
      delete[] (mfxU8 *)(*it);
    }
  }

  ExtBufManager(const ExtBufManager &ref) {
    m_ext_buf.reserve(max_num_ext_buffers);
    *this = ref; // call to operator=
  }

  ExtBufManager &operator=(const ExtBufManager &ref) {
    const T *src_base = &ref;
    this->operator=(*src_base);
    return *this;
  }

  ExtBufManager(const T &ref) {
    *this = ref; // call to operator=
  }

  ExtBufManager &operator=(const T &ref) {
    // copy content of main structure type T
    T *dst_base = this;
    const T *src_base = &ref;
    *dst_base = *src_base;

    // remove all existing extension buffers
    ClearBuffers();

    const auto ref_ = ExtParamAccessor<T>(ref);

    // reproduce list of extension buffers and copy its content
    for (size_t i = 0; i < ref_.NumExtParam; ++i) {
      const auto src_buf = ref_.ExtParam[i];
      if (!src_buf)
        throw mfxError(MFX_ERR_NULL_PTR,
                       "Null pointer attached to source ExtParam");
      if (!IsCopyAllowed(src_buf->BufferId)) {
        auto msg = "Deep copy of '" + Fourcc2Str(src_buf->BufferId) +
                   "' extBuffer is not allowed";
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, msg);
      }

      // 'false' below is because here we just copy extBuffer's one by one
      auto dst_buf = AddExtBuffer(src_buf->BufferId, src_buf->BufferSz, false);
      // copy buffer content w/o restoring its type
      memcpy(static_cast<void *>(dst_buf), static_cast<void *>(src_buf),
             src_buf->BufferSz);
    }

    return *this;
  }

  ExtBufManager(ExtBufManager &&) = default;
  ExtBufManager &operator=(ExtBufManager &&) = default;

  mfxExtBuffer *AddExtBuffer(mfxU32 id, mfxU32 size) {
    return AddExtBuffer(id, size, false);
  }

  // Always returns a valid pointer or throws an exception
  template <typename TB> TB *AddExtBuffer() {
    mfxExtBuffer *b = AddExtBuffer(mfx_ext_buffer_id<TB>::id, sizeof(TB),
                                   IsPairedMfxExtBuffer<TB>::value);
    return (TB *)b;
  }

  template <typename TB> void RemoveExtBuffer() {
    auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(),
                           CmpExtBufById(mfx_ext_buffer_id<TB>::id));
    if (it != m_ext_buf.end()) {
      delete[] (mfxU8 *)(*it);
      it = m_ext_buf.erase(it);

      if (IsPairedMfxExtBuffer<TB>::value) {
        if (it == m_ext_buf.end() ||
            (*it)->BufferId != mfx_ext_buffer_id<TB>::id)
          throw mfxError(MFX_ERR_NULL_PTR,
                         "RemoveExtBuffer: ExtBuffer's parity has been broken");

        delete[] (mfxU8 *)(*it);
        m_ext_buf.erase(it);
      }

      RefreshBuffers();
    }
  }

  template <typename TB> TB *GetExtBuffer(uint32_t fieldId = 0) const {
    return (TB *)FindExtBuffer(mfx_ext_buffer_id<TB>::id, fieldId);
  }

  template <typename TB> operator TB *() {
    return (TB *)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
  }

  template <typename TB> operator TB *() const {
    return (TB *)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
  }

private:
  mfxExtBuffer *AddExtBuffer(mfxU32 id, mfxU32 size, bool isPairedExtBuffer) {
    if (!size || !id)
      throw mfxError(MFX_ERR_NULL_PTR, "AddExtBuffer: wrong size or id!");

    auto it =
        std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
    if (it == m_ext_buf.end()) {
      auto buf = reinterpret_cast<mfxExtBuffer *>(new mfxU8[size]);
      memset(buf, 0, size);
      m_ext_buf.push_back(buf);

      buf->BufferId = id;
      buf->BufferSz = size;

      if (isPairedExtBuffer) {
        // Allocate the other mfxExtBuffer _right_after_ the first one ...
        buf = reinterpret_cast<mfxExtBuffer *>(new mfxU8[size]);
        memset(buf, 0, size);
        m_ext_buf.push_back(buf);

        buf->BufferId = id;
        buf->BufferSz = size;

        RefreshBuffers();
        return m_ext_buf[m_ext_buf.size() -
                         2]; // ... and return a pointer to the first one
      }

      RefreshBuffers();
      return m_ext_buf.back();
    }

    return *it;
  }

  mfxExtBuffer *FindExtBuffer(mfxU32 id, uint32_t fieldId) const {
    auto it =
        std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
    if (fieldId && it != m_ext_buf.end()) {
      ++it;
      return it != m_ext_buf.end() ? *it : nullptr;
    }
    return it != m_ext_buf.end() ? *it : nullptr;
  }

  void RefreshBuffers() {
    auto this_ = ExtParamAccessor<T>(*this);
    this_.NumExtParam = static_cast<mfxU16>(m_ext_buf.size());
    this_.ExtParam = this_.NumExtParam ? m_ext_buf.data() : nullptr;
  }

  void ClearBuffers() {
    if (m_ext_buf.size()) {
      for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++) {
        delete[] (mfxU8 *)(*it);
      }
      m_ext_buf.clear();
    }
    RefreshBuffers();
  }

  bool IsCopyAllowed(mfxU32 id) {
    static const mfxU32 allowed[] = {
        MFX_EXTBUFF_CODING_OPTION,       MFX_EXTBUFF_CODING_OPTION2,
        MFX_EXTBUFF_CODING_OPTION3,      MFX_EXTBUFF_BRC,
        MFX_EXTBUFF_HEVC_PARAM,          MFX_EXTBUFF_VP9_PARAM,
        MFX_EXTBUFF_AV1_BITSTREAM_PARAM, MFX_EXTBUFF_AV1_RESOLUTION_PARAM,
        MFX_EXTBUFF_AV1_TILE_PARAM,      MFX_EXTBUFF_DEC_VIDEO_PROCESSING,
        MFX_EXTBUFF_ALLOCATION_HINTS};

    auto it = std::find_if(
        std::begin(allowed), std::end(allowed),
        [&id](const mfxU32 allowed_id) { return allowed_id == id; });
    return it != std::end(allowed);
  }

  struct CmpExtBufById {
    mfxU32 id;

    CmpExtBufById(mfxU32 _id) : id(_id){};

    bool operator()(mfxExtBuffer *b) { return (b && b->BufferId == id); };
  };

  static std::string Fourcc2Str(mfxU32 fourcc) {
    std::string s;
    for (size_t i = 0; i < 4; i++) {
      s.push_back(*(i + reinterpret_cast<char *>(&fourcc)));
    }
    return s;
  }

  std::vector<mfxExtBuffer *> m_ext_buf;
};

using mfx_VideoParam = ExtBufManager<mfxVideoParam>;
using mfx_EncodeCtrl = ExtBufManager<mfxEncodeCtrl>;
using mfx_InitParam = ExtBufManager<mfxInitParam>;
using mfx_FrameSurface = ExtBufManager<mfxFrameSurface1>;
