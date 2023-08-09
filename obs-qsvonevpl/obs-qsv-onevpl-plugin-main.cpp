/*

This file is provided under a dual BSD/GPLv2 license.  When using or
redistributing this file, you may do so under either license.

GPL LICENSE SUMMARY

Copyright(c) Oct. 2015 Intel Corporation.

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Contact Information:

Seung-Woo Kim, seung-woo.kim@intel.com
705 5th Ave S #500, Seattle, WA 98104

BSD LICENSE

Copyright(c) <date> Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <obs.h>
#include <obs-module.h>
#include <util/windows/device-enum.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/pipe.h>
#include <util/dstr.h>
#include "obs-qsv-onevpl-encoder.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-qsvonevpl", "en-US");
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Intel Quick Sync Video support for Windows (oneVPL)";
}

extern obs_encoder_info obs_qsv_h264_encoder;

extern obs_encoder_info obs_qsv_av1_encoder;

extern obs_encoder_info obs_qsv_hevc_encoder;

extern obs_encoder_info obs_qsv_vp9_encoder;

extern bool av1_supported(mfxIMPL impl);

adapter_info adapters[MAX_ADAPTERS] = {0};
size_t adapter_count = 0;

static bool enum_luids(void *param, uint32_t idx, uint64_t luid)
{
	dstr *cmd = static_cast<dstr *>(param);
	dstr_catf(cmd, " %llX", luid);
	UNUSED_PARAMETER(idx);
	return true;
}

inline static bool close_config(config_t *config, dstr caps_str, dstr cmd,
				os_process_pipe *pp_vpl, char *test_exe)
{
	config_close(config);
	dstr_free(&caps_str);
	dstr_free(&cmd);
	os_process_pipe_destroy(pp_vpl);
	bfree(test_exe);

	return true;
}

bool obs_module_load(void)
{
	char *test_exe = os_get_executable_path_ptr("obs-qsv-test.exe");
	dstr cmd = {0};
	dstr caps_str = {0};
	os_process_pipe_t *pp_vpl = NULL;
	config_t *config = NULL;

	dstr_copy(&cmd, test_exe);
	enum_graphics_device_luids(enum_luids, &cmd);

	pp_vpl = os_process_pipe_create(cmd.array, "r");
	if (!pp_vpl) {
		blog(LOG_INFO, "Failed to launch the QSV test process I guess");
		return close_config(config, caps_str, cmd, pp_vpl, test_exe);
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
		return close_config(config, caps_str, cmd, pp_vpl, test_exe);
	}

	if (config_open_string(&config, caps_str.array) != 0) {
		blog(LOG_INFO, "Couldn't open QSV configuration string");
		return close_config(config, caps_str, cmd, pp_vpl, test_exe);
	}

	const char *error = config_get_string(config, "error", "string");
	if (error) {
		blog(LOG_INFO, "Error querying QSV support: %s", error);
		return close_config(config, caps_str, cmd, pp_vpl, test_exe);
	}

	adapter_count = config_num_sections(config);
	bool avc_supported = false;
	bool av1_supported = false;
	bool hevc_supported = false;
	bool vp9_supported = false;

	if (adapter_count > MAX_ADAPTERS)
		adapter_count = MAX_ADAPTERS;

	for (size_t i = 0; i < adapter_count; ++i) {
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
			config_get_bool(config, section, "supports_vp9");

		avc_supported |= adapter->is_intel;
		av1_supported |= adapter->supports_av1;
		hevc_supported |= adapter->supports_hevc;
		vp9_supported |= adapter->supports_vp9;
	}

	if (avc_supported) {
		obs_register_encoder(&obs_qsv_h264_encoder);
		blog(LOG_INFO, "QSV AVC support");
	}
	if (av1_supported) {
		obs_register_encoder(&obs_qsv_av1_encoder);
		blog(LOG_INFO, "QSV AV1 support");
	}
	if (vp9_supported) {
		obs_register_encoder(&obs_qsv_vp9_encoder);
		blog(LOG_INFO, "QSV VP9 support");
	}
#if ENABLE_HEVC
	if (hevc_supported) {
		obs_register_encoder(&obs_qsv_hevc_encoder);
		blog(LOG_INFO, "QSV HEVC support");
	}
#endif
	return true;
}