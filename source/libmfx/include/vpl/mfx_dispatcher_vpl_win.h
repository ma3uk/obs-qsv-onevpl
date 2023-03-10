/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_VPL_MFX_DISPATCHER_VPL_WIN_H_
#define DISPATCHER_VPL_MFX_DISPATCHER_VPL_WIN_H_

// headers for Windows legacy dispatcher
#if defined(_WIN32) || defined(_WIN64)
    #include "mfx_dispatcher.h"
    #include "mfx_dispatcher_defs.h"
    #include "mfx_dxva2_device.h"
    #include "mfx_library_iterator.h"
    #include "mfx_load_dll.h"
#endif

#endif // DISPATCHER_VPL_MFX_DISPATCHER_VPL_WIN_H_
