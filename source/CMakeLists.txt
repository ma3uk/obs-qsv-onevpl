option(ENABLE_QSV11 "Build Intel QSV11 Hardware Encoder." TRUE)

if(NOT ENABLE_QSV11)
  obs_status(DISABLED "obs-qsv11")
  return()
endif()

project(obs-qsv11)

add_library(obs-qsv11 MODULE)
add_library(OBS::qsv11 ALIAS obs-qsv11)

add_library(libmfx INTERFACE)
add_library(OBS::libmfx ALIAS libmfx)

target_sources(
  libmfx
  INTERFACE libmfx/include/vpl/mfx_critical_section.h
			libmfx/include/vpl/mfx_dispatcher.h
			libmfx/include/vpl/mfx_dispatcher_defs.h
			libmfx/include/vpl/mfx_dispatcher_log.h
			libmfx/include/vpl/mfx_driver_store_loader.h
			libmfx/include/vpl/mfx_dxva2_device.h
			libmfx/include/vpl/mfx_exposed_functions_list.h
			libmfx/include/vpl/mfx_library_iterator.h
			libmfx/include/vpl/mfx_load_dll.h
			libmfx/include/vpl/mfx_vector.h
			libmfx/include/vpl/mfx_win_reg_key.h
			libmfx/include/vpl/include/mfx.h
			libmfx/include/vpl/include/mfxadapter.h
			libmfx/include/vpl/include/mfxbrc.h
			libmfx/include/vpl/include/mfxcamera.h
			libmfx/include/vpl/include/mfxcommon.h
			libmfx/include/vpl/include/mfxdefs.h
			libmfx/include/vpl/include/mfxdispatcher.h
			libmfx/include/vpl/include/mfxdispatcherprefixedfunctions.h
			libmfx/include/vpl/include/mfxencodestats.h
			libmfx/include/vpl/include/mfximplcaps.h
			libmfx/include/vpl/include/mfxjpeg.h
			libmfx/include/vpl/include/mfxmvc.h
			libmfx/include/vpl/include/mfxpcp.h
			libmfx/include/vpl/include/mfxsession.h
			libmfx/include/vpl/include/mfxstructures.h
			libmfx/include/vpl/include/mfxsurfacepool.h
			libmfx/include/vpl/include/mfxvideo.h
			libmfx/include/vpl/include/mfxvp8.h
			libmfx/include/vpl/include/private/mfxav1.h
			libmfx/include/vpl/include/private/mfxdeprecated.h
			libmfx/include/vpl/include/private/mfxenctools.h
			libmfx/include/vpl/include/private/mfxplugin.h
			libmfx/include/vpl/include/private/mfxpxp.h
			libmfx/include/vpl/include/legacy/mfxvideo++.h
			libmfx/include/vpl/mfx_dispatcher_vpl.h
			libmfx/include/vpl/mfx_dispatcher_vpl_log.h
			libmfx/include/vpl/mfx_dispatcher_vpl_win.h
			libmfx/src/vpl/main.cpp
			libmfx/src/vpl/mfx_critical_section.cpp
			libmfx/src/vpl/mfx_dispatcher.cpp
			libmfx/src/vpl/mfx_dispatcher_log.cpp
			libmfx/src/vpl/mfx_driver_store_loader.cpp
			libmfx/src/vpl/mfx_dxva2_device.cpp
			libmfx/src/vpl/mfx_function_table.cpp
			libmfx/src/vpl/mfx_library_iterator.cpp
			libmfx/src/vpl/mfx_load_dll.cpp
			libmfx/src/vpl/mfx_win_reg_key.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl_config.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl_loader.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl_log.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl_lowlatency.cpp
			libmfx/src/vpl/mfx_dispatcher_vpl_msdk.cpp)

target_include_directories(
  libmfx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include/vpl/include/
                   ${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include/vpl/
				   ${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include/vpl/include/legacy/
				   ${CMAKE_CURRENT_SOURCE_DIR}/libmfx/include/vpl/include/
				   ${CMAKE_CURRENT_SOURCE_DIR}libmfx/src/vpl/enctool/
				   )

target_sources(
  obs-qsv11
  PRIVATE obs-qsv11.c
          obs-qsv11-plugin-main.c
          common_directx9.cpp
          common_directx9.h
          common_directx11.cpp
          common_directx11.h
          common_utils.cpp
          common_utils.h
          common_utils_windows.cpp
          device_directx9.cpp
          device_directx9.h
          QSV_Encoder.cpp
          QSV_Encoder.h
          QSV_Encoder_Internal.cpp
          QSV_Encoder_Internal.h
		  QSV_Encoder_Internal_new.cpp
          QSV_Encoder_Internal_new.h
          bits/linux_defs.h
          bits/windows_defs.h)

target_link_libraries(
  obs-qsv11
  PRIVATE OBS::libobs
          OBS::libmfx
          d3d9
          d3d11
          dxva2
          dxgi
          dxguid)

target_compile_definitions(obs-qsv11 PRIVATE DX11_D3D)

if(OS_WINDOWS)
  add_subdirectory(obs-qsv-test)

  set(MODULE_DESCRIPTION "OBS QSV encoder")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-qsv11.rc)

  target_sources(obs-qsv11 PRIVATE obs-qsv11.rc)

  target_compile_definitions(
    obs-qsv11 PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                      _CRT_NONSTDC_NO_WARNINGS)
endif()

set_target_properties(obs-qsv11 PROPERTIES FOLDER "plugins/obs-qsv11")

file(GLOB _OBS_QSV11_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB _OBS_QSV11_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

source_group("obs-qsv11\\Source Files" FILES ${_OBS_QSV11_SOURCE_FILES})
source_group("obs-qsv11\\Header Files" FILES ${_OBS_QSV11_HEADER_FILES})

get_target_property(_LIBMFX_SOURCES OBS::libmfx INTERFACE_SOURCES)

foreach(_LIBMFX_SOURCE ${_LIBMFX_SOURCES})
  get_filename_component(_EXT ${_LIBMFX_SOURCE} EXT)
  if(${_EXT} STREQUAL "hpp" OR ${_EXT} STREQUAL "h")
    source_group("libmfx\\Header Files" FILES ${_LIBMFX_SOURCE})
  elseif(${_EXT} STREQUAL "cpp" OR ${_EXT} STREQUAL "c")
    source_group("libmfx\\Source Files" FILES ${_LIBMFX_SOURCE})
  endif()
endforeach()

setup_plugin_target(obs-qsv11)
