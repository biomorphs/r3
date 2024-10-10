require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/world_generator'
require 'arrrgh/generator_steps'

Arrrgh_Globals.WorldGenerator = nil
Arrrgh_Globals.WorldMaxSize = uvec2.new(64,64)

-- rooms is list of {pos,size}
-- outerBorder = additional gap to leave between buildings
function Dungeons_DoRoomsOverlap(roomPos, roomSize, rooms, outerBorder)
	for r=1,#rooms do 
		-- we add a +1 border to 
		local ral = rooms[r][1].x - outerBorder
		local rar = rooms[r][1].x + rooms[r][2].x + outerBorder
		local rab = rooms[r][1].y - outerBorder
		local rat = rooms[r][1].y + rooms[1][2].y + outerBorder
		local rbl = roomPos.x 
		local rbr = roomPos.x + roomSize.x
		local rbb = roomPos.y 
		local rbt = roomPos.y + roomSize.y
		if (ral < rbr and rar > rbl and rat > rbb and rab < rbt) then 
			return true
		end
	end
	return false
end

-- generates a bunch of rooms, connects them with paths, surrounds everything with walls
-- spawns a single monster in each room but the first
function Dungeons_BasicGenerator()
	local roomCount = 64
	local rooms = {}
	local attempts = 0
	while(attempts < 1000 and #rooms < roomCount) do
		local roomPos = uvec2.new(math.random(3,Arrrgh_Globals.WorldMaxSize.x - 7),math.random(3,Arrrgh_Globals.WorldMaxSize.y - 7))
		local roomSize = uvec2.new(math.random(4,7),math.random(4,7))
		if(Dungeons_DoRoomsOverlap(roomPos,roomSize,rooms,1) == false) then
			table.insert(rooms, {roomPos, roomSize})
		end
		attempts = attempts + 1
	end

	Arrrgh.SetFogOfWarEnabled(true)
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Center Camera", Generator_CenterCamera())
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Empty", Generator_FillWorld("",true,false))	-- start with empty world of passable tiles
	for r=1,#rooms do 
		local roomPos = rooms[r][1]
		local roomSize = rooms[r][2]
		Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add Room", Generator_SimpleRoom(roomPos, roomSize, "wall,floor,exterior", "floor,interior"))
		for m=1,4 do 
			if(r > 1 and math.random() < 0.33) then	-- add monster spawners in each room but the first
				local spawnX = math.random(1, roomSize.x-2)
				local spawnY = math.random(1, roomSize.y-2)
				local monsterPos = uvec2.new(roomPos.x + spawnX, roomPos.y + spawnY)
				Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add monster spawner", Generator_SpawnMonster("", monsterPos))
			end
		end
		if(math.random() < 0.75) then -- add an item in each room
			local spawnX = math.random(1, roomSize.x-2)
			local spawnY = math.random(1, roomSize.y-2)
			local itemPos = uvec2.new(roomPos.x + spawnX, roomPos.y + spawnY)
			Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add item", Generator_SpawnItem("", itemPos))
		end
		if(math.random() < 0.9) then -- add a torch to each room
			local spawnX = math.random(1, roomSize.x-2)
			local spawnY = roomSize.y - 1
			local itemPos = uvec2.new(roomPos.x + spawnX, roomPos.y + spawnY)
			Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add torch", Generator_AddProp("Torch", itemPos, 0.0))
		end
		if(math.random() < 0.3) then -- add a bookshelf to each room
			local spawnX = math.random(1, roomSize.x-2)
			local spawnY = roomSize.y - 1
			local itemPos = uvec2.new(roomPos.x + spawnX, roomPos.y + spawnY)
			Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add Bookshelves", Generator_AddProp("Bookshelves", itemPos, 0.0))
		end
	end
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Make paths between rooms", Generator_PathFromRoomToRoom("floor,interior", 0.02))
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
			gridcmp:ResizeGrid(uvec2.new(Arrrgh_Globals.WorldMaxSize.x,Arrrgh_Globals.WorldMaxSize.y))
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
