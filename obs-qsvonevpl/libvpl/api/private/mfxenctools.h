/******************************************************************************* *\

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

#include "mfxvideo.h"
//#include "mfxbrc.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_ENCTOOLS_CONFIG = MFX_MAKEFOURCC('E', 'T', 'C', 'F') 
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct
{
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            AdaptiveI;
    mfxU16            AdaptiveB;
    mfxU16            AdaptiveRefP;
    mfxU16            AdaptiveRefB;
    mfxU16            SceneChange;
    mfxU16            AdaptiveLTR;
    mfxU16            AdaptivePyramidQuantP;
    mfxU16            AdaptivePyramidQuantB;
    mfxU16            AdaptiveQuantMatrices;
    mfxU16            AdaptiveMBQP;
    mfxU16            BRCBufferHints;
    mfxU16            BRC;
    mfxU16            SaliencyMapHint;
    mfxU16            reserved[19];
} mfxExtEncToolsConfig;
MFX_PACK_END()

#define MFX_ENCTOOLS_CONFIG_VERSION MFX_STRUCT_VERSION(1, 0)

//#define MFX_EXTBUFF_DDI MFX_MAKEFOURCC('D', 'D', 'I', 'P')

enum {
	MFX_EXTBUFF_DDI = MFX_MAKEFOURCC('D', 'D', 'I', 'P')
};
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer Header;

    // parameters below doesn't exist in MediaSDK public API
    mfxU16 IntraPredCostType; // from DDI: 1=SAD, 2=SSD, 4=SATD_HADAMARD, 8=SATD_HARR
    mfxU16 MEInterpolationMethod; // from DDI: 1=VME4TAP, 2=BILINEAR, 4=WMV4TAP, 8=AVC6TAP
    mfxU16 MEFractionalSearchType; // from DDI: 1=FULL, 2=HALF, 4=SQUARE, 8=HQ, 16=DIAMOND
    mfxU16 MaxMVs;
    mfxU16 SkipCheck; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 DirectCheck; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 BiDirSearch; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 MBAFF; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 FieldPrediction; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 RefOppositeField; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 ChromaInME; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 WeightedPrediction; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 MVPrediction; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON

    struct {
	    mfxU16 IntraPredBlockSize; // mask of 1=4x4, 2=8x8, 4=16x16, 8=PCM
	    mfxU16 InterPredBlockSize; // mask of 1=16x16, 2=16x8, 4=8x16, 8=8x8, 16=8x4, 32=4x8, 64=4x4
    } DDI;

    // MediaSDK parametrization
    mfxU16 BRCPrecision; // 0=default=normal, 1=lowest, 2=normal, 3=highest
    mfxU16 RefRaw; // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON) on=vme reference on raw(input) frames, off=reconstructed frames
    mfxU16 reserved0;
    mfxU16 ConstQP;      // disable bit-rate control and use constant QP
    mfxU16 GlobalSearch; // 0=default, 1=long, 2=medium, 3=short
    mfxU16 LocalSearch; // 0=default, 1=type, 2=small, 3=square, 4=diamond, 5=large diamond, 6=exhaustive, 7=heavy horizontal, 8=heavy vertical

    mfxU16 EarlySkip;   // 0=default (let driver choose), 1=enabled, 2=disabled
    mfxU16 LaScaleFactor; // 0=default (let msdk choose), 1=1x, 2=2x, 4=4x; Deprecated for legacy H264 encoder, for legacy use mfxExtCodingOption2::LookAheadDS instead
    mfxU16 IBC;           // on/off - SCC IBC Mode
    mfxU16 Palette;       // on/off - SCC Palette Mode
    mfxU16 StrengthN;     // strength=StrengthN/100.0
    mfxU16 FractionalQP;  // 0=disabled (default), 1=enabled

    mfxU16 NumActiveRefP; //
    mfxU16 NumActiveRefBL0;        //

    mfxU16 DisablePSubMBPartition; // tri-state, default depends on Profile and Level
    mfxU16 DisableBSubMBPartition; // tri-state, default depends on Profile and Level
    mfxU16 WeightedBiPredIdc; // 0 - off, 1 - explicit (unsupported), 2 - implicit
    mfxU16 DirectSpatialMvPredFlag; // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON)on=spatial on, off=temporal on
    mfxU16 Transform8x8Mode; // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 reserved3;        // 0..255
    mfxU16 LongStartCodes; // tri-state, use long start-codes for all NALU
    mfxU16 CabacInitIdcPlus1; // 0 - use default value, 1 - cabac_init_idc = 0 and so on
    mfxU16 NumActiveRefBL1;     //
    mfxU16 QpUpdateRange;       //
    mfxU16 RegressionWindow;    //
    mfxU16 LookAheadDependency; // LookAheadDependency < LookAhead
    mfxU16 Hme;                 // tri-state
    mfxU16 reserved4;           //
    mfxU16 WriteIVFHeaders;     // tri-state
    mfxU16 RefreshFrameContext;
    mfxU16 ChangeFrameContextIdxForTS;
    mfxU16 SuperFrameForTS;
    mfxU16 QpAdjust; // on/off - forces sps.QpAdjustment
    mfxU16 TMVP;     // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON

} mfxExtCodingOptionDDI;
MFX_PACK_END()



#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif

