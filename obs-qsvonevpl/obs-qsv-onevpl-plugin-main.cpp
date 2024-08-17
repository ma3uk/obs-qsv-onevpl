/*

This file is provided under a dual BSD/GPLv2 license.  When using or
redistributing this file, you may do so under either license.

GPL LICENSE SUMMARY

Copyright(c) Oct. 2015 Intel Corporation.

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Contact Information:

Seung-Woo Kim, seung-woo.kim@intel.com
705 5th Ave S #500, Seattle, WA 98104

BSD LICENSE

Copyright(c) <date> Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "helpers/common_utils.hpp"
//#include "obs-qsv-onevpl-encoder.hpp"

#include <obs-module.h>
#include <obs.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-qsvonevpl", "en-US");
MODULE_EXPORT const char *obs_module_description(void) {
  return "Intel Quick Sync Video support for Windows (oneVPL)";
}

extern obs_encoder_info H264FrameEncoderInfo;
extern obs_encoder_info H264TextureEncoderInfo;

extern obs_encoder_info AV1FrameEncoderInfo;
extern obs_encoder_info AV1TextureEncoderInfo;

extern obs_encoder_info HEVCFrameEncoderInfo;
extern obs_encoder_info HEVCTextureEncoderInfo;




bool obs_module_load([[maybe_unused]] void) {
  AdaptersCount = MAX_ADAPTERS;
  GetAdaptersInfo(AdaptersInfo, &AdaptersCount);

  bool SupportAVC = false;
  bool SupportAV1 = false;
  bool SupportHEVC = false;
  for (size_t i = 0; i < AdaptersCount; i++) {
    struct adapter_info *AdapterInfo = &AdaptersInfo[i];
    SupportAVC |= AdapterInfo->IsIntel;
    SupportAV1 |= AdapterInfo->SupportAV1;
    SupportHEVC |= AdapterInfo->SupportHEVC;
  }

  if (SupportAVC) {
    obs_register_encoder(&H264FrameEncoderInfo);
    obs_register_encoder(&H264TextureEncoderInfo);
    info( "QSV AVC support");
  }
  if (SupportAV1) {
    obs_register_encoder(&AV1FrameEncoderInfo);
    obs_register_encoder(&AV1TextureEncoderInfo);
    info( "QSV AV1 support");
  }
  if (SupportHEVC) {
    obs_register_encoder(&HEVCFrameEncoderInfo);
    obs_register_encoder(&HEVCTextureEncoderInfo);
    info( "QSV HEVC support");
  }


  return true;
}
