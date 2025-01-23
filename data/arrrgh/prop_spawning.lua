require 'arrrgh/prop_torch'

-- name, scene path
Arrrgh_Globals.Dungeons_PropSpawnTable = {
	{ 'Torch', 'arrrgh/props/wall_torch.scn' },
	{ 'Bookshelves', 'arrrgh/props/book_shelves.scn' }
}

function Dungeons_FindSpecificProp(name)
	for i=1,#Arrrgh_Globals.Dungeons_PropSpawnTable do 
		if(Arrrgh_Globals.Dungeons_PropSpawnTable[i][1]==name) then 
			return i
		end
	end
	return nil
end

function Dungeons_SpawnProp(gridcmp, propName, tilePos, worldPos, rotation)
	local world = R3.ActiveWorld()
	local spawnIndex = Dungeons_FindSpecificProp(propName) 
	if(spawnIndex == nil) then 
		print("no spawn table entry for prop " .. propName)
		return 
	end
	local newEntities = world:ImportScene(Arrrgh_Globals.Dungeons_PropSpawnTable[spawnIndex][2])
	local rootEntity = Dungeons_GetFirstRootEntity(world, newEntities)
	if(rootEntity == nil) then 
		print("failed to find root entity from prop scene")
		return
	end
	Arrrgh.MoveEntitiesWorldspace(newEntities, worldPos)
	local rootTransform = world.GetComponent_Transform(rootEntity)
	if(rootTransform ~= nil) then 
		local orientation = quat.new(rootTransform:GetOrientation())
		orientation = R3.RotateQuat(orientation, rotation, vec3.new(0,1,0))
		rootTransform:SetOrientation(orientation)
	end
	-- rotate root entity 
	for e=1,#newEntities do 
		-- add every entity to the grid, even if they are not interactive
		Arrrgh.SetEntityTilePosition(gridcmp, newEntities[e], tilePos.x, tilePos.y)
		-- props spawn invisible
		local staticMesh = world.GetComponent_StaticMesh(newEntities[e])
		if(staticMesh ~= nil) then 
			staticMesh:SetShouldDraw(false)
		end
	end
end