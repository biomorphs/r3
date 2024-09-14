require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/world_generator'
require 'arrrgh/generator_steps'

Arrrgh_Globals.WorldGenerator = nil

function Dungeons_GenerateWorld_WanderToGoal(grid, inParams, spawnPos, goalPos)
	local floorTag = TileTagset.new("floor,exterior")
	local stepsPerYield = inParams:GetInt("Wander steps per iteration", 20)
	local forwardChanceIncrement = 0.0000001
	local gridSize = grid:GetDimensions()
	-- random stumble by one tile until we hit the goal
	local myPos = uvec2.new(spawnPos.x, spawnPos.y)
	local chanceToGoForward = 0					-- slowly increases by some tiny amount
	local iterationCount = 0					-- to determine how often to yield
	repeat
		local nextPos = myPos
		local direction = math.random(0,3)		-- up, right, down, left
		if(math.random() < chanceToGoForward) then	-- chance to go in right direction
			if(math.abs(nextPos.x - goalPos.x) > math.abs(nextPos.y - goalPos.y)) then 
				if(nextPos.x > goalPos.x) then 
					direction = 3	-- left 
				else 
					direction = 1	-- right
				end
			else
				if(nextPos.y > goalPos.y) then 
					direction = 2	-- down 
				else 
					direction = 0	-- up
				end
			end
		end
		chanceToGoForward = chanceToGoForward + forwardChanceIncrement
		if(direction == 0 and nextPos.y + 1 < gridSize.y) then 
			nextPos.y = nextPos.y + 1
		elseif(direction == 1 and nextPos.x + 1 < gridSize.x) then
			nextPos.x = nextPos.x + 1
		elseif(direction == 2 and nextPos.y > 0) then 
			nextPos.y = nextPos.y - 1
		elseif(direction == 3 and nextPos.x > 0) then
			nextPos.x = nextPos.x - 1
		end
		myPos.x = math.floor(nextPos.x)
		myPos.y = math.floor(nextPos.y)
		grid:Fill(myPos, uvec2.new(1,1), floorTag, true, false)
		iterationCount = iterationCount + 1
		if(iterationCount > stepsPerYield) then 
			YieldGenerator(0)
			iterationCount = 0
		end
	until (myPos.x == goalPos.x) and (myPos.y == goalPos.y)
	grid.m_isDirty = true	-- update graphics
	YieldGenerator()
end

function Dungeons_CreatePlayerSpawn(grid, spawnPos)
	local world = R3.ActiveWorld()
	local spawnPointEntities = world:ImportScene('arrrgh/pois/playerspawnpoint.scn')
	local actualPos = vec3.new(spawnPos.x * Arrrgh_Globals.TileDimensions.x, 0, spawnPos.y * Arrrgh_Globals.TileDimensions.y)
	actualPos.x = actualPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
	actualPos.z = actualPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
	Arrrgh.MoveEntitiesWorldspace(spawnPointEntities, actualPos)
end

function Dungeons_BasicGenerator()
	local roomCount = 10
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Center Camera", Generator_CenterCamera())
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Empty", Generator_FillWorld("",true,false))	-- start with empty world of passable tiles
	for r=1,roomCount do 
		Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Add Room", Generator_SimpleRoom(uvec2.new(math.random(5,60),math.random(5,60)), uvec2.new(math.random(4,8),math.random(4,8)), "wall,floor,exterior", "floor,interior"))
	end
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Make paths between rooms", Generator_PathFromRoomToRoom("floor,interior"))
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Make spawn point", Generator_SpawnPlayerInFirstRoom())
	Dungeons_Generator.AddStep(Arrrgh_Globals.WorldGenerator, "Fill empty tiles with impassables", Generator_FillEmptyTiles("wall,floor,exterior", false, false))
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
			local totalWorldSize = scriptcmp.m_inputParams:GetIntVec2("Total World Size", ivec2.new(32,32))
			gridcmp:ResizeGrid(uvec2.new(totalWorldSize.x,totalWorldSize.y))
			Arrrgh_Globals.WorldGenerator = Dungeons_Generator.new(gridEntity)
			Dungeons_BasicGenerator()
		end
		if(Dungeons_Generator.Continue(Arrrgh_Globals.WorldGenerator) == 'complete') then
			gridcmp.m_isDirty = true	-- update world graphics
			scriptcmp.m_isActive = false -- disable scripts
			Dungeons_CreatePlayerSpawn(gridcmp, Arrrgh_Globals.WorldGenerator.Context.SpawnPoint)	-- player spawn point
			local gameUpdate = world:GetEntityByName('GameUpdate')
			local updateScript = world.GetComponent_LuaScript(gameUpdate)
			updateScript.m_isActive = true	-- start game
		end
	else
		print('No Grid')
	end	
end
