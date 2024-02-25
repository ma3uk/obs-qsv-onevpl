#include "common_utils.hpp"

#include "hw_d3d11.hpp"

#include <util/config-file.h>
#include <util/dstr.h>
#include <util/pipe.h>
#include <util/platform.h>
#include <util/windows/device-enum.h>

#include <intrin.h>
#include <inttypes.h>

void Release() {
#if defined(_WIN32) || defined(_WIN64)
  //CleanupHWDevice();
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
