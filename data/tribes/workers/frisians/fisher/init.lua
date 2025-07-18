push_textdomain("tribes")

local dirname = path.dirname (__file__)

wl.Descriptions():new_worker_type {
   name = "frisians_fisher",
   -- TRANSLATORS: This is a worker name used in lists of workers
   descname = pgettext ("frisians_worker", "Fisher"),
   animation_directory = dirname,
   icon = dirname .. "menu.png",
   vision_range = 2,

   buildcost = {
      frisians_carrier = 1,
      fishing_net = 1
   },

   programs = {
      fish = {
         -- steps from building to water: 2-11, min+max average 6.5
         -- min. worker time: 2 * 2 * 1.8 + 3 = 10.2 sec
         -- max. worker time: 2 * 11 * 1.8 + 3 = 42.6 sec
         -- avg. worker time: 2 * 6.5 * 1.8 + 3 = 26.4 sec
         "findspace=size:any radius:8 resource:resource_fish",
         "walk=coords",
         "mine=resource_fish radius:1",
         "animate=fishing duration:3s",
         "createware=fish",
         "return"
      },
      fish_in_pond = {
         -- steps from building to pond: 2-11
         -- min. worker time: 2 * 2 * 1.8 + 3 = 10.2 sec
         -- max. worker time: 2 * 11 * 1.8 + 3 = 42.6 sec
         "findobject=attrib:pond_mature radius:8",
         "walk=object",
         "animate=fishing duration:3s",
         "callobject=fall_dry",
         "createware=fish",
         "return"
      },
      breed_in_pond = {
         -- steps from building to pond: 2-11
         -- min. worker time: 2 * 2 * 1.8 + 2 = 9.2 sec
         -- max. worker time: 2 * 11 * 1.8 + 2 = 41.6 sec
         "findobject=attrib:pond_dry radius:8",
         "walk=object",
         "animate=release duration:2s",
         "callobject=with_fish",
         "return"
      }
   },

   ware_hotspot = {0, 20},

   spritesheets = {
      walk = {
         fps = 15,
         frames = 10,
         columns = 5,
         rows = 2,
         directional = true,
         hotspot = {13, 25}
      },
      walkload = {
         fps = 15,
         frames = 10,
         columns = 5,
         rows = 2,
         directional = true,
         hotspot = {13, 26}
      },
      fishing = {
         fps = 10,
         frames = 30,
         columns = 5,
         rows = 6,
         hotspot = {9, 23}
      },
      release = {
         fps = 10,
         frames = 20,
         columns = 5,
         rows = 4,
         hotspot = {15, 20}
      },
   },
   animations = {
      idle = {
         hotspot = {8, 25}
      },
   },
}

pop_textdomain()
