require'arrrgh/camera'

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

-- center camera in world 
function Generator_CenterCamera()
	return {
		Run = function(grid, context)			
			Dungeons_CameraActivate()
			local gridSize = grid:GetDimensions()
			Dungeons_CameraLookAt(vec3.new(gridSize.x * 2, 0, gridSize.y * 1.5), math.max(gridSize.x,gridSize.y) * 3)
			Dungeons_Generator.Yield(4.0)	-- wait for the camera to get into place
		end
	}
end

-- position/size must be uvec2
function Generator_SimpleRoom(position, size, wallTagStr, floorTagStr)
	return {
		Run = function(grid, context)
			local wallTags = TileTagset.new(wallTagStr)
			local floorTags = TileTagset.new(floorTagStr)
			grid:Fill(position, size, wallTags, false, true)	-- walls
			grid:Fill(uvec2.new(position.x + 1, position.y + 1), uvec2.new(size.x-2, size.y - 2), floorTags, true, false)	-- floor
			Dungeons_GeneratorContext.AddRoom(context, position, size, {})
		end
	}
end

-- position/size must be uvec2
function Generator_SetPlayerSpawn(tilePos)
	return {
		Run = function(grid, context)
			context.SpawnPoint = tilePos
		end
	}
end