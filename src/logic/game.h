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

#ifndef WL_LOGIC_GAME_H
#define WL_LOGIC_GAME_H

#include <memory>

#include "base/crypto.h"
#include "base/random.h"
#include "commands/cmd_queue.h"
#include "economy/flag_job.h"
#include "io/streamwrite.h"
#include "logic/detected_port_space.h"
#include "logic/editor_game_base.h"
#include "logic/map_objects/tribes/shipstates.h"
#include "logic/save_handler.h"
#include "logic/trade_agreement.h"
#include "scripting/logic.h"

class InteractivePlayer;
struct GameSettings;
class GameController;

namespace Widelands {

/// How often are statistics to be sampled.
constexpr Duration kStatisticsSampleTime(30 * 1000);
// See forester_cache_
constexpr int16_t kInvalidForesterEntry = -1;

constexpr uint32_t kScenarioDifficultyNotSet = 0;

struct Flag;
struct Path;
struct PlayerImmovable;
enum class IslandExploreDirection;
class PortDock;
enum class ScoutingDirection;
struct Ship;
class TrainingSite;
#if 0  // TODO(Nordfriese): Re-add training wheels code after v1.0
class TrainingWheels;
#endif
enum class StockPolicy;

enum {
	gs_notrunning = 0,  // game is being prepared
	gs_running,         // game was fully prepared at some point and is now in-game
	gs_ending
};

// The entry types that are written to the syncstream
// The IDs are a number in the higher 4 bits and the length in bytes in the lower 4 bits
// Keep this synchronized with utils/syncstream/syncexcerpt-to-text.py
enum SyncEntry : uint8_t {
	// Used in:
	// game.cc Game::report_desync()
	// Parameters:
	// s32 id of desynced user, -1 when written on client
	kDesync = 0x14,
	// map_object.cc CmdDestroyMapObject::execute()
	// u32 object serial
	kDestroyObject = 0x24,
	// economy.cc Economy::process_requests()
	// u8 request type
	// u8 request index
	// u32 target serial
	kProcessRequests = 0x36,
	// economy.cc Economy::handle_active_supplies()
	// u32 assignments size
	kHandleActiveSupplies = 0x44,
	// request.cc Request::start_transfer()
	// u32 target serial
	// u32 source(?) serial
	kStartTransfer = 0x58,
	// cmd_queue.cc CmdQueue::run_queue()
	// u32 duetime
	// u32 command id
	kRunQueue = 0x68,
	// game.h Game::logic_rand_seed()
	// u32 random seed
	kRandomSeed = 0x74,
	// game.cc Game::logic_rand()
	// u32 random value
	kRandom = 0x84,
	// map_object.cc CmdAct::execute()
	// u32 object serial
	// u8 object type (see map_object.h MapObjectType)
	kCmdAct = 0x95,
	// battle.cc Battle::Battle()
	// u32 first soldier serial
	// u32 second soldier serial
	kBattle = 0xA8,
	// bob.cc Bob::set_position()
	// u32 bob serial
	// s16 position x
	// s16 position y
	kBobSetPosition = 0xB8
};

class PlayerCommand;
class ReplayWriter;

/** class Game
 *
 * This class manages the entire lifetime of a game session, from creating the
 * game and setting options, selecting maps to the actual playing phase and the
 * final statistics screen(s).
 */

class Game : public EditorGameBase {
public:
	struct GeneralStats {
		std::vector<uint32_t> land_size;
		std::vector<uint32_t> nr_workers;
		std::vector<uint32_t> nr_buildings;
		std::vector<uint32_t> nr_wares;
		std::vector<uint32_t> productivity;
		std::vector<uint32_t> nr_casualties;
		std::vector<uint32_t> nr_kills;
		std::vector<uint32_t> nr_msites_lost;
		std::vector<uint32_t> nr_msites_defeated;
		std::vector<uint32_t> nr_civil_blds_lost;
		std::vector<uint32_t> nr_civil_blds_defeated;
		std::vector<uint32_t> miltary_strength;
		std::vector<uint32_t> nr_ships;
		std::vector<uint32_t> nr_naval_victories;
		std::vector<uint32_t> nr_naval_losses;

