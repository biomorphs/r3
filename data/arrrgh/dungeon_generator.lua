require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/world_generator'
require 'arrrgh/generator_steps'

Arrrgh_Globals.WorldGenerator = nil

-- generates a bunch of rooms, connects them with paths, surrounds everything with walls
-- spawns a single monster in each room but the first
function Dungeons_BasicGenerator()
	local roomCount = 10
	Arrrgh.SetFogOfWarEnabled(true)
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Center Camera", Generator_CenterCamera())
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Empty", Generator_FillWorld("",true,false))	-- start with empty world of passable tiles
	for r=1,roomCount do 
		local roomPos = uvec2.new(math.random(5,50),math.random(5,50))
		local roomSize = uvec2.new(math.random(5,9),math.random(5,9))
		Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add Room", Generator_SimpleRoom(roomPos, roomSize, "wall,floor,exterior", "floor,interior"))
		-- add a monster spawner in each room but the first
		if(r > 1) then 
			local monsterPos = uvec2.new(roomPos.x + 1, roomPos.y + 1)
			Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add monster spawner", Generator_SpawnMonster("", monsterPos))
		end
	end
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Make paths between rooms", Generator_PathFromRoomToRoom("floor,interior", 0.5))
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Make spawn point", Generator_SpawnPlayerInFirstRoom())
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Fill empty tiles with impassables", Generator_FillEmptyTiles("wall,floor,exterior", false, true))
end

-- main entry point, called from variable update
-- generator runs as a coroutine
function Dungeons_GenerateWorld(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local scriptcmp = world.GetComponent_LuaScript(e)
	if(scriptcmp ~= nil and gridcmp ~= nil) then
		if(Arrrgh_Globals.WorldGenerator == nil) then 
			gridcmp:ResizeGrid(uvec2.new(64,64))
			Arrrgh_Globals.WorldGenerator = Dungeons_Generator.new(gridEntity)
			Dungeons_BasicGenerator()
		end
		if(Dungeons_Generator.Continue(Arrrgh_Globals.WorldGenerator) == 'complete') then
			gridcmp.m_isDirty = true	-- update world graphics
			scriptcmp.m_isActive = false -- disable scripts
			local gameUpdate = world:GetEntityByName('GameUpdate')
			local updateScript = world.GetComponent_LuaScript(gameUpdate)
			updateScript.m_isActive = true	-- start game,  this should not be happening here! fine for now though
		end
	else
		print('No Grid')
	end	
end
