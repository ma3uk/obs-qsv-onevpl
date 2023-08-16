#include "common_utils.hpp"

#include <util/windows/device-enum.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/pipe.h>
#include <util/dstr.h>

#include <intrin.h>
#include <wrl/client.h>

/* =======================================================
 * Windows implementation of OS-specific utility functions
 */

void util_cpuid(int cpuinfo[4], int flags)
{
	return __cpuid(cpuinfo, flags);
}

static bool enum_luids(void *param, uint32_t idx, uint64_t luid)
{
	dstr *cmd = static_cast<dstr *>(param);
	dstr_catf(cmd, " %llX", luid);
	UNUSED_PARAMETER(idx);
	return true;
}

void check_adapters(struct adapter_info *adapters, size_t *adapter_count)
{
	char *test_exe = os_get_executable_path_ptr("obs-qsv-test.exe");
	dstr cmd = {0};
	dstr caps_str = {0};
	os_process_pipe_t *pp_vpl = NULL;
	config_t *config = NULL;

	//bool avc_supported = false;
	//bool av1_supported = false;
	//bool hevc_supported = false;
	//bool vp9_supported = false;

	const char *error = NULL;

	dstr_copy(&cmd, test_exe);
	enum_graphics_device_luids(enum_luids, &cmd);

	pp_vpl = os_process_pipe_create(cmd.array, "r");
	if (!pp_vpl) {
		blog(LOG_INFO, "Failed to launch the QSV test process I guess");
		goto fail;
	}

	for (;;) {
		char data[2048] = "nullptr";
		size_t len = os_process_pipe_read(pp_vpl, (uint8_t *)data,
										  sizeof(data));
		if (!len)
			break;

		dstr_ncat(&caps_str, data, len);
	}

	if (dstr_is_empty(&caps_str)) {
		blog(LOG_INFO, "Seems the QSV test subprocess crashed. "
					   "Better there than here I guess. "
					   "Let's just skip loading QSV then I suppose.");
		goto fail;
	}

	if (config_open_string(&config, caps_str.array) != 0) {
		blog(LOG_INFO, "Couldn't open QSV configuration string");
		goto fail;
	}

	error = config_get_string(config, "error", "string");
	if (error) {
		blog(LOG_INFO, "Error querying QSV support: %s", error);
		goto fail;
	}

	*adapter_count = config_num_sections(config);

	if (*adapter_count > MAX_ADAPTERS)
		*adapter_count = MAX_ADAPTERS;

	for (size_t i = 0; i < *adapter_count; ++i) {
		char section[16];
		snprintf(section, sizeof(section), "%u", (int)i);

		adapter_info *adapter = &adapters[i];

		adapter->is_intel =
				config_get_bool(config, section, "is_intel");
		adapter->is_dgpu = config_get_bool(config, section, "is_dgpu");

		adapter->supports_av1 =
				config_get_bool(config, section, "supports_av1");

		adapter->supports_hevc =
				config_get_bool(config, section, "supports_hevc");

		adapter->supports_vp9 =
				config_get_bool(config, section, "supports_hevc");
	}

	fail:
	config_close(config);
	dstr_free(&caps_str);
	dstr_free(&cmd);
	os_process_pipe_destroy(pp_vpl);
	bfree(test_exe);
}
