include "scripting/coroutine.lua"
include "scripting/infrastructure.lua"
include "scripting/ui.lua"
include "test/scripting/lunit.lua"
include "test/scripting/stable_save.lua"

game = wl.Game()
map = game.map
p1 = game.players[1]
p2 = game.players[2]
p1.see_all = true

p1:allow_workers("all")

function full_headquarters(player, x, y)
   prefilled_buildings(player, { "barbarians_headquarters", x, y })
   hq = player:get_buildings("barbarians_headquarters")[1]
   for idx,descr in ipairs(game:get_tribe_description("barbarians").wares) do
      hq:set_wares(descr.name, 350)
   end
   for idx,descr in ipairs(game:get_tribe_description("barbarians").workers) do
      if descr.type_name ~= "soldier" and descr.type_name ~= "ferry" then
         hq:set_workers(descr.name, 50)
      end
   end
   hq:set_soldiers({0,0,0,0}, 30)
end

function place_markets()
   prefilled_buildings(p1, { "barbarians_market", 22, 28 })
   market_p1 = map:get_field(22, 28).immovable
   connected_road("normal", p1, market_p1.flag, "tr,tr,tl|", true)

   prefilled_buildings(p2, { "barbarians_market", 31, 28 })
   market_p2 = map:get_field(31, 28).immovable
   connected_road("normal", p2, market_p2.flag, "tr,tr,tl|", true)
end

full_headquarters(p1, 22, 25)
full_headquarters(p2, 31, 25)

game.desired_speed = 50000
