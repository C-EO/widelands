/*
 * Copyright (C) 2002-2025 by the Widelands Development Team
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

#include "logic/map_objects/tribes/carrier.h"

#include "base/macros.h"
#include "base/wexception.h"
#include "economy/flag.h"
#include "economy/road.h"
#include "economy/ware_instance.h"
#include "io/fileread.h"
#include "io/filewrite.h"
#include "logic/game.h"
#include "logic/game_data_error.h"
#include "logic/player.h"

namespace Widelands {

/**
 * Deliver all wares addressed to the building the carrier is already into
 *
 * \param state UNDOCUMENTED
 */
// TODO(unknown):  Upgrade this function to really support many-wares-at-a-time
// TODO(unknown):  Document parameter state
void Carrier::deliver_to_building(Game& game, State& state) {
	BaseImmovable* const pos = game.map()[get_position()].get_immovable();

	if ((pos != nullptr) && pos->descr().type() == Widelands::MapObjectType::FLAG) {
		return pop_task(game);  //  we are done
	}
	if (upcast(Building, building, pos)) {
		// Drop all wares addressed to this building
		while (WareInstance* const ware = get_carried_ware(game)) {
			// If the building has disappeared and immediately been replaced
			// with another building, we might have to return without dropping
			// the ware.
			PlayerImmovable const* const next = ware->get_next_move_step(game);

			if (next == pos) {
				fetch_carried_ware(game);
				ware->enter_building(game, *building);
			} else {
				molog(
				   game.get_gametime(), "[Carrier]: Building switch from under us, return to road.\n");

				state.ivar1 = static_cast<int32_t>(&building->base_flag() ==
				                                   &dynamic_cast<RoadBase&>(*get_location(game))
				                                       .get_flag(static_cast<RoadBase::FlagId>(0)));
				break;
			}
		}

		// No more deliverable wares. Walk out to the flag.
		return start_task_move(
		   game, WALK_SE, descr().get_right_walk_anims(does_carry_ware(), this), true);
	}  //  tough luck, the building has disappeared
	molog(game.get_gametime(), "[Carrier]: Building disappeared while in building.\n");
	set_location(nullptr);
}

/**
 * Walks to the queued flag and picks up one acked ware
 *
 * \param g Game the carrier lives on
 * \param s Flags sent to the task
 */
void Carrier::pickup_from_flag(Game& game, const State& state) {
	int32_t const ivar1 = state.ivar1;
	if (!start_task_walktoflag(game, ivar1)) {

		promised_pickup_to_ = NOONE;

		RoadBase& road = dynamic_cast<RoadBase&>(*get_location(game));
		Flag& flag = road.get_flag(static_cast<RoadBase::FlagId>(ivar1));
		Flag& otherflag = road.get_flag(static_cast<RoadBase::FlagId>(ivar1 ^ 1));

		// Are there wares to move between our flags?
		if (WareInstance* const ware = flag.fetch_pending_ware(game, otherflag)) {
			// pay before getting the ware, while checking for road promotion
			if (upcast(Road, r, &road)) {
				r->pay_for_road(game, flag.count_wares_in_queue(otherflag));
			}
			set_carried_ware(game, ware);

			set_animation(game, descr().get_animation("idle", this));
			return schedule_act(game, Duration(20));
		}
		molog(game.get_gametime(), "[Carrier]: Nothing suitable on flag.\n");
		return pop_task(game);
	}
}

/**
 * Drop one ware in a flag, and pick up a new one if we acked it
 *
 * \param g Game the carrier lives on.
 * \param s Flags sent to the task
 */
void Carrier::drop_ware(Game& game, State& state) {
	WareInstance* other = nullptr;
	RoadBase& road = dynamic_cast<RoadBase&>(*get_location(game));
	Flag& flag = road.get_flag(static_cast<RoadBase::FlagId>(state.ivar1 ^ 1));
	Flag& otherflag = road.get_flag(static_cast<RoadBase::FlagId>(state.ivar1));

	if (promised_pickup_to_ == (state.ivar1 ^ 1)) {
		// If there's a ware we acked, we can drop ours even if the flag is
		// flooded
		other = flag.fetch_pending_ware(game, otherflag);

		if ((other == nullptr) && !flag.has_capacity()) {
			molog(game.get_gametime(), "[Carrier]: strange: acked ware from busy flag no longer "
			                           "present.\n");

			promised_pickup_to_ = NOONE;
			set_animation(game, descr().get_animation("idle", this));
			return schedule_act(game, Duration(20));
		}

		state.ivar1 = promised_pickup_to_;
		promised_pickup_to_ = NOONE;
	}

	// Drop our ware
	flag.add_ware(game, *fetch_carried_ware(game));

	// Pick up new load, if any
	if (other != nullptr) {
		// pay before getting the ware, while checking for road promotion
		if (upcast(Road, r, &road)) {
			r->pay_for_road(game, flag.count_wares_in_queue(otherflag));
		}
		set_carried_ware(game, other);

		set_animation(game, descr().get_animation("idle", this));
		return schedule_act(game, Duration(20));
	}
	return pop_task(game);
}

