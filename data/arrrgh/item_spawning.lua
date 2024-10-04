-- name, scene path
Arrrgh_Globals.Dungeons_ItemSpawnTable = {
	{ 'Mystery Meat', 'arrrgh/items/mystery_meat.scn' },
	{ 'Suspicious Burger', 'arrrgh/items/burger.scn' },
	{ 'Dagger', 'arrrgh/items/dagger.scn' }
}

function Dungeons_FindSpecificItem(name)
	for i=1,#Arrrgh_Globals.Dungeons_ItemSpawnTable do 
		if(Arrrgh_Globals.Dungeons_ItemSpawnTable[i][1]==name) then 
			return i
		end
	end
	return nil
end

function Dungeons_SpawnItem(gridcmp, itemName, tilePos, worldPos)
	local world = R3.ActiveWorld()
	local spawnIndex = Dungeons_FindSpecificItem(itemName) 
	if(spawnIndex == nil) then 
		spawnIndex = math.random(1, #Arrrgh_Globals.Dungeons_ItemSpawnTable) 
	end
	local newEntities = world:ImportScene(Arrrgh_Globals.Dungeons_ItemSpawnTable[spawnIndex][2])
	local rootEntity = Dungeons_GetFirstRootEntity(world, newEntities)
	if(rootEntity == nil) then 
		print("failed to find root entity from item scene")
		return
	end
	Arrrgh.MoveEntitiesWorldspace(newEntities, worldPos)
	Arrrgh.SetEntityTilePosition(gridcmp, rootEntity, tilePos.x, tilePos.y)

	for e=1,#newEntities do 
		local staticMesh = world.GetComponent_StaticMesh(newEntities[e])
		if(staticMesh ~= nil) then 
			staticMesh.m_shouldDraw = false
		end
	end
end