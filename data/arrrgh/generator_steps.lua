require'arrrgh/camera'

-- doesnt actually spawn a monster, it creates a spawner for later
function Generator_SpawnMonster(monsterType, position)
	return {
		Run = function(grid, context)
			-- todo, check if tile already contains a spawner or the player?
			Dungeons_GeneratorContext.AddMonster(context, monsterType, position)
		end
	}
end

-- fill world with a tagset
function Generator_FillWorld(tagString, isPassable, blockVisibility)
	return {
		Run = function(grid, context)
			local tagsToSet = TileTagset.new(tagString)
			local gridSize = grid:GetDimensions()
			grid:Fill(uvec2.new(0,0), gridSize, tagsToSet, isPassable, blockVisibility)
		end
	}
end

-- fill empty tiles with a tagset
function Generator_FillEmptyTiles(tagString, isPassable, blockVisibility)
	return {
		Run = function(grid, context)
			local tagsToSet = TileTagset.new(tagString)
			local gridSize = grid:GetDimensions()
			for z=0,gridSize.y-1 do
				for x=0,gridSize.x-1 do 
					if(grid:TileHasTags(x,z) == false) then 
						grid:Fill(uvec2.new(x,z), uvec2.new(1,1), tagsToSet, isPassable, blockVisibility)
					end
				end
				Dungeons_Generator.Yield(0.01)
			end
		end
	}
end

function DistanceBetween(v0,v1)
	local tov1 = vec2.new(v0.x - v1.x, v0.y - v1.y)
	return math.sqrt((tov1.x * tov1.x) + (tov1.y * tov1.y))
end

function Generator_FillCircle(center, radius, tagString, isPassable, blocksVisibility)
	return {
		Run = function(grid,context)
			local tagsToSet = TileTagset.new(tagString)
			local areaCenter = vec2.new(center.x, center.y)
			local start = vec2.new(center.x - radius, center.y - radius)
			local size = vec2.new(radius * 2, radius * 2)
			for z=start.y,(start.y + size.y) do 
				for x=start.x,(start.x + size.x) do 
					local distanceToCenter = DistanceBetween(vec2.new(x,z), areaCenter)
					if(distanceToCenter <= radius) then
						grid:Fill(uvec2.new(math.tointeger(x),math.tointeger(z)), uvec2.new(1,1), tagsToSet, isPassable, blocksVisibility)
					end
				end
			end
		end
	}
end

-- center camera in world 
function Generator_CenterCamera()
	return {
		Run = function(grid, context)			
			Dungeons_CameraActivate()
			local gridSize = grid:GetDimensions()
			Dungeons_CameraLookAt(vec3.new(gridSize.x * 2, 0, gridSize.y * 1.25), math.max(gridSize.x,gridSize.y) * 3)
			Dungeons_Generator.Yield(3.0)	-- wait for the camera to get into place
		end
	}
end

function Generator_FindDoor(roomPos, roomSize)
	local doorPos = {}	-- make a door
	local wallForDoor = math.random(0, 3)
	if(wallForDoor == 0) then -- top
		doorPos = uvec2.new(math.random(roomPos.x + 1, roomPos.x + roomSize.x - 2), roomPos.y)
	elseif(wallForDoor == 1) then -- right
		doorPos = uvec2.new(roomPos.x + roomSize.x - 1, math.random(roomPos.y + 1, roomPos.y + roomSize.y - 2))
	elseif(wallForDoor == 2) then -- bottom
		doorPos = uvec2.new(math.random(roomPos.x + 1, roomPos.x + roomSize.x - 2), roomPos.y + roomSize.y - 1)
	elseif(wallForDoor == 3) then -- left
		doorPos = uvec2.new(roomPos.x, math.random(roomPos.y + 1, roomPos.y + roomSize.y - 2))
	end
	return doorPos
end

-- position/size must be uvec2
function Generator_SimpleRoom(position, size, wallTagStr, floorTagStr)
	return {
		Run = function(grid, context)
			local wallTags = TileTagset.new(wallTagStr)
			local floorTags = TileTagset.new(floorTagStr)
			grid:Fill(position, size, wallTags, false, true)	-- walls
			grid:Fill(uvec2.new(position.x + 1, position.y + 1), uvec2.new(size.x-2, size.y - 2), floorTags, true, false)	-- floor
			-- todo, check if a path to spawn pos is possible
			local doorPos = Generator_FindDoor(position, size)
			if(doorPos ~= nil) then
				grid:Fill(doorPos, uvec2.new(1, 1), floorTags, true, false)
			end
			Dungeons_GeneratorContext.AddRoom(context, position, size, {doorPos})
		end
	}
end

function Generator_PathFromRoomToRoom(floorTagStr, pathChance)	-- chance = 0 to 1
	return {
		Run = function(grid, context)
			local roomCount = #context.Rooms
			local floorTags = TileTagset.new(floorTagStr)
			for room=1,roomCount do 
				local entranceCount = #context.Rooms[room].Entrances
				for entrance=1,entranceCount do 
					for otherRoom=1,roomCount do 
						if(otherRoom ~= room) then
							for otherEntrance=1,#context.Rooms[otherRoom].Entrances do
								if(math.random() < pathChance) then
									local fromPos = context.Rooms[room].Entrances[entrance]
									local toPos = context.Rooms[otherRoom].Entrances[otherEntrance]
									local foundPath = grid:CalculatePath(fromPos, toPos, false)
									for pathNode=1,#foundPath do
										grid:Fill(foundPath[pathNode], uvec2.new(1, 1), floorTags, true, false)
									end
								end
								Dungeons_Generator.Yield(0.0)
							end
						end
					end
				end
			end
		end
	}
end

function Generator_SpawnPlayerInFirstRoom()
	return {
		Run = function(grid, context)
			if(#context.Rooms > 0) then 
				local spawnPos = uvec2.new(context.Rooms[1].Position.x + 1, context.Rooms[1].Position.y + 1)
				context.SpawnPoint = spawnPos
			end
		end
	}
end

function Generator_SetPlayerSpawn(tilePos)
	return {
		Run = function(grid, context)
			context.SpawnPoint = tilePos
		end
	}
end