		std::vector<uint32_t> custom_statistic;
	};
	using GeneralStatsVector = std::vector<GeneralStats>;

	friend class CmdQueue;  // this class handles the commands
	friend struct GameClassPacket;
	friend struct GamePlayerTradesPacket;
	friend struct GamePlayerInfoPacket;
	friend struct GameLoader;

	// TODO(kxq): The lifetime of game-instance is okay for this, but is this the right spot?
	// TODO(kxq): I should find the place where LUA changes map, and clear this whenever that
	// happens.
	// TODO(kxq): When done, the x_check in worker.cc could be removed (or made more rare, and put
	// under an assert as then mismatch would indicate the presence of a bug).
	// TODO(k.halfmann): this shoud perhpas better be a map, it will be quite sparse?

	/** Qualitity of terrain for tree planting normalized to int16.
	 *
	 *  Indexed by MapIndex. -1  is an ivalid entry. Shared between all tribes (on the same server)
	 *  will be cleared when diffrences are detected. Presently, that can only happen if terrain
	 *  is changed (lua scripting). The map is sparse, lookups are fast.
	 */
	std::vector<int16_t> forester_cache_;

	Game();
	~Game() override;

	bool is_game() const override {
		return true;
	}

	// life cycle
	void set_game_controller(std::shared_ptr<GameController>);
	GameController* game_controller();
	void set_write_replay(bool wr);
	void set_write_syncstream(bool wr);
	void save_syncstream(bool save);
	void init_newgame(const GameSettings&);
	void init_savegame(const GameSettings&);

	enum class StartGameType { kMap, kSinglePlayerScenario, kMultiPlayerScenario, kSaveGame };

	bool run(StartGameType, const std::string& script_to_run, const std::string& prefix_for_replays);

	// Returns the upcasted lua interface.
	LuaGameInterface& lua() override;

	// Run a single player scenario directly via --scenario on the cmdline. Will
	// run the 'script_to_run' after any init scripts of the map.
	// `list_of_scenarios` is list of this scenario and – if applicable –
	// all the subsequent scenarios in the campaign.
	// Returns the result of run().
	bool run_splayer_scenario_direct(const std::list<std::string>& list_of_scenarios,
	                                 const std::string& script_to_run);

	// Run a single player loaded game directly via --loadgame on the cmdline. Will
	// run the 'script_to_run' directly after the game was loaded.
	// Returns the result of run().
	bool run_load_game(const std::string& filename, const std::string& script_to_run);

	bool run_replay(const std::string& filename, const std::string& script_to_run);

#if 0  // TODO(Nordfriese): Re-add training wheels code after v1.0
	bool acquire_training_wheel_lock(const std::string& objective);
	void release_training_wheel_lock();
	void mark_training_wheel_as_solved(const std::string& objective);
	void skip_training_wheel(const std::string& objective);
	void run_training_wheel(const std::string& objective, bool force);

	bool training_wheels_wanted() const;
	std::string active_training_wheel() const;
#endif

	void postload() override;
	void postload_addons_before_loading();

	void think() override;

	ReplayWriter* get_replaywriter() {
		return replaywriter_.get();
	}

	/**
	 * \return \c true if the game is completely loaded and running (or paused)
	 * or \c false otherwise.
	 */
	bool is_loaded() const {
		return state_ == gs_running;
	}

	void cleanup_for_load() override;
	void full_cleanup() override;

	// in-game logic
	const CmdQueue& cmdqueue() const {
		return cmdqueue_;
	}
	CmdQueue& cmdqueue() {
		return cmdqueue_;
	}
	const RNG& rng() const {
		return rng_;
	}
	RNG& rng() {
		return rng_;
	}

	uint32_t logic_rand();

	/// Generate a random location within radius from location.
	Coords random_location(Coords location, uint8_t radius);

	void logic_rand_seed(uint32_t const seed) {
		rng().seed(seed);
		syncstream().unsigned_8(SyncEntry::kRandomSeed);
		syncstream().unsigned_32(seed);
	}

	StreamWrite& syncstream();
	void report_sync_request();
	void report_desync(int32_t playernumber);
	crypto::MD5Checksum get_sync_hash() const;

	void enqueue_command(Command*);

	void send_player_command(Widelands::PlayerCommand*);