/**
 * When picking up wares, if some of them is targeted to the building attached
 * to target flag walk straight into it and deliver.
 *
 * \param g Game the carrier lives on.
 * \param s Flags sent to the task.
 */
void Carrier::enter_building(Game& game, State& state) {
	if (!start_task_walktoflag(game, state.ivar1 ^ 1)) {
		state.ivar1 = -1;
		return start_task_move(
		   game, WALK_NW, descr().get_right_walk_anims(does_carry_ware(), this), true);
	}
}

/**
 * Swaps wares from an overloaded flag for as long as the carrier can pick
 * up new wares from it. Otherwise, changes the carrier state to wait.
 *
 * \param g Game the carrier lives on.
 * \param s Flags sent to the task.
 *
 * \return true if the carrier must wait before delivering his wares.
 */
bool Carrier::swap_or_wait(Game& game, State& state) {
	// Road that employs us
	const RoadBase& road = dynamic_cast<RoadBase&>(*get_location(game));
	// Flag we are delivering to
	Flag& flag = road.get_flag(static_cast<RoadBase::FlagId>(state.ivar1 ^ 1));
	// The other flag of our road
	Flag& otherflag = road.get_flag(static_cast<RoadBase::FlagId>(state.ivar1));

	if (promised_pickup_to_ == (state.ivar1 ^ 1)) {
		// All is well, we already acked a ware that we can pick up
		// from this flag
		return false;
	}
	if (flag.has_pending_ware(game, otherflag)) {
		if (!flag.ack_pickup(game, otherflag)) {
			throw wexception(
			   "MO(%u): transport: overload exchange: flag %u is fucked up", serial(), flag.serial());
		}

		promised_pickup_to_ = state.ivar1 ^ 1;
		return false;
	}

	if (!start_task_walktoflag(game, state.ivar1 ^ 1, true)) {
		start_task_waitforcapacity(game, flag);  //  wait one node away
	}
	return true;
}

/**
 * Called by Road code to indicate that a new ware has arrived on a flag
 * (0 = start, 1 = end).
 * \return true if the carrier is going to fetch it.
 */
bool Carrier::notify_ware(Game& game, int32_t const flag) {
	const State& state = top_state();

	// Check if we've already acked something
	if (promised_pickup_to_ != NOONE) {
		return false;
	}

	// If we are currently in a transport.
	// Explanation:
	//  a) a different carrier / road may be better suited for this ware
	//     (the transport code does not have priorities for the actual
	//     carrier that is notified)
	//  b) the transport task has logic that allows it to
	//     drop a ware on an overloaded flag iff it can pick up a ware
	//     at the same time.
	//     We should ack said ware to avoid more confusion before we move
	//     onto the flag, but we can't do that if we have already acked
	//     something.
	//  c) we might ack for a flag that we are actually moving away from;
	//     this will get us into trouble if wares have arrived on the other
	//     flag while we couldn't ack them.
	//
	// (Maybe the need for this lengthy explanation is proof that the
	// ack system needs to be reworked.)
	if (State const* const transport = get_state(taskTransport)) {
		if ((transport->ivar1 == -1 && find_closest_flag(game) != flag) || flag == transport->ivar1) {
			return false;
		}
	}

	// Ack it if we haven't
	promised_pickup_to_ = flag;

	if (state.task == &taskRoad) {
		send_signal(game, "ware");
	} else if (state.task == &taskWaitforcapacity) {
		send_signal(game, "wakeup");
	}
	return true;
}

/**
 * Find a pending ware on one of the road's flags, ack it and set promised_pickup_to_
 * accordingly.
 */
