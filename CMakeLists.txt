cmake_minimum_required(VERSION 3.24...3.25)

project(obs-qsv-onevpl
  VERSION 1.1.0.0
  LANGUAGES CXX
)

# If building as subdirectory..
if(NOT PROJECT_IS_TOP_LEVEL)
  # Remove warnings as errors, needed for in-OBS builds
  if(UNIX)
    add_compile_options(-Wno-error)
  elseif(WIN32)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_WARNING_AS_ERROR OFF)
  endif()
endif()

# VPL
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(oneVPL)

# VPL-private (experimental / hidden stuff)
add_subdirectory(oneVPL-private)

# Actual plugin
add_subdirectory(obs-qsvonevpl)
