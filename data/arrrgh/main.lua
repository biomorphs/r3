require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/camera'
require 'arrrgh/action_walkto'
require 'arrrgh/world_generator'
require 'arrrgh/monster_ai'

Arrrgh_Globals.GameState = nil
Arrrgh_Globals.ActionQueue = Fastqueue.new()

-- todo 
-- 
--	only draw enemies within player visibility(?)
--		fog of war or just current vision?
--		how did hunters do it? 
-- make tile generator rules data driven 
-- need ability to define new rules and sets of rules 
-- start making... a game?
-- actor interations
--	how to attach actions to objects?
--	through components?
--	many will be common
--		action(inspect) - output some text 
--		action(pickup) - add to inventory if possible
--		action(talk) - talk to something
-- UI (lots to do)
--  add IMGUI api for lua
--		keep it clean
--  implement basic UI for game in lua 
--		display player stats
--		debug ui 
--		action menu 
--			mouseover tile, menu pops up, enumerate actions from actors on tile
--	need to add textures to IM renderer
--	need script API for IM renderer 
--		one that isnt shit and slow 
--		might end up being mostly c++ anyway
--	need to add old font loader
--	add 2d pass to IM render
--  add nice draw text API 
-- basic ai
--	implement 2 dumb melee-only enemies
--		same behavior, different stats 
-- monster/player stats 
--	hp,energy,mana, whatever 
--	action points 
-- 

function Dungeons_SpawnPlayer()
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local spawnEntity = world:GetEntityByName('PlayerSpawnPoint')
	local spawnTransform = world.GetComponent_Transform(spawnEntity)
	local playerEntity = world:ImportScene('arrrgh/actors/player_actor.scn')
	local actualPos = spawnTransform:GetPosition()
	local vision = world.GetComponent_DungeonsVisionComponent(playerEntity[1])
	vision.m_needsUpdate = true
	actualPos.y = 0
	Arrrgh.MoveEntitiesWorldspace(playerEntity, actualPos)
	world:RemoveEntity(spawnEntity,false)
	local tilePos = Arrrgh.GetTileFromWorldspace(gridcmp, actualPos)
	Arrrgh.SetEntityTilePosition(gridcmp, playerEntity[1], tilePos.x, tilePos.y)

	Dungeons_CameraLookAt(actualPos, 40)
end

function Dungeons_SpawnMonster(gridcmp, spawnEntity, spawnerCmp)
	local world = R3.ActiveWorld()
	local spawnTransform = world.GetComponent_Transform(spawnEntity)	-- use the spawner world pos
	local batEntity = world:ImportScene('arrrgh/actors/bat.scn')
	Arrrgh.MoveEntitiesWorldspace(batEntity, spawnTransform:GetPosition())
	local vision = world.GetComponent_DungeonsVisionComponent(spawnEntity)
	if(vision ~= nil) then
		vision.m_needsUpdate = true
	end
	world:RemoveEntity(spawnEntity,false)
	Arrrgh.SetEntityTilePosition(gridcmp, batEntity[1], spawnerCmp.m_spawnPosition.x, spawnerCmp.m_spawnPosition.y)
end

function Dungeons_SpawnMonsters()
	print("Spawning monsters")
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local allSpawners = world:GetOwnersOfComponent1('Dungeons_MonsterSpawner')
	for monster=1,#allSpawners do 
		local spawnerCmp = world.GetComponent_Dungeons_MonsterSpawner(allSpawners[monster])
		if(spawnerCmp ~= nil) then 
			Dungeons_SpawnMonster(gridcmp, allSpawners[monster], spawnerCmp)
		else
			print('missing spawner component!')
		end
	end
end

function Dungeons_OnGameStart()
	print('Welcome to the Dungeons of Arrrgh!')
	Arrrgh_Globals.CurrentTurn = {ActionPointsSpent = 0, PlayerIndex = 0, EndTurnNow = false}
end

function Dungeons_OnTurnBegin()
	Arrrgh_Globals.CurrentTurn.ActionPointsSpent = 0
	Arrrgh_Globals.CurrentTurn.EndTurnNow = false
	if(Arrrgh_Globals.CurrentTurn.PlayerIndex == 0) then 
		-- focus on player on turn start
		local world = R3.ActiveWorld()
		local playerEntity = world:GetEntityByName('PlayerActor')
		local playerTransform = world.GetComponent_Transform(playerEntity)
		Dungeons_CameraLookAt(playerTransform:GetPosition())
	end
