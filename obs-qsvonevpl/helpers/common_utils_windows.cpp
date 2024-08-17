#include "common_utils.hpp"
#include <util/dstr.h>


void Release() {
#if defined(_WIN32) || defined(_WIN64)
  //CleanupHWDevice();
#endif
}

void ReleaseSessionData(void *) {}

static bool enum_luids(void *param, uint32_t idx, uint64_t luid) {
  dstr *cmd = static_cast<dstr *>(param);
  dstr_catf(cmd, " %llX", luid);
  UNUSED_PARAMETER(idx);
  return true;
}

void GetAdaptersInfo(struct adapter_info *AdaptersInfo, size_t *AdaptersCount) {
  char *TestExecutable = os_get_executable_path_ptr("obs-qsv-test.exe");
  struct dstr CMD = {0};
  struct dstr Caps = {0};
  os_process_pipe_t *ProcessPipe = nullptr;
  config_t *Config = nullptr;
  const char *Error = nullptr;
  size_t ConfigAdaptersCount;

  dstr_init_move_array(&CMD, TestExecutable);
  dstr_insert_ch(&CMD, 0, '\"');
  dstr_cat(&CMD, "\"");

  enum_graphics_device_luids(enum_luids, &CMD);

  ProcessPipe = os_process_pipe_create(CMD.array, "r");
  if (!ProcessPipe) {
    info("Failed to launch the QSV test process I guess");
    goto fail;
  }

  for (;;) {
    char Data[2048]{};
    size_t CMDLength = os_process_pipe_read(
        ProcessPipe, reinterpret_cast<uint8_t *>(Data), sizeof(Data));
    if (!CMDLength)
      break;

    dstr_ncat(&Caps, Data, CMDLength);
  }

  if (dstr_is_empty(&Caps)) {
    info("Seems the QSV test subprocess crashed. "
         "Better there than here I guess. "
         "Let's just skip loading QSV then I suppose.");
    goto fail;
  }

  if (config_open_string(&Config, Caps.array) != 0) {
    info("Couldn't open QSV configuration string");
    goto fail;
  }

  Error = config_get_string(Config, "error", "string");
  if (Error) {
    info("Error querying QSV support: %s", Error);
    goto fail;
  }

  ConfigAdaptersCount = config_num_sections(Config);

  if (ConfigAdaptersCount < *AdaptersCount)
    *AdaptersCount = ConfigAdaptersCount;

  for (size_t i = 0; i < *AdaptersCount; i++) {
    char CMDSection[16];
    snprintf(CMDSection, sizeof(CMDSection), "%d", (int)i);

    struct adapter_info *AdapterInfo = &AdaptersInfo[i];
    AdapterInfo->IsIntel = config_get_bool(Config, CMDSection, "is_intel");
    AdapterInfo->IsDGPU = config_get_bool(Config, CMDSection, "is_dgpu");
    AdapterInfo->SupportAV1 =
        config_get_bool(Config, CMDSection, "supports_av1");
    AdapterInfo->SupportHEVC =
        config_get_bool(Config, CMDSection, "supports_hevc");
  }

fail:
  config_close(Config);
  dstr_free(&Caps);
  dstr_free(&CMD);
  os_process_pipe_destroy(ProcessPipe);
}
