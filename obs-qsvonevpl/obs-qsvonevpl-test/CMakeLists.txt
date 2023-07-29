cmake_minimum_required(VERSION 3.24...3.25)

legacy_check()

add_executable(obs-qsvonevpl-test)

target_sources(obs-qsvonevpl-test PRIVATE obs-qsv-test.cpp)
target_compile_definitions(obs-qsvonevpl-test PRIVATE "$<$<BOOL:${ENABLE_HEVC}>:ENABLE_HEVC>")
target_link_libraries(obs-qsvonevpl-test d3d11 dxgi dxguid OBS::libvpl OBS::COMutils)

# cmake-format: off
set_target_properties_obs(obs-qsvonevpl-test PROPERTIES FOLDER plugins/obs-qsvonevpl PREFIX "")
# cmake-format: on