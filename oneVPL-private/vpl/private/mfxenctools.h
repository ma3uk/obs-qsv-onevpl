/*******************************************************************************
*\

Copyright (C) 2019-2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxenctools.h

*******************************************************************************/
#ifndef __MFXENCTOOLS_H__
#define __MFXENCTOOLS_H__

#define MFX_ENABLE_ENCTOOLS_LPLA

#include <vpl/mfxbrc.h>
#include <vpl/mfxvideo.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Extended Buffer Ids */
enum {
  MFX_EXTBUFF_ENCTOOLS_CONFIG = MFX_MAKEFOURCC('E', 'T', 'C', 'F'),
  MFX_EXTBUFF_ENCTOOLS = MFX_MAKEFOURCC('E', 'E', 'T', 'L'),
  MFX_EXTBUFF_ENCTOOLS_DEVICE = MFX_MAKEFOURCC('E', 'T', 'E', 'D'),
  MFX_EXTBUFF_ENCTOOLS_ALLOCATOR = MFX_MAKEFOURCC('E', 'T', 'E', 'A'),
  MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE = MFX_MAKEFOURCC('E', 'F', 'T', 'A'),
  MFX_EXTBUFF_ENCTOOLS_HINT_SCENE_CHANGE = MFX_MAKEFOURCC('E', 'H', 'S', 'C'),
  MFX_EXTBUFF_ENCTOOLS_HINT_GOP = MFX_MAKEFOURCC('E', 'H', 'G', 'O'),
  MFX_EXTBUFF_ENCTOOLS_HINT_AREF = MFX_MAKEFOURCC('E', 'H', 'A', 'R'),
  MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT = MFX_MAKEFOURCC('E', 'B', 'B', 'H'),
  MFX_EXTBUFF_ENCTOOLS_BRC_FRAME_PARAM = MFX_MAKEFOURCC('E', 'B', 'F', 'P'),
  MFX_EXTBUFF_ENCTOOLS_BRC_QUANT_CONTROL = MFX_MAKEFOURCC('E', 'B', 'Q', 'C'),
  MFX_EXTBUFF_ENCTOOLS_BRC_HRD_POS = MFX_MAKEFOURCC('E', 'B', 'H', 'P'),
  MFX_EXTBUFF_ENCTOOLS_BRC_ENCODE_RESULT = MFX_MAKEFOURCC('E', 'B', 'E', 'R'),
  MFX_EXTBUFF_ENCTOOLS_BRC_STATUS = MFX_MAKEFOURCC('E', 'B', 'S', 'T'),
  MFX_EXTBUFF_ENCTOOLS_HINT_MATRIX = MFX_MAKEFOURCC('E', 'H', 'Q', 'M'),
  MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP = MFX_MAKEFOURCC('E', 'H', 'Q', 'P'),
  MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP = MFX_MAKEFOURCC('E', 'H', 'S', 'M'),
  MFX_EXTBUFF_ENCTOOLS_PREFILTER_PARAM = MFX_MAKEFOURCC('E', 'P', 'R', 'P'),
  MFX_EXTBUFF_LP_LOOKAHEAD = MFX_MAKEFOURCC('L', 'P', 'L', 'A'),
  MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK = MFX_MAKEFOURCC('A', 'P', 'B', 'K')
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
  mfxExtBuffer Header;
  mfxStructVersion Version;
  mfxU16 AdaptiveI;
  mfxU16 AdaptiveB;
  mfxU16 AdaptiveRefP;
  mfxU16 AdaptiveRefB;
  mfxU16 SceneChange;
  mfxU16 AdaptiveLTR;
  mfxU16 AdaptivePyramidQuantP;
  mfxU16 AdaptivePyramidQuantB;
  mfxU16 AdaptiveQuantMatrices;
  mfxU16 AdaptiveMBQP;
  mfxU16 BRCBufferHints;
  mfxU16 BRC;
  mfxU16 SaliencyMapHint;
  mfxU16 reserved[19];
} mfxExtEncToolsConfig;
MFX_PACK_END()

#define MFX_ENCTOOLS_CONFIG_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
  mfxExtBuffer Header;
  mfxStructVersion Version;
  mfxU16 reserved[3];
  mfxU32 QpY;          /* Frame-level Luma QP. Mandatory */
  mfxU32 MaxFrameSize; /* Max frame size in bytes (used for rePak). Optional */
  mfxU8
      DeltaQP[8]; /* deltaQP[i] is added to QP value while ith-rePakOptional */
  mfxU16 NumDeltaQP; /* Max number of rePaks to provide MaxFrameSize (from 0 to
                        8) */
  mfxU16 reserved2[5];
} mfxEncToolsBRCQuantControl;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCQUANTCONTROL_VERSION MFX_STRUCT_VERSION(1, 0);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif
