-- name, scene path, item stats list
Arrrgh_Globals.Dungeons_ItemSpawnTable = {
	{ 'Mystery Meat', 'arrrgh/items/mystery_meat.scn' },
	{ 'Suspicious Burger', 'arrrgh/items/burger.scn' },
	{ 'Rusty Dagger', 'arrrgh/items/rusty_dagger.scn', { { "Melee Damage", 1} } },
	{ 'Dagger', 'arrrgh/items/dagger.scn', { { "Melee Damage", 2} } },
	{ 'Sharp Dagger', 'arrrgh/items/dagger.scn', { { "Melee Damage", 3} } },
	{ 'Torn Leather Body Armour', 'arrrgh/items/body_armour_leather_torn.scn', { { "Armour", 1} } },
	{ 'Leather Body Armour', 'arrrgh/items/body_armour_leather.scn', { { "Armour", 2} } }
}

function Dungeons_FindSpecificItem(name)
	for i=1,#Arrrgh_Globals.Dungeons_ItemSpawnTable do 
		if(Arrrgh_Globals.Dungeons_ItemSpawnTable[i][1]==name) then 
			return i
		end
	end
	return nil
end

-- returns name of item spawned or nothing
function Dungeons_SpawnItem(gridcmp, itemName, tilePos, worldPos, spawnVisible)
	local world = R3.ActiveWorld()
	local spawnIndex = Dungeons_FindSpecificItem(itemName) 
	if(spawnIndex == nil) then 
		spawnIndex = R3.RandomInt(1, #Arrrgh_Globals.Dungeons_ItemSpawnTable) 
	end
	local newEntities = world:ImportScene(Arrrgh_Globals.Dungeons_ItemSpawnTable[spawnIndex][2])
	local rootEntity = Dungeons_GetFirstRootEntity(world, newEntities)
	if(rootEntity == nil) then 
		print("failed to find root entity from item scene")
		return
	end
	Arrrgh.MoveEntitiesWorldspace(newEntities, worldPos)
	Arrrgh.SetEntityTilePosition(gridcmp, rootEntity, tilePos.x, tilePos.y)

	local makeVisible = spawnVisible or false -- invisible by default
	for e=1,#newEntities do 
		local staticMesh = world.GetComponent_StaticMesh(newEntities[e])
		if(staticMesh ~= nil) then 
			staticMesh:SetShouldDraw(makeVisible)
		end
	end

	local item = world.GetComponent_Dungeons_Item(rootEntity)
	item.m_name = Arrrgh_Globals.Dungeons_ItemSpawnTable[spawnIndex][1]
	world:SetEntityName(rootEntity, Arrrgh_Globals.Dungeons_ItemSpawnTable[spawnIndex][1])

	-- apply stats
	local statsList = Arrrgh_Globals.Dungeons_ItemSpawnTable[spawnIndex][3]
	if(statsList ~= nil) then 
		world.AddComponent_Dungeons_ItemStats(rootEntity)
		local newStats = world.GetComponent_Dungeons_ItemStats(rootEntity)
		for stat=1,#statsList do 
			newStats.m_stats:add(DungeonsItemStat.new(statsList[stat][1], statsList[stat][2]))
		end
	end
	
	return item.m_name
end