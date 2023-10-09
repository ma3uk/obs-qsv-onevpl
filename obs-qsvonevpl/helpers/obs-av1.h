// SPDX-FileCopyrightText: 2023 David Rosca <nowrep@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

enum obu {
	OBS_OBU_SEQUENCE_HEADER = 1,
	OBS_OBU_TEMPORAL_DELIMITER = 2,
	OBS_OBU_FRAME_HEADER = 3,
	OBS_OBU_TILE_GROUP = 4,
	OBS_OBU_METADATA = 5,
	OBS_OBU_FRAME = 6,
	OBS_OBU_REDUNDANT_FRAME_HEADER = 7,
	OBS_OBU_TILE_LIST = 8,
	OBS_OBU_PADDING = 15,
};

/* Helpers for parsing AV1 OB units.  */

bool av1_keyframe(const uint8_t *data, size_t size);
void extract_av1_headers(const uint8_t *packet, size_t size,
				    uint8_t **new_packet_data,
				    size_t *new_packet_size,
				    uint8_t **header_data, size_t *header_size);