	void send_player_bulldoze(PlayerImmovable&, bool recurse = false);
	void send_player_dismantle(PlayerImmovable&, bool keep_wares);
	void send_player_build_building(int32_t, const Coords&, DescriptionIndex);
	void send_player_build_flag(int32_t, const Coords&);
	void send_player_build_road(int32_t, Path&);
	void send_player_build_waterway(int32_t, Path&);
	void send_player_flagaction(Flag&, FlagJob::Type);
	void send_player_start_stop_building(Building&);
	void send_player_toggle_infinite_production(Building&);
	void send_player_set_soldier_preference(MapObject&, SoldierPreference preference);
	void send_player_start_or_cancel_expedition(Building&);
	void send_player_expedition_config(PortDock&, WareWorker, DescriptionIndex, bool);

	void send_player_enhance_building(Building&, DescriptionIndex, bool keep_wares);
	void send_player_evict_worker(Worker&);
	void send_player_set_stock_policy(Building&, WareWorker, DescriptionIndex, StockPolicy);
	void send_player_set_ware_priority(PlayerImmovable&,
	                                   WareWorker,
	                                   DescriptionIndex,
	                                   const WarePriority&,
	                                   bool is_cs = false,
	                                   uint32_t disambiguator_id = 0);
	void send_player_set_input_max_fill(PlayerImmovable&,
	                                    DescriptionIndex index,
	                                    WareWorker type,
	                                    uint32_t,
	                                    bool is_cs = false,
	                                    uint32_t disambiguator_id = 0);
	void send_player_change_training_options(TrainingSite&, TrainingAttribute, int32_t);
	void send_player_drop_soldier(MapObject&, int32_t);
	void send_player_change_soldier_capacity(Building&, int32_t);
	void
	send_player_attack(const Flag&, PlayerNumber, const std::vector<Serial>&, bool allow_conquer);
	void send_player_mark_object_for_removal(PlayerNumber, Immovable&, bool);

	void send_player_ship_scouting_direction(const Ship& ship, WalkingDir direction);
	void send_player_ship_construct_port(const Ship& ship, Coords coords);
	void send_player_ship_explore_island(const Ship& ship, IslandExploreDirection direction);
	void send_player_ship_set_destination(const Ship& ship, const MapObject* dest);
	void send_player_ship_set_destination(const Ship& ship, const DetectedPortSpace& dest);
	void send_player_sink_ship(const Ship& ship);
	void send_player_refit_ship(const Ship& ship, ShipType t);
	void send_player_warship_command(const Ship& ship,
	                                 WarshipCommand cmd,
	                                 const std::vector<uint32_t>& parameters);
	void send_player_cancel_expedition_ship(const Ship&);
	void send_player_propose_trade(const TradeInstance& trade);
	void send_player_extend_trade(PlayerNumber sender,
	                              TradeID trade_id,
	                              TradeAction action,
	                              int32_t batches);
	void send_player_trade_action(
	   PlayerNumber sender, TradeID trade_id, TradeAction action, Serial accepter, Serial source);
	void send_player_toggle_mute(const Building&, bool all);
	void send_player_diplomacy(PlayerNumber, DiplomacyAction, PlayerNumber);
	void send_player_pinned_note(
	   PlayerNumber p, Coords pos, const std::string& text, const RGBColor& rgb, bool del);
	void send_player_building_name(PlayerNumber p, Serial s, const std::string& name);
	void send_player_fleet_targets(PlayerNumber p, Serial i, Quantity q);

	InteractivePlayer* get_ipl();
	const InteractivePlayer* get_ipl() const;

	SaveHandler& save_handler() {
		return savehandler_;
	}

	uint32_t get_scenario_difficulty() const {
		return scenario_difficulty_;
	}
	void set_scenario_difficulty(uint32_t d) {
		assert(scenario_difficulty_ == kScenarioDifficultyNotSet);
		assert(d != kScenarioDifficultyNotSet);
		scenario_difficulty_ = d;
	}

	[[nodiscard]] Serial generate_economy_serial();
	[[nodiscard]] Serial generate_detectedportspace_serial();
	void notify_economy_serial(Serial serial);
	void notify_detectedportspace_serial(Serial serial);

