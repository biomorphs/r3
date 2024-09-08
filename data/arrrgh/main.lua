local Arrrgh_Globals = {}
Arrrgh_Globals.FillWithExteriorFloor = false
Arrrgh_Globals.DoRandomWander = true
Arrrgh_Globals.TileDimensions = vec2.new(4,4)

function Dungeons_PopulateInputs(luacmp)
	luacmp.m_inputParams:AddIntVec2("Total World Size", ivec2.new(32,32))
	luacmp.m_inputParams:AddIntVec2("Min Building Size", ivec2.new(5,5))
	luacmp.m_inputParams:AddIntVec2("Max Building Size", ivec2.new(16,16))
	luacmp.m_inputParams:AddIntVec2("Level Exit Safe Area (Min/Max)", ivec2.new(3,5))
	luacmp.m_inputParams:AddIntVec2("Player Spawn Safe Area (Min/Max)", ivec2.new(2,4))
	luacmp.m_inputParams:AddInt("Min Tower Radius", 3)
	luacmp.m_inputParams:AddInt("Tower Wall Thickness", 1)
	luacmp.m_inputParams:AddInt("Max Building Iterations", 100000)
	luacmp.m_inputParams:AddInt("Wander steps per iteration", 20)
	luacmp.m_inputParams:AddInt("Max Buildings", 200)
	luacmp.m_inputParams:AddFloat("Tower Chance", 0.5)
end

-- yields until time passed
function YieldGenerator(timeToWait)
	timeToWait = timeToWait or 0.25
	local currentWaitTime = 0 
	repeat
		currentWaitTime = currentWaitTime + R3.GetVariableDelta() -- variable delta is good enough
		coroutine.yield()
	until(currentWaitTime >= timeToWait)
end

function DistanceBetween(v0,v1)
	local tov1 = vec2.new(v0.x - v1.x, v0.y - v1.y)
	return math.sqrt((tov1.x * tov1.x) + (tov1.y * tov1.y))
end

function Dungeons_FillCircle(grid, center, radius, tiletype, isPassable, blocksVisibility)
	local areaCenter = vec2.new(center.x, center.y)
	local start = vec2.new(center.x - radius, center.y - radius)
	local size = vec2.new(radius * 2, radius * 2)
	for z=start.y,(start.y + size.y) do 
		for x=start.x,(start.x + size.x) do 
			local distanceToCenter = DistanceBetween(vec2.new(x,z), areaCenter)
			if(distanceToCenter <= radius) then
				grid:Fill(uvec2.new(math.tointeger(x),math.tointeger(z)), uvec2.new(1,1), tiletype, isPassable, blocksVisibility)
			end
		end
	end
end


function Dungeons_GenerateSimpleBuilding(grid, inParams, start, size)
	grid:Fill(start, size, 1, false, true) -- outer walls
	grid:Fill(uvec2.new(start.x + 1, start.y + 1), uvec2.new(size.x - 2, size.y - 2), 2, true, false) -- interior floor

	-- make a door
	local doorPos = {}
	local wallForDoor = math.random(0, 3)
	if(wallForDoor == 0) then -- top
		doorPos = uvec2.new(math.random(start.x + 1, start.x + size.x - 2), start.y)
	elseif(wallForDoor == 1) then -- right
		doorPos = uvec2.new(start.x + size.x - 1, math.random(start.y + 1, start.y + size.y - 2))
	elseif(wallForDoor == 2) then -- bottom
		doorPos = uvec2.new(math.random(start.x + 1, start.x + size.x - 2), start.y + size.y - 1)
	elseif(wallForDoor == 3) then -- left
		doorPos = uvec2.new(start.x, math.random(start.y + 1, start.y + size.y - 2))
	end
	grid:Fill(doorPos, uvec2.new(1, 1), 2, true, false)
	return true
end

function Dungeons_GenerateRoundTower(grid, inParams, start, size)
	local minTowerRadius = inParams:GetInt("Min Tower Radius", 3)
	local tileWallThickness = inParams:GetInt("Tower Wall Thickness", 1)
	local towerRadius = math.floor(math.min(size.x, size.y) * 0.5)
	local towerCenter = vec2.new(start.x + (size.x * 0.5), start.y + (size.y * 0.5))
	if(towerRadius < minTowerRadius) then 
		return false
	end
	for z=start.y,(start.y + size.y) do 
		for x=start.x,(start.x + size.x) do 
			local distanceToCenter = DistanceBetween(vec2.new(x,z), towerCenter)
			if(distanceToCenter <= towerRadius) then
				if((towerRadius - distanceToCenter) <= tileWallThickness) then
					grid:Fill(uvec2.new(x,z), uvec2.new(1,1), 1, false, true)	-- outer wall
				else
					grid:Fill(uvec2.new(x,z), uvec2.new(1,1), 2, true, false)	-- floor
				end
			end
		end
	end
	return true
