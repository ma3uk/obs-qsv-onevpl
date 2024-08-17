option(ENABLE_HEVC "Enable HEVC support" ON)

add_library(obs-qsvonevpl MODULE)

find_package(VPL CONFIG)
find_package(libobs CONFIG)

target_sources(
  obs-qsvonevpl
  PRIVATE # cmake-format: sortable
  obs-qsv-onevpl-plugin-main.cpp
  obs-qsv-onevpl-plugin-init.cpp
  obs-qsv-onevpl-plugin-init.hpp
  obs-qsv-onevpl-encoder.cpp
  obs-qsv-onevpl-encoder.hpp
  obs-qsv-onevpl-encoder-internal.cpp
  obs-qsv-onevpl-encoder-internal.hpp
  helpers/common_utils.cpp
  helpers/common_utils.hpp
  helpers/ext_buf_manager.hpp
  helpers/qsv_params.hpp
)

target_link_libraries(obs-qsvonevpl PRIVATE VPL libvpl-private OBS::libobs)

# cmake-format: off
set_target_properties(obs-qsvonevpl PROPERTIES
  FOLDER "plugins/obs-qsvonevpl"
  PREFIX ""
)
# cmake-format: on

# ONEVPL_EXPERIMENTAL
target_compile_definitions(
  obs-qsvonevpl
  PRIVATE
  ONEVPL_EXPERIMENTAL
  MFXVIDEO_CPP_ENABLE_MFXLOAD
  $<$<BOOL:${ENABLE_HEVC}>:ENABLE_HEVC=1>
)

# C++20 for C-like structs
target_compile_features(obs-qsvonevpl PRIVATE cxx_std_23)

if(WIN32)
target_compile_options(obs-qsvonevpl PRIVATE "/O2")  
target_compile_options(obs-qsvonevpl PRIVATE "/Ot")
target_compile_options(obs-qsvonevpl PRIVATE "/Oi")
target_compile_options(obs-qsvonevpl PRIVATE "/MP")
target_compile_options(obs-qsvonevpl PRIVATE "/Qpar")
target_compile_options(obs-qsvonevpl PRIVATE "/GL")
target_compile_options(obs-qsvonevpl PRIVATE "/Gy")
endif()

# Windows
if(WIN32)
  target_compile_definitions(obs-qsvonevpl PRIVATE DX11_D3D)

  target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
    bits/windows_defs.hpp
    helpers/hw_d3d11.cpp
    helpers/hw_d3d11.hpp
    helpers/common_utils_windows.cpp)

  target_link_libraries(obs-qsvonevpl PRIVATE d3d11 dxgi dxguid)

  # source_group is to show the files in the IDE #
  # Main sources and headers
  file(GLOB _QSVONEVPL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
  file(GLOB _QSVONEVPL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
  source_group("obs-qsvonevpl\\Source Files" FILES ${_QSVONEVPL_SOURCES})
  source_group("obs-qsvonevpl\\Header Files" FILES ${_QSVONEVPL_HEADERS})

  # Helpers
  file(GLOB _QSVONEVPL_HELPERS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.cpp)
  file(GLOB _QSVONEVPL_HELPERS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/helpers/*.hpp)
  source_group("obs-qsvonevpl-helpers\\Source Files" FILES ${_QSVONEVPL_HELPERS_SOURCES})
  source_group("obs-qsvonevpl-helpers\\Header Files" FILES ${_QSVONEVPL_HELPERS_HEADERS})

  # Bit headers (.h)
  file(GLOB _QSVONEVPL_BITS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/bits/*.h)
  source_group("obs-qsvonevpl-bits\\Header Files" FILES ${_QSVONEVPL_BITS_HEADERS})

  # Filter target sources and headers
  get_target_property(target_sources obs-qsvonevpl SOURCES)
  list(FILTER target_sources INCLUDE REGEX ".*\\.(m|c[cp]?p?|swift)")
  list(FILTER target_headers INCLUDE REGEX ".*\\.h(pp)?")
  source_group("obs-qsvonevpl\\Source Files" FILES ${target_sources})
  source_group("obs-qsvonevpl\\Header Files" FILES ${target_headers})

  # Linux
elseif(UNIX AND NOT APPLE)
  target_sources(
    obs-qsvonevpl
    PRIVATE # cmake-format: sortable
    bits/linux_defs.hpp
    helpers/common_utils_linux.cpp)

  target_link_libraries(obs-qsvonevpl PRIVATE va va-drm va-wayland va-x11)

  # pkg-config is used to find other libraries
  find_package(PkgConfig REQUIRED)

  # Find and link to wayland-client
  pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
  target_link_libraries(obs-qsvonevpl PRIVATE PkgConfig::WAYLAND_CLIENT)
endif()