	// Statistics
	const GeneralStatsVector& get_general_statistics() const {
		return general_stats_;
	}

	void read_statistics(FileRead& fr, uint16_t packet_version);
	void write_statistics(FileWrite& fw);

	void sample_statistics();

	const std::string& get_win_condition_displayname() const;
	void set_win_condition_displayname(const std::string& name);
	int32_t get_win_condition_duration() const;
	void set_win_condition_duration(int32_t);

	bool is_replay() const {
		return !replay_filename_.empty();
	}
	const std::string& replay_filename() const {
		assert(is_replay());
		return replay_filename_;
	}

	bool is_ai_training_mode() const {
		return ai_training_mode_;
	}

	bool is_auto_speed() const {
		return auto_speed_;
	}

	void set_ai_training_mode(bool);

	void set_auto_speed(bool);

	/**
	 * Cause the game to proceed with this file directly after the current game ends.
	 * A .wgf file will be loaded as a singleplayer savegame this way;
	 * a map file will be loaded as a singleplayer scenario.
	 */
	void set_next_game_to_load(const std::string& file);

	const std::list<std::string>& list_of_scenarios() const {
		return list_of_scenarios_;
	}

	TradeID propose_trade(TradeInstance trade);
	void accept_trade(TradeID trade_id, Market& receiver);
	void reject_trade(TradeID trade_id);
	void retract_trade(TradeID trade_id);
	void cancel_trade(TradeID trade_id, bool reached_regular_end, const Player* canceller);
	void move_trade(TradeID trade_id, Market& old_market, Market& new_market);

	void propose_trade_extension(PlayerNumber sender, TradeID trade_id, int32_t batches);
	void reject_trade_extension(PlayerNumber sender, TradeID trade_id, int32_t batches);
	void retract_trade_extension(PlayerNumber sender, TradeID trade_id, int32_t batches);
	void accept_trade_extension(PlayerNumber sender, TradeID trade_id, int32_t batches);

	[[nodiscard]] bool has_trade(TradeID trade_id) const {
		return trade_agreements_.count(trade_id) != 0;
	}
	[[nodiscard]] const TradeInstance& get_trade(TradeID trade_id) const {
		return trade_agreements_.at(trade_id);
	}
	[[nodiscard]] TradeInstance& get_mutable_trade(TradeID trade_id) {
		return trade_agreements_.at(trade_id);
	}
	[[nodiscard]] const std::map<TradeID, TradeInstance>& all_trade_agreements() const {
		return trade_agreements_;
	}
	[[nodiscard]] const std::vector<TradeExtension>& all_trade_extension_proposals() const {
		return trade_extension_proposals_;
	}
	[[nodiscard]] std::vector<TradeID> find_trade_offers(PlayerNumber receiver,
	                                                     Coords accept_at = Coords::null()) const;
	[[nodiscard]] std::vector<TradeID> find_trade_proposals(PlayerNumber initiator,
	                                                        Serial market_filter = 0) const;
	[[nodiscard]] std::vector<TradeID> find_active_trades(PlayerNumber player) const;
	[[nodiscard]] std::vector<TradeExtension>
	find_trade_extensions(TradeID trade_id, PlayerNumber player, bool as_proposer) const;

	struct PendingDiplomacyAction {
		PlayerNumber sender;     ///< The player who initiated the action.
		DiplomacyAction action;  ///< The action to perform.
		PlayerNumber other;      ///< The other player affected, if any.

		PendingDiplomacyAction(PlayerNumber p1, DiplomacyAction a, PlayerNumber p2)
		   : sender(p1), action(a), other(p2) {
		}

		inline bool operator==(const PendingDiplomacyAction& pda) const {
			return sender == pda.sender && action == pda.action && other == pda.other;
		}
	};
	const std::list<PendingDiplomacyAction>& pending_diplomacy_actions() const {
		return pending_diplomacy_actions_;
	}
	std::list<PendingDiplomacyAction>& pending_diplomacy_actions() {
		return pending_diplomacy_actions_;
	}

	[[nodiscard]] bool diplomacy_allowed() const {
		return diplomacy_allowed_;
	}
	void set_diplomacy_allowed(bool allow) {
		diplomacy_allowed_ = allow;
	}

