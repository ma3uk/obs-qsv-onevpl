# obs-qsv-onevpl
obs-qsv11 plugin with oneVPL support

Implementation of the obs-sv 11 plugin with oneVPL support for Intel Tiger Lake processors and higher, as well as discrete DG1 and Arc Alchemist video cards.
Fully working plugin, but need help with rewriting memory allocation to new internal memory
At the moment, backward compatibility is maintained for Intel Ice Lake and below.
Removed the code for the D3D9 implementation.

TODO:
- Use Internal memory allocator (https://spec.oneapi.io/versions/latest/elements/oneVPL/source/programming_guide/VPL_prg_mem.html#internal-memory-management)
- Using a new hardware acceleration model to get rid of D3D dependencies (common_directx11.cpp) (https://spec.oneapi.io/versions/latest/elements/oneVPL/source/programming_guide/VPL_prg_hw.html#new-model-to-work-with-hardware-acceleration)
- ~~Rewrite obs-qsv-test to determine support for AV1 and discrete cards based on CPUID (Definition of processors with HEVC support has already been implemented)~~
- Fix MFX_ERR_DEVICE_FAIL on Xe video drivers
- 
The latest ready-to-use version of the DLL for integration into OBS-Studio is in the builds folder.