end

function Dungeons_OnTurnEnd()
	Arrrgh_Globals.CurrentTurn.PlayerIndex = Arrrgh_Globals.CurrentTurn.PlayerIndex + 1
	if(Arrrgh_Globals.CurrentTurn.PlayerIndex > 1) then 
		Arrrgh_Globals.CurrentTurn.PlayerIndex = 0
	end
end

function Dungeons_EndTurnNow()	-- signal that no more actions are going to be pushed
	Arrrgh_Globals.CurrentTurn.EndTurnNow = true
end

function Dungeons_IsTurnFinished()
	if(Dungeons_GetActionPointsRemaining() <= 0) then 
		return true 
	end
	return Arrrgh_Globals.CurrentTurn.EndTurnNow
end

function Dungeons_GetTotalActionPoints()
	if(Arrrgh_Globals.CurrentTurn.PlayerIndex == 0) then 
		return 1	-- todo, get based on player/monster attributes, etc
	else
		return 100	-- monster ai can do 1 action per monster per turn, up to this value
	end
end

function Dungeons_GetActionPointsRemaining()
	return Dungeons_GetTotalActionPoints(Arrrgh_Globals.CurrentTurn.PlayerIndex) - Arrrgh_Globals.CurrentTurn.ActionPointsSpent
end

function Dungeons_SpendActionPoint()
	Arrrgh_Globals.CurrentTurn.ActionPointsSpent = Arrrgh_Globals.CurrentTurn.ActionPointsSpent + 1
end

function Dungeons_GameTickFixed(e)
	-- state changes always happen in fixed update
	if(Arrrgh_Globals.GameState == nil) then 
		Arrrgh_Globals.GameState = 'start'
		Dungeons_OnGameStart()
	end
	if(Arrrgh_Globals.GameState == 'start') then 
		Dungeons_SpawnPlayer()
		Dungeons_SpawnMonsters()
		Arrrgh_Globals.GameState = 'startturn'
	end
	if(Arrrgh_Globals.GameState == 'startturn') then 
		Dungeons_OnTurnBegin()
		Arrrgh_Globals.GameState = 'doturn'
	end
	if(Arrrgh_Globals.GameState == 'doturn') then 
		if(Fastqueue.hasItems(Arrrgh_Globals.ActionQueue)) then		-- actions always run in fixed update
			local theAction = Arrrgh_Globals.ActionQueue[Arrrgh_Globals.ActionQueue.first]
			local result = theAction.onRun(theAction)
			if(result == 'complete') then
				Fastqueue.popleft(Arrrgh_Globals.ActionQueue)
			end
		elseif(Dungeons_IsTurnFinished()) then
			Dungeons_OnTurnEnd()
			Arrrgh_Globals.GameState = 'startturn'
		end
	end
end

-- player action ui happens in variable update
function Dungeons_ChoosePlayerAction()
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTransform = world.GetComponent_Transform(playerEntity)
	if(gridcmp ~= nil) then 
		local playerTile = Arrrgh.GetEntityTilePosition(playerEntity)
		local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
		if(playerTile ~= nil and mouseTile ~= nil and (playerTile.x ~= mouseTile.x or playerTile.y ~= mouseTile.y)) then
			if(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)) then
				local foundPath = gridcmp:CalculatePath(mouseTile, playerTile)
				if(#foundPath >= 2 and (#foundPath - 2) < Dungeons_GetActionPointsRemaining())  then
					Arrrgh.DebugDrawTiles(gridcmp, foundPath)
					if(R3.IsRightMouseButtonPressed()) then
						Dungeons_NewWalkAction(playerEntity, foundPath)
					end
				end
			end
		end
	end
end

function Dungeons_GameTickVariable(e)
	if(Arrrgh_Globals.GameState == 'doturn') then 
		if(Fastqueue.hasItems(Arrrgh_Globals.ActionQueue) == false) then	-- no actions are running
			if(Dungeons_IsTurnFinished() == false) then
				if(Arrrgh_Globals.CurrentTurn.PlayerIndex == 0) then 
					Dungeons_ChoosePlayerAction()
				else
					Dungeons_MonsterChooseActions()
				end
			end
		end
	end
end