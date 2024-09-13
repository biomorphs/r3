require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/camera'
require 'arrrgh/action_walkto'

Arrrgh_Globals.GameState = nil
Arrrgh_Globals.ActionQueue = Fastqueue.new()

-- todo 
--	only draw enemies within player visibility(?)
--		how did hunters do it? pretty sure they were visible or 'ghosts'

function Dungeons_SpawnPlayer()
	print('spawning player')
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
end

function Dungeons_OnTurnBegin()
	print('turn begin')
	Arrrgh_Globals.CurrentTurn = {ActionPointsSpent = 0, PlayerIndex = 0}
end

function Dungeons_OnTurnEnd()
	print('turn end')
end

function Dungeons_GetTotalActionPoints(playerIndex)
	return 10	-- todo, get based on player/monster attributes, etc
end

function Dungeons_GetActionPointsRemaining(playerIndex)
	return Dungeons_GetTotalActionPoints(playerIndex) - Arrrgh_Globals.CurrentTurn.ActionPointsSpent
end

function Dungeons_SpendActionPoint()
	print('spent an action point')
	Arrrgh_Globals.CurrentTurn.ActionPointsSpent = Arrrgh_Globals.CurrentTurn.ActionPointsSpent + 1
	print('AP remaining', Dungeons_GetActionPointsRemaining(0))
end

function Dungeons_OnChooseActions(playerIndex)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTransform = world.GetComponent_Transform(playerEntity)
	if(gridcmp ~= nil) then 
		local playerTile = Arrrgh.GetTileFromWorldspace(gridcmp, playerTransform:GetPosition())
		local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
		if(playerTile ~= nil and mouseTile ~= nil and (playerTile.x ~= mouseTile.x or playerTile.y ~= mouseTile.y)) then
			if(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)) then
				local foundPath = gridcmp:CalculatePath(mouseTile, playerTile)
				if(#foundPath >= 2 and (#foundPath - 2) < Dungeons_GetActionPointsRemaining(playerIndex))  then
					Arrrgh.DebugDrawTiles(gridcmp, foundPath)
					if(R3.IsRightMouseButtonPressed()) then
						Dungeons_NewWalkAction(playerEntity, foundPath)
					end
				end
			end
		end
	end
end

function Dungeons_GameTickFixed(e)
	 -- keep camera looking at the player for now
	local world = R3.ActiveWorld()
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTransform = world.GetComponent_Transform(playerEntity)
	if(playerTransform ~= nil) then 
		Dungeons_CameraLookAt(playerTransform:GetPosition())
	end
	Dungeons_UpdateCamera()

	-- state changes always happen in fixed update
	if(Arrrgh_Globals.GameState == nil) then 
		Arrrgh_Globals.GameState = 'start'
	end
	if(Arrrgh_Globals.GameState == 'start') then 
		Dungeons_SpawnPlayer()
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
		elseif(Dungeons_GetActionPointsRemaining(0) <= 0) then
			Dungeons_OnTurnEnd()
			Arrrgh_Globals.GameState = 'startturn'
		end
	end
end

function Dungeons_GameTickVariable(e)
	if(Arrrgh_Globals.GameState == 'doturn') then 
		if(Fastqueue.hasItems(Arrrgh_Globals.ActionQueue) == false) then	-- no actions to perform
			if(Dungeons_GetActionPointsRemaining(0) > 0) then
				Dungeons_OnChooseActions(0)
			end
		end
	end
end