#include "common_utils.hpp"

#if DX11_D3D
#include "common_directx11.hpp"
#endif

#include <util/config-file.h>
#include <util/dstr.h>
#include <util/pipe.h>
#include <util/platform.h>
#include <util/windows/device-enum.h>

#include <intrin.h>
#include <inttypes.h>

/* =======================================================
 * Windows implementation of OS-specific utility functions
 */
mfxStatus Initialize(mfxSession *mfx_Session,
                     mfxHDL *mfx_GFXHandle, int deviceNum, bool mfx_UseTexAlloc,
                     [[maybe_unused]] enum qsv_codec codec,
                     [[maybe_unused]] void **data) {

  mfxStatus sts = MFX_ERR_NONE;

  // Initialize Intel VPL Session
  mfxLoader mfx_Loader = MFXLoad();
  mfxConfig mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfxVariant mfx_LoaderVariant{};

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.Impl.mfxImplType"),
      mfx_LoaderVariant);

  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = static_cast<mfxU32>(0x8086);
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.VendorID"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_PTR;
  mfx_LoaderVariant.Data.Ptr = mfxHDL("mfx-gen");
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.ImplName"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>(
          "mfxImplDescription.mfxAccelerationModeDescription.Mode"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_HANDLE_D3D11_DEVICE;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>("mfxHandleType"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig, reinterpret_cast<const mfxU8 *>("mfxInitParam.GPUCopy"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxInitializationParam.DeviceCopy"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>("DeviceCopy"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>(
                                 "mfxInitializationParam.AccelerationMode"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_PRIORITY_HIGH;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>(
          "mfxInitializationParam.mfxExtThreadsParam.Priority"),
      mfx_LoaderVariant);

  sts = MFXCreateSession(mfx_Loader, 0, mfx_Session);
  if (sts < MFX_ERR_NONE) {
    warn("CreateSession error: %d", sts);
    return sts;
  }

  // Create DirectX device context
  if (mfx_GFXHandle == NULL || *mfx_GFXHandle == NULL) {
    sts = CreateHWDevice(*mfx_Session, mfx_GFXHandle, deviceNum);
    if (sts < MFX_ERR_NONE) {
      warn("CreateHWDevice error: %d", sts);
      return sts;
    }
  }

  if (mfx_GFXHandle == NULL || *mfx_GFXHandle == NULL)
    return MFX_ERR_DEVICE_FAILED;

  // Provide device manager to VPL
  sts = MFXVideoCORE_SetHandle(*mfx_Session, MFX_HANDLE_D3D11_DEVICE,
                               *mfx_GFXHandle);
  if (sts < MFX_ERR_NONE) {
    warn("SetHandle error: %d", sts);
    return sts;
  }

  mfxPlatform mfx_Platform{};
  MFXVideoCORE_QueryPlatform(*mfx_Session, &mfx_Platform);
  if (mfx_Platform.MediaAdapterType == MFX_MEDIA_DISCRETE) {
    info("\tAdapter type: Discrete");
  } else {
    info("\tAdapter type: Integrate");
  }

  return sts;
}

void Release() {
#if defined(_WIN32) || defined(_WIN64)
  CleanupHWDevice();
#endif
}

void ReleaseSessionData(void *) {}

void util_cpuid(int cpuinfo[4], int flags) { return __cpuid(cpuinfo, flags); }

static bool enum_luids(void *param, uint32_t idx, uint64_t luid) {
  dstr *cmd = static_cast<dstr *>(param);
  dstr_catf(cmd, " %llX", luid);
  UNUSED_PARAMETER(idx);
  return true;
}

void check_adapters(struct adapter_info *adapters, size_t *adapter_count) {
  char *test_exe = os_get_executable_path_ptr("obs-qsv-test.exe");
  struct dstr cmd = {0};
  struct dstr caps_str = {0};
  os_process_pipe_t *pp = nullptr;
  config_t *config = nullptr;
  const char *error = nullptr;
  size_t config_adapter_count;

  dstr_init_move_array(&cmd, test_exe);
  dstr_insert_ch(&cmd, 0, '\"');
  dstr_cat(&cmd, "\"");

  enum_graphics_device_luids(enum_luids, &cmd);

  pp = os_process_pipe_create(cmd.array, "r");
  if (!pp) {
    info("Failed to launch the QSV test process I guess");
    goto fail;
  }

  for (;;) {
    char data[2048]{};
    size_t len = os_process_pipe_read(pp, reinterpret_cast<uint8_t *>(data),
                                      sizeof(data));
    if (!len)
      break;

    dstr_ncat(&caps_str, data, len);
  }

  if (dstr_is_empty(&caps_str)) {
    info("Seems the QSV test subprocess crashed. "
         "Better there than here I guess. "
         "Let's just skip loading QSV then I suppose.");
    goto fail;
  }

  if (config_open_string(&config, caps_str.array) != 0) {
    info("Couldn't open QSV configuration string");
    goto fail;
  }

  error = config_get_string(config, "error", "string");
  if (error) {
    info("Error querying QSV support: %s", error);
    goto fail;
  }

  config_adapter_count = config_num_sections(config);

  if (config_adapter_count < *adapter_count)
    *adapter_count = config_adapter_count;

  for (size_t i = 0; i < *adapter_count; i++) {
    char section[16];
    snprintf(section, sizeof(section), "%d", (int)i);

    struct adapter_info *adapter = &adapters[i];
    adapter->is_intel = config_get_bool(config, section, "is_intel");
    adapter->is_dgpu = config_get_bool(config, section, "is_dgpu");
    adapter->supports_av1 = config_get_bool(config, section, "supports_av1");
    adapter->supports_hevc = config_get_bool(config, section, "supports_hevc");
    adapter->supports_vp9 = config_get_bool(config, section, "supports_hevc");
  }

fail:
  config_close(config);
  dstr_free(&caps_str);
  dstr_free(&cmd);
  os_process_pipe_destroy(pp);
}
