cmake_minimum_required(VERSION 3.24...3.25)

project(obs-qsvonevpl)

add_library(obs-qsvonevpl MODULE)
add_library(OBS::qsvonevpl ALIAS obs-qsvonevpl)

add_library(libvpl INTERFACE)
add_library(OBS::libvpl ALIAS libvpl)

target_sources(
  obs-qsvonevpl
  PRIVATE # cmake-format: sortable
	  obs-qsv-onevpl-plugin-main.c
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
	  helpers/ext_buf_manager.h
      bits/linux_defs.h
      bits/windows_defs.h
)

target_link_libraries(obs-qsvonevpl PRIVATE OBS::libobs)

# cmake-format: off
set_target_properties_obs(obs-qsvonevpl PROPERTIES FOLDER plugins/obs-qsvonevpl PREFIX "")
# cmake-format: on

if(OS_WINDOWS)
  add_subdirectory(obs-qsvonevpl-test)

target_sources(
  libvpl
  INTERFACE 		# cmake-format: sortable
			libvpl/dispatcher/mfx_critical_section.h
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
				   ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/structures/
				   ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/legacy/
				   ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/private/
                   ${CMAKE_CURRENT_SOURCE_DIR}/libvpl/api/src/
)

get_target_property(mfx_sources libvpl SOURCES)
  list(FILTER mfx_sources INCLUDE REGEX ".*\\.(m|c[cp]?p?|swift)")
  list(FILTER mfx_headers INCLUDE REGEX ".*\\.h(pp)?")
  source_group("libvpl\\Source Files" FILES ${mfx_sources})
  source_group("libvpl\\Header Files" FILES ${mfx_headers})

 target_compile_definitions(obs-qsvonevpl PRIVATE DX11_D3D)

target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
            helpers/common_directx11.cpp
            helpers/common_directx11.h
            helpers/common_utils.cpp)
  
target_link_libraries(obs-qsvonevpl PRIVATE OBS::libvpl d3d11 dxgi dxguid OBS::COMutils)

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
endif()

get_target_property(target_sources obs-qsvonevpl SOURCES)
list(FILTER target_sources INCLUDE REGEX ".*\\.(m|c[cp]?p?|swift)")
list(FILTER target_headers INCLUDE REGEX ".*\\.h(pp)?")
source_group("obs-qsvonevpl\\Source Files" FILES ${target_sources})
source_group("obs-qsvonevpl\\Header Files" FILES ${target_headers})