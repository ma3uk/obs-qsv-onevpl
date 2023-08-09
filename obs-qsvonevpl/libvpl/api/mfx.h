/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFX_H__
#define __MFX_H__

#define _MFX_CONFIG_ENCODE_H_
#define ONEVPL_EXPERIMENTAL
#define MFX_ENABLE_ENCTOOLS
#define MFX_ENABLE_ADAPTIVE_ENCODE
#define MFX_ENABLE_H264_REPARTITION_CHECK
#define MFX_ENABLE_AVC_CUSTOM_QMATRIX
#define MFX_ENABLE_APQ_LQ
#define MFX_ENABLE_H264_REPARTITION_CHECK
#define MFX_ENABLE_H264_ROUNDING_OFFSET
#define MFX_ENABLE_AVC_CUSTOM_QMATRIX
#define MFX_ENABLE_AVCE_VDENC_B_FRAMES
#define MFX_ENABLE_MCTF_IN_AVC
#define MFX_ENABLE_EXT
#define MFX_ENABLE_FADE_DETECTION
#define MFX_ENABLE_EXT_BRC
#define MFX_ENABLE_VIDEO_BRC_COMMON
#define MFXVIDEO_CPP_ENABLE_MFXLOAD
#define MFX_ENABLE_LPLA_BASE


#include "mfxdefs.h"
#include "mfxcommon.h"
#include "mfxstructures.h"
#include "mfxdispatcher.h"
#include "mfximplcaps.h"
#include "mfxsession.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxadapter.h"

#include "mfxbrc.h"

#include "mfxenctools.h"
#include "mfxav1.h"
#include "mfxddi.h"

#include "mfxsurfacepool.h"

//#ifdef ONEVPL_EXPERIMENTAL
//#include "mfxencodestats.h"
//#endif

#endif /* __MFXDEFS_H__ */
