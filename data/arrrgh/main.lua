local Arrrgh_Globals = {}

function Dungeons_PopulateInputs(luacmp)
	luacmp.m_inputParams:TryAddIntVec2("Total World Size", ivec2.new(32,32))
	luacmp.m_inputParams:TryAddIntVec2("Min Building Size", ivec2.new(5,5))
	luacmp.m_inputParams:TryAddIntVec2("Max Building Size", ivec2.new(16,16))
end

function Dungeons_GenerateSimpleBuilding(grid, start, size)
	grid:Fill(start, size, 1, false) -- outer walls
	grid:Fill(uvec2.new(start.x + 1, start.y + 1), uvec2.new(size.x - 2, size.y - 2), 2, true) -- interior floor

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
	grid:Fill(doorPos, uvec2.new(1, 1), 2, true)
end

function FillWithBuildings(grid,minBuildingSize,maxBuildingSize)
	local gridDims = grid:GetDimensions()
	local maxAttempts = 4000
	local maxBuildings = 100
	local totalBuildings = 0
	for attempt=1,maxAttempts do 
		local buildingSize = uvec2.new(math.random(minBuildingSize.x,maxBuildingSize.x),math.random(minBuildingSize.y,maxBuildingSize.y))
		local buildingPosition = uvec2.new(math.random(2, gridDims.x - buildingSize.x), math.random(2, gridDims.y - buildingSize.x))
		if(grid:AllTilesMatchType(buildingPosition, buildingSize, 3)) then -- if area only contains outdoor floor
			Dungeons_GenerateSimpleBuilding(grid, buildingPosition, buildingSize)
			totalBuildings = totalBuildings + 1
			if(totalBuildings == maxBuildings) then 
				print(totalBuildings, ' buildings generated')
				return 
			end
			coroutine.yield()
		end
	end
	print(totalBuildings, ' buildings generated')
end

function Dungeons_GenerateWorld(grid, luacmp)
	-- get the generation params from the script component
	local totalWorldSize = luacmp.m_inputParams:GetIntVec2("Total World Size", ivec2.new(32,32))
	local minBuildingSize = luacmp.m_inputParams:GetIntVec2("Min Building Size", ivec2.new(5,5))
	local maxBuildingSize = luacmp.m_inputParams:GetIntVec2("Max Building Size", ivec2.new(16,16))

	local gridSize = uvec2.new(totalWorldSize.x,totalWorldSize.y)	-- need to use uvec2 from here
	grid:ResizeGrid(gridSize)

	-- fill world with empty tiles
	grid:Fill(uvec2.new(0,0), gridSize, 0, false)

	coroutine.yield()

	if(gridSize.x < 2 or gridSize.y < 2) then
		print('too small')
	end

	-- fill most of world with walkable floor tiles
	grid:Fill(uvec2.new(1,1), uvec2.new(gridSize.x - 2, gridSize.y - 2), 3, true)

	coroutine.yield()

	-- add buildings
	FillWithBuildings(grid, minBuildingSize, maxBuildingSize)
end

-- main entry point, called from variable update
function Dungeons_VariableUpdate(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local scriptcmp = world.GetComponent_LuaScript(e)
	if(scriptcmp ~= nil and gridcmp ~= nil) then
		if(Arrrgh_Globals.genCoroutine == nil) then 
			print('Starting dungeon generator')
			Arrrgh_Globals.genCoroutine = coroutine.create( Dungeons_GenerateWorld )
			gridcmp.m_debugDraw = true
		end
		local runningStatus = coroutine.status(Arrrgh_Globals.genCoroutine)
		if(runningStatus ~= 'dead') then
			coroutine.resume(Arrrgh_Globals.genCoroutine, gridcmp, scriptcmp)
		else
			Arrrgh_Globals.genCoroutine = nil
			gridcmp.m_isDirty = true		-- update the graphics once at the end
			gridcmp.m_debugDraw = false
			scriptcmp.m_isActive = false	-- we are done, stop running
			print('Dungeon finished')
		end
	else
		print('no grid')
	end	
end