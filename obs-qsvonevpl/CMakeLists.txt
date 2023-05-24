option(ENABLE_QSV11 "Build Intel QSV11 oneVPL Hardware Encoder." TRUE)

if(NOT ENABLE_QSV11)
  obs_status(DISABLED "obs-qsv11")
  return()
endif()

project(obs-qsvonevpl)

add_library(obs-qsvonevpl MODULE)
add_library(OBS::qsvonevpl ALIAS obs-qsvonevpl)

add_library(libvpl INTERFACE)
add_library(OBS::libvpl ALIAS libvpl)

target_sources(
  libvpl
  INTERFACE libvpl/dispatcher/mfx_critical_section.h
			libvpl/dispatcher/mfx_dispatcher.h
			libvpl/dispatcher/mfx_dispatcher_defs.h
			libvpl/dispatcher/mfx_dispatcher_log.h
			libvpl/dispatcher/mfx_driver_store_loader.h
			libvpl/dispatcher/mfx_dxva2_device.h
			libvpl/dispatcher/mfx_exposed_functions_list.h
			libvpl/dispatcher/mfx_library_iterator.h
			libvpl/dispatcher/mfx_load_dll.h
			libvpl/dispatcher/mfx_vector.h
			libvpl/dispatcher/mfx_win_reg_key.h
      libvpl/dispatcher/mfx_dispatcher_vpl.h
			libvpl/dispatcher/mfx_dispatcher_vpl_log.h
			libvpl/dispatcher/mfx_dispatcher_vpl_win.h
			libvpl/api/mfx.h
			libvpl/api/mfxadapter.h
			libvpl/api/mfxbrc.h
			libvpl/api/mfxcamera.h
			libvpl/api/mfxcommon.h
			libvpl/api/mfxdefs.h
			libvpl/api/mfxdispatcher.h
			libvpl/api/mfxdispatcherprefixedfunctions.h
			libvpl/api/mfxencodestats.h
			libvpl/api/mfximplcaps.h
			libvpl/api/mfxjpeg.h
			libvpl/api/mfxmvc.h
			libvpl/api/mfxpcp.h
			libvpl/api/mfxsession.h
			libvpl/api/mfxstructures.h
			libvpl/api/mfxsurfacepool.h
			libvpl/api/mfxvideo.h
			libvpl/api/mfxvp8.h
			libvpl/api/private/mfxav1.h
			libvpl/api/private/mfxdeprecated.h
			libvpl/api/private/mfxenctools.h
			libvpl/api/private/mfxplugin.h
			libvpl/api/private/mfxpxp.h
			libvpl/api/legacy/mfxvideo++.h
			libvpl/src/main.cpp
			libvpl/src/mfx_critical_section.cpp
			libvpl/src/mfx_dispatcher.cpp
			libvpl/src/mfx_dispatcher_log.cpp
			libvpl/src/mfx_driver_store_loader.cpp
			libvpl/src/mfx_dxva2_device.cpp
			libvpl/src/mfx_function_table.cpp
			libvpl/src/mfx_library_iterator.cpp
			libvpl/src/mfx_load_dll.cpp
			libvpl/src/mfx_win_reg_key.cpp
			libvpl/src/mfx_dispatcher_vpl.cpp
			libvpl/src/mfx_dispatcher_vpl_config.cpp
			libvpl/src/mfx_dispatcher_vpl_loader.cpp
			libvpl/src/mfx_dispatcher_vpl_log.cpp
			libvpl/src/mfx_dispatcher_vpl_lowlatency.cpp
			libvpl/src/mfx_dispatcher_vpl_msdk.cpp
)

target_include_directories(
  libvpl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/
                   ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/dispatcher/
				           ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/legacy/
				           ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/private/
				           ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/src/
)

target_sources(
  obs-qsvonevpl
  PRIVATE obs-qsv-onevpl-plugin-main.c
	        obs-qsv-onevpl-plugin-init.c
          obs-qsv-onevpl-encoder.cpp
          obs-qsv-onevpl-encoder.h
	        obs-qsv-onevpl-encoder-internal.cpp
          obs-qsv-onevpl-encoder-internal.h
          helpers/common_directx11.cpp
          helpers/common_directx11.h
          helpers/common_utils.cpp
          helpers/common_utils.h
          helpers/common_utils_windows.cpp
          bits/linux_defs.h
          bits/windows_defs.h
)

target_link_libraries(
  obs-qsvonevpl
  PRIVATE OBS::libobs
          OBS::libvpl
          d3d9
          d3d11
          dxva2
          dxgi
          dxguid
)

target_compile_definitions(obs-qsvonevpl PRIVATE DX11_D3D)

if(OS_WINDOWS)
  add_subdirectory(obs-qsv-test)

  set(MODULE_DESCRIPTION "OBS QSV oneVPL encoder")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in
                 obs-qsvonevpl.rc)

  target_sources(obs-qsvonevpl PRIVATE obs-qsvonevpl.rc)

  target_compile_definitions(
    obs-qsvonevpl PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
                      _CRT_NONSTDC_NO_WARNINGS)
endif()

set_target_properties(obs-qsvonevpl PROPERTIES FOLDER "plugins/obs-qsvonevpl")

file(GLOB _OBS_QSVONEVPL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB _OBS_QSVONEVPL_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

source_group("obs-qsvonevpl\\Source Files" FILES ${_OBS_QSVONEVPL_SOURCE_FILES})
source_group("obs-qsvonevpl\\Header Files" FILES ${_OBS_QSVONEVPL_HEADER_FILES})

file(GLOB _OBS_QSVHELPERS_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.cpp)
file(GLOB _OBS_QSVHELPERS_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.hpp)

source_group("obs-qsvonevpl-helpers\\Source Files" FILES ${_OBS_QSVHELPERS_SOURCE_FILES})
source_group("obs-qsvonevpl-helpers\\Header Files" FILES ${_OBS_QSVHELPERS_HEADER_FILES})

get_target_property(_LIBVPL_SOURCES OBS::libvpl INTERFACE_SOURCES)

foreach(_LIBVPL_SOURCE ${_LIBVPL_SOURCES})
  get_filename_component(_EXT ${_LIBVPL_SOURCE} EXT)
  if(${_EXT} STREQUAL "hpp" OR ${_EXT} STREQUAL "h")
    source_group("libvpl\\Header Files" FILES ${_LIBVPL_SOURCE})
  elseif(${_EXT} STREQUAL "cpp" OR ${_EXT} STREQUAL "c")
    source_group("libvpl\\Source Files" FILES ${_LIBVPL_SOURCE})
  endif()
endforeach()

setup_plugin_target(obs-qsvonevpl)
