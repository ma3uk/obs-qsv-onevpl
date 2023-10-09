#pragma once
#ifndef __QSV_VPL_BITSTREAM_H__
#define __QSV_VPL_BITSTREAM_H__
#endif

#ifndef __QSV_VPL_ENCODER_H__
#include "obs-qsv-onevpl-encoder.hpp"
#endif

struct QSV_VPL_Bitstream {
private:
  mfxBitstream mBitstream;

public:
  mfxBitstream *Get() { return &mBitstream; }
  const mfxBitstream *Get() const { return &mBitstream; }
  mfxBitstream &Bitstream() { return mBitstream; }
  const mfxBitstream &Bitstream() const { return mBitstream; }

  mfxU8 *PTR() const { return mBitstream.Data; }

  mfxU8 *Data() const {
    return static_cast<mfxU8 *>(mBitstream.Data + mBitstream.DataOffset);
  }

  mfxU16 DataFlag() const { return mBitstream.DataFlag; }

  void SetDataFlag(mfxU32 flag) {
    mBitstream.DataFlag = static_cast<mfxU32>(flag);
  }

  mfxU16 Frametype() const { return mBitstream.FrameType; }

  void SetFrametype(mfxU16 frametype) {
    mBitstream.FrameType = static_cast<mfxU16>(frametype);
  }

  mfxU16 Picstruct() const { return mBitstream.PicStruct; }

  void SetPicstruct(mfxU16 picstruct) {
    mBitstream.PicStruct = static_cast<mfxU16>(picstruct);
  }

  mfxU32 Length() const { return mBitstream.DataLength; }

  void SetLength(mfxU32 size) {
    mBitstream.DataLength = static_cast<mfxU32>(size);
  }

  mfxU32 Offset() const { return mBitstream.DataOffset; }

  void AddOffset(mfxU32 offset) {
    mBitstream.DataOffset += static_cast<mfxU32>(offset);
  }

  void SetOffset(mfxU32 offset) {
    mBitstream.DataOffset = static_cast<mfxU32>(offset);
  }

  mfxU32 BufferSize() const { return mBitstream.MaxLength; }

  void SetPTS(mfxU64 pts) { mBitstream.TimeStamp = pts; }

  mfxU64 PTS() const { return mBitstream.TimeStamp; }

  void SetDTS(mfxI64 dts) { mBitstream.DecodeTimeStamp = dts; }

  mfxI64 DTS() const { return mBitstream.DecodeTimeStamp; }

  void Release() {
    if (mBitstream.Data) {
      _aligned_free(mBitstream.Data);
      mBitstream.Data = nullptr;
    }
  }

  void Clear() {
    Release();
    memset(&mBitstream, 0, sizeof(mBitstream));
  }

  mfxStatus Init(mfxU32 nSize) {
    Release();

    if (nSize > 0) {
      if (nullptr == (mBitstream.Data =
                          static_cast<mfxU8 *>(_aligned_malloc(nSize, 32)))) {
        return MFX_ERR_MEMORY_ALLOC;
      }

      mBitstream.MaxLength = static_cast<mfxU32>(nSize);
    }
    return MFX_ERR_NONE;
  }

  void Trim() {
    if (mBitstream.DataOffset > 0 && mBitstream.DataLength > 0) {
      memmove(mBitstream.Data, mBitstream.Data + mBitstream.DataOffset,
              mBitstream.DataLength);
      mBitstream.DataOffset = 0;
    }
  }

  mfxStatus CopyFrom(const mfxU8 *setData, mfxU32 setSize) {
    if (setData == nullptr || setSize == 0) {
      return MFX_ERR_MORE_BITSTREAM;
    }
    if (mBitstream.MaxLength < setSize) {
      Release();
      auto sts = Init(setSize);
      if (sts != MFX_ERR_NONE) {
        return sts;
      }
    }
    mBitstream.DataLength = static_cast<mfxU32>(setSize);
    mBitstream.DataOffset = 0;
    memcpy(mBitstream.Data, setData, setSize);
    return MFX_ERR_NONE;
  }