end

function FillWithBuildings(grid,inParams)
	local minBuildingSize = inParams:GetIntVec2("Min Building Size", ivec2.new(5,5))
	local maxBuildingSize = inParams:GetIntVec2("Max Building Size", ivec2.new(16,16))
	local towerChance = inParams:GetFloat("Tower Chance", 0.5)
	local maxAttempts = inParams:GetInt("Max Building Iterations", 200000)
	local maxBuildings = inParams:GetInt("Max Buildings", 500)
	local gridDims = grid:GetDimensions()
	if(gridDims.x < 2 or gridDims.y < 2) then
		print('too small')
		return
	end
	local totalBuildings = 0
	for attempt=1,maxAttempts do 
		-- detect areas to build on
		local buildingSize = uvec2.new(math.random(minBuildingSize.x,maxBuildingSize.x),math.random(minBuildingSize.y,maxBuildingSize.y))
		local buildingPosition = uvec2.new(math.random(2, gridDims.x - buildingSize.x), math.random(2, gridDims.y - buildingSize.x))
		local testAreaSize = uvec2.new(buildingSize.x + 2, buildingSize.y + 2)
		local testAreaPos = uvec2.new(buildingPosition.x -1, buildingPosition.y - 1)
		
		if(grid:AllTilesMatchType(testAreaPos, testAreaSize, 3)) then -- if area only contains outdoor floor
			local buildingCreated = false
			if(math.random() <= towerChance) then 
				buildingCreated = Dungeons_GenerateRoundTower(grid, inParams, buildingPosition, buildingSize)
			end
			if(buildingCreated == false) then 
				buildingCreated = Dungeons_GenerateSimpleBuilding(grid, inParams, buildingPosition, buildingSize)
			end
			if(buildingCreated) then 
				totalBuildings = totalBuildings + 1
				if(totalBuildings == maxBuildings) then 
					print(totalBuildings, ' buildings generated')
					return 
				end
				YieldGenerator(0.0)
			end
		end
	end
	print(totalBuildings, ' buildings generated. attempts=', maxAttempts)
end

function Dungeons_GenerateWorld_WanderToGoal(grid, inParams, spawnPos, goalPos)
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
		grid:Fill(myPos, uvec2.new(1,1), 3, true, false)
		iterationCount = iterationCount + 1
		if(iterationCount > stepsPerYield) then 
			YieldGenerator(0)
			iterationCount = 0
		end
	until (myPos.x == goalPos.x) and (myPos.y == goalPos.y)
	grid.m_isDirty = true	-- update graphics
	YieldGenerator()
end

function Dungeons_CreatePlayerSpawn(grid, inParams, spawnPos)
	local world = R3.ActiveWorld()
	local spawnPointEntities = world:ImportScene('arrrgh/pois/playerspawnpoint.scn')
	local actualPos = vec3.new(spawnPos.x * Arrrgh_Globals.TileDimensions.x, 0, spawnPos.y * Arrrgh_Globals.TileDimensions.y)
	actualPos.x = actualPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
	actualPos.z = actualPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
	Arrrgh.MoveEntities(spawnPointEntities, actualPos)
end

function Dungeons_CreateLevelExit(grid, inParams, pos)
	local world = R3.ActiveWorld()
	local levelExit = world:ImportScene('arrrgh/pois/levelexit.scn')
	local actualPos = vec3.new(pos.x * Arrrgh_Globals.TileDimensions.x, 0, pos.y * Arrrgh_Globals.TileDimensions.y)
	actualPos.x = actualPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
	actualPos.z = actualPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
	Arrrgh.MoveEntities(levelExit, actualPos)
end

function Dungeons_GenerateLevelExitSafeArea(grid, inParams, goalPos)
	local safeAreaSizeRange = inParams:GetIntVec2("Level Exit Safe Area (Min/Max)", ivec2.new(2,4))
	local areaRadius = math.random(safeAreaSizeRange.x, safeAreaSizeRange.y)
	local areaCenter = vec2.new(goalPos.x, goalPos.y)
	Dungeons_FillCircle(grid, areaCenter, areaRadius, 3, true, false)
end

