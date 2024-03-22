#ifndef __QSV_VPL_WINDOWS_DEFS_H__
#define __QSV_VPL_WINDOWS_DEFS_H__
#endif
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define D3D11_IGNORE_SDK_LAYERS

#ifndef __INTRIN_H_
#include <intrin.h>
#endif
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#ifndef __d3d10_1_h__
#include <d3d10_1.h>
#endif
#ifndef __d3d11_h__
#include <d3d11.h>
#endif
#ifndef __d3d11_1_h__
#include <d3d11_1.h>
#endif
#ifndef __d3dcommon_h__
#include <d3dcommon.h>
#endif
#ifndef __dxgi_h__
#include <dxgi.h>
#endif
#ifndef __dxgi1_6_h__
#include <dxgi1_6.h>
#endif
#ifndef __dxgitype_h__
#include <dxgitype.h>
#endif

#include <util/windows/device-enum.h>

#endif