  mfxStatus CopyFrom(const mfxU8 *setData, mfxU32 setSize, mfxU64 dts,
                 int64_t pts) {
    auto sts = CopyFrom(setData, setSize);
    if (sts != MFX_ERR_NONE) {
      return sts;
    }
    mBitstream.DecodeTimeStamp = dts;
    mBitstream.TimeStamp = pts;
    return MFX_ERR_NONE;
  }

  mfxStatus CopyFrom(const QSV_VPL_Bitstream *pBitstream) {
    auto sts = CopyFrom(pBitstream->Data(), pBitstream->Length());
    if (sts != MFX_ERR_NONE) {
      return sts;
    }

    auto ptr = mBitstream.Data;
    auto offset = mBitstream.DataOffset;
    auto datalength = mBitstream.DataLength;
    auto maxLength = mBitstream.MaxLength;

    memcpy(&mBitstream, pBitstream, sizeof(pBitstream));

    mBitstream.Data = ptr;
    mBitstream.DataLength = datalength;
    mBitstream.DataOffset = offset;
    mBitstream.MaxLength = maxLength;
    return MFX_ERR_NONE;
  }

  mfxStatus ChangeSize(mfxU32 nNewSize) {
    mfxU8 *pData = static_cast<mfxU8 *>(_aligned_malloc(nNewSize, 32));
    if (pData == nullptr) {
      return MFX_ERR_NULL_PTR;
    }

    mfxU32 nDataLen = mBitstream.DataLength;
    if (mBitstream.DataLength) {
      memcpy(pData, mBitstream.Data + mBitstream.DataOffset,
             (std::min)(nDataLen, nNewSize));
    }
    Release();

    mBitstream.Data = pData;
    mBitstream.DataOffset = 0;
    mBitstream.DataLength = static_cast<mfxU32>(nDataLen);
    mBitstream.MaxLength = static_cast<mfxU32>(nNewSize);

    return MFX_ERR_NONE;
  }

  mfxStatus ExpandBuffer() {
    auto BufSize = BufferSize();
    BufSize += ((BufSize / 100) * 10);

    if (ChangeSize(BufSize) == MFX_ERR_NONE) {
      return MFX_ERR_NONE;
    }

    return MFX_ERR_NULL_PTR;
  }

  mfxStatus ExpandBuffer(mfxU32 appendSize) {
    auto mBufSize = BufferSize();

    if (ChangeSize(mBufSize + appendSize) == MFX_ERR_NONE) {
      return MFX_ERR_NONE;
    }

    return MFX_ERR_NULL_PTR;
  }

  mfxStatus Append(const mfxU8 *appendData, mfxU32 appendSize) {
    if (appendData && appendSize > 0) {
      const auto new_data_length =
          static_cast<mfxU32>(appendSize + mBitstream.DataLength);
      if (mBitstream.MaxLength < new_data_length) {
        auto sts = ChangeSize(new_data_length);
        if (sts != MFX_ERR_NONE) {
          return sts;
        }
      }

      if (mBitstream.MaxLength < new_data_length + mBitstream.DataOffset) {
        memmove(mBitstream.Data, mBitstream.Data + mBitstream.DataOffset,
                mBitstream.DataLength);
        mBitstream.DataOffset = 0;
      }
      memcpy(mBitstream.Data + mBitstream.DataLength + mBitstream.DataOffset,
             appendData, appendSize);
      mBitstream.DataLength = static_cast<mfxU32>(new_data_length);
    }
    return MFX_ERR_NONE;
  }

  mfxStatus Append(const QSV_VPL_Bitstream *pBitstream) {
    if (pBitstream != nullptr && pBitstream->Length() > 0) {
      return Append(pBitstream->Data(), pBitstream->Length());
    }
    return MFX_ERR_NONE;
  }
};

static inline QSV_VPL_Bitstream BitstreamInit() {
  QSV_VPL_Bitstream Bitstream;
  memset(&Bitstream, 0, sizeof(Bitstream));
  return Bitstream;
}