Arrrgh_Globals.MonsterAICoroutine = nil

-- yields until time passed
-- keeping separate functions for debugging
function Dungeons_MonsterAIYield(timeToWait)
	timeToWait = timeToWait or 0.1
	local currentWaitTime = 0 
	repeat
		currentWaitTime = currentWaitTime + R3.GetVariableDelta() -- variable delta is good enough
		coroutine.yield()
	until(currentWaitTime >= timeToWait)
end

-- runs as coroutine
function Dungeons_MonsterAIDoTurn()
	print('monsters are thinking...')
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTile = Arrrgh.GetEntityTilePosition(playerEntity)
	local allMonsters = world:GetOwnersOfComponent1('Dungeons_Monster')
	for monster=1,#allMonsters do 
		-- first update vision of each monster in case something changed in the world
		local monsterCmp = World.GetComponent_Dungeons_Monster(allMonsters[monster])
		local visCmp = world.GetComponent_DungeonsVisionComponent(allMonsters[monster])
		if(visCmp ~= nil) then 
			visCmp.m_needsUpdate = true
			Dungeons_MonsterAIYield(0)
			local seesPlayer = false
			for index,tilePos in pairs(visCmp.m_visibleTiles) do
				if(tilePos.x == playerTile.x and tilePos.y == playerTile.y) then 
					seesPlayer = true
				end
			end
			if(seesPlayer) then 
				print(monsterCmp.m_name, ' can see you!')
				local monsterTile = Arrrgh.GetEntityTilePosition(allMonsters[monster])
				local foundPath = gridcmp:CalculatePath(playerTile,monsterTile)
				if(#foundPath >= 2)  then
					Dungeons_NewWalkAction(allMonsters[monster], foundPath, 2)	
				end
			end
		end
		Dungeons_MonsterAIYield(0)
	end
end

-- keep calling this until end turn condition is hit
function Dungeons_MonsterChooseActions()
	if(Arrrgh_Globals.MonsterAICoroutine == nil) then 
		Arrrgh_Globals.MonsterAICoroutine = coroutine.create( Dungeons_MonsterAIDoTurn )
	end
	local runningStatus = coroutine.status(Arrrgh_Globals.MonsterAICoroutine)
	if(runningStatus ~= 'dead') then
		coroutine.resume(Arrrgh_Globals.MonsterAICoroutine)
	else
		Dungeons_EndTurnNow()
		Arrrgh_Globals.MonsterAICoroutine = nil
	end
end