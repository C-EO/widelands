/*
 * Copyright (C) 2002-2023 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "map_io/map_heights_packet.h"

#include "io/fileread.h"
#include "io/filewrite.h"
#include "logic/editor_game_base.h"
#include "logic/game_data_error.h"
#include "logic/map.h"

namespace Widelands {

constexpr uint16_t kCurrentPacketVersion = 2;

void MapHeightsPacket::read(FileSystem& fs,
                            EditorGameBase& egbase,
                            bool /* skip */,
                            MapObjectLoader& /* mol */) {

	FileRead fr;
	fr.open(fs, "binary/heights");

	try {
		uint16_t const packet_version = fr.unsigned_16();
		if (packet_version >= 1 && packet_version <= kCurrentPacketVersion) {
			Map& map = *egbase.mutable_map();
			map.max_field_height_diff_ =
			   (packet_version < 2) ? kDefaultMaxFieldHeightDiff : fr.unsigned_8();
			MapIndex const max_index = map.max_index();
			for (MapIndex i = 0; i < max_index; ++i) {
				map[i].set_height(fr.unsigned_8());
			}
		} else {
			throw UnhandledVersionError("MapHeightsPacket", packet_version, kCurrentPacketVersion);
		}
	} catch (const WException& e) {
		throw GameDataError("heights: %s", e.what());
	}
}

/*
 * Write Function
 */
void MapHeightsPacket::write(FileSystem& fs, EditorGameBase& egbase, MapObjectSaver& /* mos */) {
	FileWrite fw;

	fw.unsigned_16(kCurrentPacketVersion);

	const Map& map = egbase.map();
	fw.unsigned_8(map.max_field_height_diff());
	MapIndex const max_index = map.max_index();
	for (MapIndex i = 0; i < max_index; ++i) {
		fw.unsigned_8(map[i].get_height());
	}

	fw.write(fs, "binary/heights");
}
}  // namespace Widelands