	[[nodiscard]] bool naval_warfare_allowed() const {
		return naval_warfare_allowed_;
	}
	void set_naval_warfare_allowed(bool allow) {
		naval_warfare_allowed_ = allow;
	}

private:
	bool did_postload_addons_before_loading_{false};

	void sync_reset();

	crypto::MD5Checksummer synchash_;

	struct SyncWrapper : public StreamWrite {
		SyncWrapper(Game& game, crypto::MD5Checksummer& target) : game_(game), target_(target) {
		}

		~SyncWrapper() override;

		/// Start dumping the entire syncstream into a file.
		///
		/// Note that this file is deleted at the end of the game, unless
		/// \ref syncstreamsave_ has been set.
		void start_dump(const std::string& fname);

		void data(void const* data, size_t size) override;

	public:
		Game& game_;
		crypto::MD5Checksummer& target_;
		uint32_t counter_{0U};
		uint32_t next_diskspacecheck_{0U};
		std::unique_ptr<StreamWrite> dump_;
		std::string dumpfname_;
		bool syncstreamsave_{false};
		// Use a cyclic buffer for storing parts of the syncstream
		// Currently used buffer
		size_t current_excerpt_id_{0U};
		// (Arbitrary) count of buffers
		// Syncreports seem to be requested from the network clients every game second so this
		// buffer should be big enough to store the last 32 seconds of the game actions leading
		// up to the desync
		static constexpr size_t kExcerptSize = 32;
		// Array of byte buffers
		// std::string is used as a binary buffer here
		std::string excerpts_buffer_[kExcerptSize];
	} syncwrapper_;

	void delete_pending_player_commands();
	void do_send_player_command(PlayerCommand*);
	std::shared_ptr<GameController> ctrl_;

	/// Whether a replay writer should be created.
	/// Defaults to \c true, and should only be set to \c false for playing back
	/// replays.
	bool writereplay_{true};

	/// Whether a syncsteam file should be created.
	/// Defaults to \c false, and can be set to true for network games. The file
	/// is written only if \ref writereplay_ is true too.
	bool writesyncstream_{false};

	bool ai_training_mode_{false};
	bool auto_speed_{false};

	int32_t state_{gs_notrunning};

	RNG rng_;

	CmdQueue cmdqueue_;
	std::deque<PlayerCommand*> pending_player_commands_;

	SaveHandler savehandler_;

	std::unique_ptr<ReplayWriter> replaywriter_;

	uint32_t scenario_difficulty_{kScenarioDifficultyNotSet};

	GeneralStatsVector general_stats_;

	TradeID next_trade_agreement_id_ = 1;
	// Maps from trade agreement id to the agreement.
	std::map<TradeID, TradeInstance> trade_agreements_;
	std::vector<TradeExtension> trade_extension_proposals_;

	[[nodiscard]] bool check_trade_player_matches(const TradeInstance& trade,
	                                              PlayerNumber sender,
	                                              PlayerNumber proposer,
	                                              bool check_recipient,
	                                              Player** p1,
	                                              Player** p2,
	                                              const Market** market);

	Serial last_economy_serial_ = 0;
	Serial last_detectedportspace_serial_ = 0;

	std::list<PendingDiplomacyAction> pending_diplomacy_actions_;
	bool diplomacy_allowed_{true};
	bool naval_warfare_allowed_{false};

	/// For save games and statistics generation
	std::string win_condition_displayname_;

	int32_t win_condition_duration_{kDefaultWinConditionDuration};

#if 0  // TODO(Nordfriese): Re-add training wheels code after v1.0
	std::unique_ptr<TrainingWheels> training_wheels_;
	bool training_wheels_wanted_{false};
#endif

	/** Filename of the replay represented by this game, or empty if this is not a replay. */
	std::string replay_filename_;

	std::string next_game_to_load_;
	std::list<std::string> list_of_scenarios_;
};

inline Coords Game::random_location(Coords location, uint8_t radius) {
	const uint16_t s = radius * 2 + 1;
	location.x += logic_rand() % s - radius;
	location.y += logic_rand() % s - radius;
	map().normalize_coords(location);
	return location;
}
}  // namespace Widelands

#endif  // end of include guard: WL_LOGIC_GAME_H
