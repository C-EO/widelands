push_textdomain("tribes")

local dirname = path.dirname(__file__)

wl.Descriptions():new_productionsite_type {
   name = "atlanteans_gold_spinning_mill",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext("atlanteans_building", "Gold Spinning Mill"),
   icon = dirname .. "menu.png",
   size = "small",

   buildcost = {
      log = 2,
      granite = 1,
      planks = 1
   },
   return_on_dismantle = {
      log = 1,
      granite = 1
   },

   animation_directory = dirname,
   animations = {
      idle = {
         hotspot = { 34, 74 },
      },
      working = {
         basename = "idle", -- TODO(GunChleoc): No animation yet.
         hotspot = { 34, 74 },
      }
   },

   aihints = {
      prohibited_till = 1210
   },

   working_positions = {
      atlanteans_carrier = 1
   },

   inputs = {
      { name = "gold", amount = 5 }
   },

   programs = {
      main = {
         -- TRANSLATORS: Completed/Skipped/Did not start spinning gold because ...
         descname = _("spinning gold"),
         actions = {
            -- time total: 15.4 + 25 + 3.6 = 44 sec
            "return=skipped unless economy needs gold_thread",
            "consume=gold",
            "sleep=duration:15s400ms",
            "playsound=sound/atlanteans/goldspin priority:50% allow_multiple",
            "animate=working duration:25s",
            "produce=gold_thread"
         }
      },
   },
}

pop_textdomain()