function Dungeons_GeneratePlayerSpawnSafeArea(grid, inParams, pos)
	local safeAreaSizeRange = inParams:GetIntVec2("Player Spawn Safe Area (Min/Max)", ivec2.new(2,4))
	local areaRadius = math.random(safeAreaSizeRange.x, safeAreaSizeRange.y)
	local areaCenter = vec2.new(pos.x, pos.y)
	Dungeons_FillCircle(grid, areaCenter, areaRadius, 3, true, false)
end

function Dungeons_CoGenerateWorld(grid, inParams)
	local totalWorldSize = inParams:GetIntVec2("Total World Size", ivec2.new(32,32))
	local gridSize = uvec2.new(totalWorldSize.x,totalWorldSize.y)	-- need to use uvec2 from here
	grid:ResizeGrid(gridSize)

	-- fill world with empty tiles + update graphics entities
	grid:Fill(uvec2.new(0,0), gridSize, 0, false, false)
	grid.m_isDirty = true
	YieldGenerator()

	-- choose a minimum distance between start + goal based on world size 
	local minDistance = math.floor(math.max(gridSize.x, gridSize.y) * 0.8)
	local spawnPos = {}	-- choose a spawn tile and goal tile, making sure they are far away
	local goalPos = {}
	repeat
		spawnPos = uvec2.new(math.random(2, gridSize.x - 2), math.random(2, gridSize.y - 2))
		goalPos = uvec2.new(math.random(2, gridSize.x - 2), math.random(2, gridSize.y - 2))
		local distance = DistanceBetween(spawnPos,goalPos)
	until distance >= minDistance

	-- make the spawn + goal entities 
	Dungeons_CreatePlayerSpawn(grid, inParams, spawnPos)
	Dungeons_GeneratePlayerSpawnSafeArea(grid, inParams, spawnPos)
	Dungeons_CreateLevelExit(grid, inParams, goalPos)
	Dungeons_GenerateLevelExitSafeArea(grid, inParams, goalPos)

	grid.m_debugDraw = true

	if(Arrrgh_Globals.FillWithExteriorFloor) then 
		grid:Fill(uvec2.new(0,0), gridSize, 3, true, false)
		grid.m_isDirty = true
		YieldGenerator()
	end
	
	-- random wander between start + goal
	if(Arrrgh_Globals.DoRandomWander) then 
		Dungeons_GenerateWorld_WanderToGoal(grid, inParams, spawnPos, goalPos)
	end

	-- fill the start + goal tiles
	grid:Fill(spawnPos, uvec2.new(1,1), 4, true, false)	-- 4 = player spawn 
	YieldGenerator(1.0)
	grid:Fill(goalPos, uvec2.new(1,1), 5, true, false)	-- 5 = level exit 
	YieldGenerator(1.0)
	
	grid.m_isDirty = true	-- update graphics
	YieldGenerator()
	FillWithBuildings(grid, inParams)
	grid.m_debugDraw = false
end

-- main entry point, called from variable update
function Dungeons_GenerateWorld(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local scriptcmp = world.GetComponent_LuaScript(e)
	if(scriptcmp ~= nil and gridcmp ~= nil) then
		if(Arrrgh_Globals.genCoroutine == nil) then 
			print('Starting dungeon generator')
			Arrrgh_Globals.genCoroutine = coroutine.create( Dungeons_CoGenerateWorld )
		end
		local runningStatus = coroutine.status(Arrrgh_Globals.genCoroutine)
		if(runningStatus ~= 'dead') then
			coroutine.resume(Arrrgh_Globals.genCoroutine, gridcmp, scriptcmp.m_inputParams)
		else
			Arrrgh_Globals.genCoroutine = nil
			gridcmp.m_isDirty = true		-- update the graphics once at the end
			scriptcmp.m_isActive = false	-- we are done, stop running
			print('Generator finished')
		end
	else
		print('No Grid')
	end	
end

function Dungeons_PathfindTest(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local spawnEntity = world:GetEntityByName('PlayerSpawnPoint')
	local spawnTransform = world.GetComponent_Transform(spawnEntity)
	local myTransform = world.GetComponent_Transform(e)
	if(spawnTransform == nil or gridcmp == nil or myTransform == nil) then 
		return
	end
	local myTile = Arrrgh.GetTileFromWorldspace(gridcmp, myTransform:GetPosition())
	local spawnTile = Arrrgh.GetTileFromWorldspace(gridcmp, spawnTransform:GetPosition())
	local foundPath = gridcmp:CalculatePath(myTile, spawnTile)
	Arrrgh.DebugDrawTiles(gridcmp, foundPath)
end