
local Dungeons = {}

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

function FillWithBuildings(grid)
	local gridDims = grid:GetDimensions()
	local minBuildingSize = uvec2.new(5,5)
	local maxBuildingSize = uvec2.new(16, 16)
	local maxAttempts = 100
	local maxBuildings = 100
	local totalBuildings = 0
	for attempt=1,maxAttempts do 
		local buildingSize = uvec2.new(math.random(minBuildingSize.x,maxBuildingSize.x),math.random(minBuildingSize.y,maxBuildingSize.y))
		local buildingPosition = uvec2.new(math.random(2, gridDims.x - 7), math.random(2, gridDims.y - 7))
		if((buildingSize.x + buildingPosition.x) < (gridDims.x - 2) and (buildingSize.y + buildingPosition.y) < (gridDims.y - 2)) then 
			if(grid:AllTilesMatchType(buildingPosition, buildingSize, 3)) then -- if area only contains outdoor floor
				Dungeons_GenerateSimpleBuilding(grid, buildingPosition, buildingSize)
				totalBuildings = totalBuildings + 1
				if(totalBuildings == maxBuildings) then 
					return 
				end
			end
		end
	end
end

function Dungeons_GenerateWorld(grid)
	grid:ResizeGrid(uvec2.new(64,64))
	local gridSize = grid:GetDimensions()

	-- fill world with empty tiles
	grid:Fill(uvec2.new(0,0), gridSize, 0, false)

	if(gridSize.x < 2 or gridSize.y < 2) then
		print('too small')
	end

	-- fill most of world with walkable floor tiles
	grid:Fill(uvec2.new(1,1), uvec2.new(gridSize.x - 2, gridSize.y - 2), 3, true)

	-- add buildings
	FillWithBuildings(grid)
end

-- main entry point, called from variable update
-- only needs to run once
function Dungeons_VariableUpdate(e)
	Arrrgh.SetGenerateWorldCb(Dungeons_GenerateWorld)	-- register generation cb
	local world = R3.ActiveWorld()
	world.GetComponent_LuaScript(e).m_isActive = false		-- we are done, stop running
end