void Carrier::find_pending_ware(Game& game) {
	const RoadBase& road = dynamic_cast<RoadBase&>(*get_location(game));
	uint32_t havewarebits = 0;

	assert(promised_pickup_to_ == NOONE);

	if (road.get_flag(RoadBase::FlagStart)
	       .has_pending_ware(game, road.get_flag(RoadBase::FlagEnd))) {
		havewarebits |= 1;
	}

	if (road.get_flag(RoadBase::FlagEnd)
	       .has_pending_ware(game, road.get_flag(RoadBase::FlagStart))) {
		havewarebits |= 2;
	}

	//  If both flags have a ware, we pick the one closer to us.
	if (havewarebits == 3) {
		havewarebits = 1 << find_closest_flag(game);
	}

	// Ack our decision
	if (havewarebits == 1) {
		promised_pickup_to_ = START_FLAG;
		if (!road.get_flag(RoadBase::FlagStart).ack_pickup(game, road.get_flag(RoadBase::FlagEnd))) {
			throw wexception("Carrier::find_pending_ware: start flag is messed up");
		}

	} else if (havewarebits == 2) {
		promised_pickup_to_ = END_FLAG;
		if (!road.get_flag(RoadBase::FlagEnd).ack_pickup(game, road.get_flag(RoadBase::FlagStart))) {
			throw wexception("Carrier::find_pending_ware: end flag is messed up");
		}
	}
}

/**
 * Find the flag we are closest to (in walking time).
 */
int32_t Carrier::find_closest_flag(const Game& game) {
	const Map& map = game.map();
	CoordPath startpath(map, dynamic_cast<RoadBase&>(*get_location(game)).get_path());

	CoordPath endpath;
	int32_t startcost;
	int32_t endcost;
	int32_t curidx = startpath.get_index(get_position());

	// Apparently, we're in a building
	if (curidx < 0) {
		Coords pos = get_position();

		map.get_brn(pos, &pos);

		if (pos == startpath.get_start()) {
			curidx = 0;
		} else if (pos == startpath.get_end()) {
			curidx = startpath.get_nsteps();
		} else {
			throw wexception("MO(%u): Carrier::find_closest_flag: not on road, not on "
			                 "building",
			                 serial());
		}
	}

	// Calculate the paths and their associated costs
	endpath = startpath;

	startpath.truncate(curidx);
	startpath.reverse();

	endpath.trim_start(curidx);

	map.calc_cost(startpath, &startcost, nullptr);
	map.calc_cost(endpath, &endcost, nullptr);

	return static_cast<int32_t>(endcost < startcost);
}

/**
 * Walk to the given flag, or one field before it if offset is true.
 *
 * \return true if a move task has been started, or false if we're already on
 * the target field.
 */
bool Carrier::start_task_walktoflag(Game& game, int32_t const flag, bool const offset) {
	const Path& path = dynamic_cast<RoadBase&>(*get_location(game)).get_path();
	int32_t idx;

	if (flag == 0) {
		idx = 0;
		if (offset) {
			++idx;
		}
	} else {
		idx = path.get_nsteps();
		if (offset) {
			--idx;
		}
	}

	return start_task_movepath(
	   game, path, idx, descr().get_right_walk_anims(does_carry_ware(), this));
}

void Carrier::log_general_info(const Widelands::EditorGameBase& egbase) const {
	molog(egbase.get_gametime(), "Carrier at %i,%i\n", get_position().x, get_position().y);

	Worker::log_general_info(egbase);

	molog(egbase.get_gametime(), "promised_pickup_to = %i\n", promised_pickup_to_);
}

/*
==============================

Load/save support

==============================
*/
constexpr uint8_t kCurrentPacketVersion = 3;

void Carrier::Loader::load(FileRead& fr) {
	Worker::Loader::load(fr);

	try {
		const uint8_t packet_version = fr.unsigned_8();
		if (packet_version == kCurrentPacketVersion) {
			Carrier& carrier = get<Carrier>();
			carrier.promised_pickup_to_ = fr.signed_32();
		} else {
			throw UnhandledVersionError("Carrier", packet_version, kCurrentPacketVersion);
		}
	} catch (const WException& e) {
		throw wexception("loading carrier: %s", e.what());
	}
}

const Bob::Task* Carrier::Loader::get_task(const std::string& name) {
	if (name == "road") {
		return &taskRoad;
	}
	if (name == "transport") {
		return &taskTransport;
	}
	return Worker::Loader::get_task(name);
}

Carrier::Loader* Carrier::create_loader() {
	return new Loader;
}

void Carrier::do_save(EditorGameBase& egbase, MapObjectSaver& mos, FileWrite& fw) {
	Worker::do_save(egbase, mos, fw);

	fw.unsigned_8(kCurrentPacketVersion);
	fw.signed_32(promised_pickup_to_);
}

CarrierDescr::CarrierDescr(const std::string& init_descname,
                           const LuaTable& table,
                           const std::vector<std::string>& attribs,
                           Descriptions& descriptions,
                           MapObjectType t)
   : WorkerDescr(init_descname, t, table, attribs, descriptions) {
}

/**
 * Create a new carrier
 */
Bob& CarrierDescr::create_object() const {
	return *new Carrier(*this);
}
}  // namespace Widelands
