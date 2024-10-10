function Dungeons_MonsterBaseStats(level, maxHP, strength, endurance, baseHitChance)
	return {
		Level = level,
		MaxHP = maxHP,
		Strength = strength or 0,
		Endurance = endurance or 0,
		BaseHitChance = baseHitChance or 50
	}
end

-- type string, scene path, base stats
Arrrgh_Globals.Dungeons_MonsterSpawnTable = {
	{ 'Bat', 'arrrgh/actors/bat.scn', Dungeons_MonsterBaseStats(1, 5, 0, 0, 60) },
	{ 'Zombie', 'arrrgh/actors/zombie.scn', Dungeons_MonsterBaseStats(1, 10, 1, 2, 30) }
}

function Dungeons_FindSpecificMonster(typeStr)
	for i=1,#Arrrgh_Globals.Dungeons_MonsterSpawnTable do 
		if(Arrrgh_Globals.Dungeons_MonsterSpawnTable[i][1]==typeStr) then 
			return i
		end
	end
	return nil
end

function Dungeons_GetFirstRootEntity(world, entityList)
	for e=1,#entityList do 
		local parent = world:GetParent(entityList[e])
		if(world:IsHandleValid(parent)==false) then 
			return entityList[e]
		end
	end
	return nil
end

function Dungeons_SpawnMonster(gridcmp, monsterType, tilePos, worldPos)
	local world = R3.ActiveWorld()
	local spawnIndex = Dungeons_FindSpecificMonster(monsterType) 
	if(spawnIndex == nil) then 
		spawnIndex = math.random(1, #Arrrgh_Globals.Dungeons_MonsterSpawnTable) 
	end
	local newEntities = world:ImportScene(Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][2])
	local rootEntity = Dungeons_GetFirstRootEntity(world, newEntities)
	if(rootEntity == nil) then 
		print("failed to find root entity from monster scene")
		return
	end
	Arrrgh.MoveEntitiesWorldspace(newEntities, worldPos)
	local vision = world.GetComponent_DungeonsVisionComponent(rootEntity)
	if(vision ~= nil) then
		vision.m_needsUpdate = true
	end
	world.AddComponent_Dungeons_BaseActorStats(rootEntity)
	local baseStats = world.GetComponent_Dungeons_BaseActorStats(rootEntity)
	baseStats.m_level = Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][3].Level
	baseStats.m_baseMaxHP = Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][3].MaxHP
	baseStats.m_strength = Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][3].Strength
	baseStats.m_endurance = Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][3].Endurance
	baseStats.m_currentHP = Dungeons_CalculateMaxHP(rootEntity, baseStats)
	baseStats.m_baseHitChance = Arrrgh_Globals.Dungeons_MonsterSpawnTable[spawnIndex][3].BaseHitChance
	Arrrgh.SetEntityTilePosition(gridcmp, rootEntity, tilePos.x, tilePos.y)

	for e=1,#newEntities do 
		local staticMesh = world.GetComponent_StaticMesh(newEntities[e])
		if(staticMesh ~= nil) then 
			staticMesh.m_shouldDraw = false
		end
	end
end