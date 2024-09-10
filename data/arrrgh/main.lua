require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'

Arrrgh_Globals.GameState = nil
Arrrgh_Globals.ActionQueue = Fastqueue.new()
Arrrgh_Globals.CameraHeight = 50
Arrrgh_Globals.CameraLookAt = vec3.new(0,0,0)
Arrrgh_Globals.CameraSpeed = vec3.new(128,128,128)

-- todo 
-- fog of war object 
	-- should update every time player/owner moves
	-- should update based on contents of vis component
--	any tiles that were ever visible are always visible
-- add some kind of component to identify world actors/objects/things 
--	it should contain the tile, and from there on, we should not be using world transform -> tile! 
--	tile component should be updated via grid 
--	and entity stored in the grid tile 
	-- minimap?!
--	only draw enemies within player visibility(?)
--		how did hunters do it? pretty sure they were visible or 'ghosts'
function Dungeons_PathfindTest(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local spawnEntity = world:GetEntityByName('PlayerSpawnPoint')
	local spawnTransform = world.GetComponent_Transform(spawnEntity)
	local myTransform = world.GetComponent_Transform(e)
	if(spawnTransform == nil or gridcmp == nil or myTransform == nil) then 
		return
	end
	local myTile = Arrrgh.GetTileFromWorldspace(gridcmp, myTransform:GetPosition())
	local spawnTile = Arrrgh.GetTileFromWorldspace(gridcmp, spawnTransform:GetPosition())
	if(myTile ~= nil and spawnTile ~= nil) then 
		local foundPath = gridcmp:CalculatePath(myTile, spawnTile)
		Arrrgh.DebugDrawTiles(gridcmp, foundPath)
	end
end

function Dungeons_PathfindToMouse(e)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local spawnEntity = world:GetEntityByName('PlayerSpawnPoint')
	local spawnTransform = world.GetComponent_Transform(spawnEntity)
	local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
	if(spawnTransform == nil or gridcmp == nil or mouseTile == nil) then 
		return
	end
	local spawnTile = Arrrgh.GetTileFromWorldspace(gridcmp, spawnTransform:GetPosition())
	if(mouseTile ~= nil and spawnTile ~= nil) then 
		local foundPath = gridcmp:CalculatePath(mouseTile, spawnTile)
		Arrrgh.DebugDrawTiles(gridcmp, foundPath)
	end
end

function Dungeons_SpawnPlayer()
	print('spawning player')
	local world = R3.ActiveWorld()
	local spawnEntity = world:GetEntityByName('PlayerSpawnPoint')
	local spawnTransform = world.GetComponent_Transform(spawnEntity)
	local playerEntity = world:ImportScene('arrrgh/actors/player_actor.scn')
	local actualPos = spawnTransform:GetPosition()
	local vision = world.GetComponent_DungeonsVisionComponent(playerEntity[1])
	vision.m_needsUpdate = true
	actualPos.y = 0
	Arrrgh.MoveEntitiesWorldspace(playerEntity, actualPos)
	world:RemoveEntity(spawnEntity,false)
	-- add the actor to the grid somehow 
	-- grid.addactor(playerEntity, playerTile)
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

function Dungeons_ActionWalkTo(action)
	local world = R3.ActiveWorld()
	local actorTransform = world.GetComponent_Transform(action.target)
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	if(actorTransform ~= nil and gridcmp ~= nil and #action.walkPath > 0 and action.currentTargetNode <= #action.walkPath) then 
		local actorPos = actorTransform:GetPosition()
		local targetTile = action.walkPath[action.currentTargetNode]
		local targetPos = vec3.new(targetTile.x * Arrrgh_Globals.TileDimensions.x, 0, targetTile.y * Arrrgh_Globals.TileDimensions.y)
		targetPos.x = targetPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		targetPos.z = targetPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		local targetDir = vec3.new(targetPos.x - actorPos.x, targetPos.y - actorPos.y, targetPos.z - actorPos.z)
		local targetLength = R3.Vec3Length(targetDir)
		if(targetLength < 0.1) then -- target reached
			-- grid.MoveActor(action.target, targetTile)	-- alert the grid that this entity changed tiles
			local vision = world.GetComponent_DungeonsVisionComponent(action.target)
			if(vision ~= nil) then
				vision.m_needsUpdate = true
			end
			actorTransform:SetPosition(targetPos)
			if(action.currentTargetNode ~= 1) then
				Dungeons_SpendActionPoint()	-- each node after the first costs an action point
			end
			action.currentTargetNode = action.currentTargetNode + 1
			if(action.currentTargetNode > #action.walkPath) then -- final goal reached
				return 'complete'
			end
		else
			local actualSpeed = action.moveSpeedWorldspace
			local tDelta = R3.GetVariableDelta()
			targetDir.x = actualSpeed * (targetDir.x / targetLength)	
			targetDir.y = actualSpeed * (targetDir.y / targetLength) 
			targetDir.z = actualSpeed * (targetDir.z / targetLength) 
			actorPos.x = actorPos.x + targetDir.x * tDelta
			actorPos.y = actorPos.y + targetDir.y * tDelta
			actorPos.z = actorPos.z + targetDir.z * tDelta
			actorTransform:SetPosition(actorPos)
			return 'continue'
		end
	end
	return 'error'
end

function Dungeons_NewWalkAction(walkActor, pathToWalk)
	local newAction = {}
	newAction.name = "Walk"
	newAction.moveSpeedWorldspace = 8.0
	newAction.target = walkActor	-- should be an entity
	newAction.walkPath = pathToWalk
	newAction.currentTargetNode = 1
	newAction.onRun = Dungeons_ActionWalkTo
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
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

function Dungeons_CameraLookAt(worldPos)
	Arrrgh_Globals.CameraLookAt = worldPos
end

function Dungeons_UpdateCamera()
	local camHeight = Arrrgh_Globals.CameraHeight
	local mouseScroll = R3.GetMouseWheelScroll()
	camHeight = camHeight + (mouseScroll * -4)
	if(camHeight < 20) then 
		camHeight = 20
	end
	if(camHeight > 200) then 
		camHeight = 200
	end
	Arrrgh_Globals.CameraHeight = camHeight

	-- move the camera towards the lookAt point  
	local world = R3.ActiveWorld()
	local cameraEntity = world:GetEntityByName('GameCamera')
	local cameraTransform = world.GetComponent_Transform(cameraEntity)
	local cameraPos = cameraTransform:GetPosition()
	local targetPosition = vec3.new(Arrrgh_Globals.CameraLookAt.x, Arrrgh_Globals.CameraLookAt.y + Arrrgh_Globals.CameraHeight, Arrrgh_Globals.CameraLookAt.z - 4.0)
	local camToTarget = vec3.new(targetPosition.x - cameraPos.x,targetPosition.y - cameraPos.y,targetPosition.z - cameraPos.z)
	local targetLength = R3.Vec3Length(camToTarget)
	local speedLimit = targetLength * 2	-- scale max speed by distance to goal
	local actualSpeed = vec3.new(math.min(Arrrgh_Globals.CameraSpeed.x, speedLimit),math.min(Arrrgh_Globals.CameraSpeed.y, speedLimit),math.min(Arrrgh_Globals.CameraSpeed.z, speedLimit))
	camToTarget.x = camToTarget.x / targetLength
	camToTarget.y = camToTarget.y / targetLength
	camToTarget.z = camToTarget.z / targetLength
	if(targetLength > 0.01) then
		cameraPos.x = cameraPos.x + (camToTarget.x * actualSpeed.x * R3.GetVariableDelta())
		cameraPos.y = cameraPos.y + (camToTarget.y * actualSpeed.y * R3.GetVariableDelta())
		cameraPos.z = cameraPos.z + (camToTarget.z * actualSpeed.z * R3.GetVariableDelta())
		cameraTransform:SetPosition(cameraPos)
	end
end

function Dungeons_GameTick(e)
	 -- keep looking at the player for now
	local world = R3.ActiveWorld()
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTransform = world.GetComponent_Transform(playerEntity)
	if(playerTransform ~= nil) then 
		Dungeons_CameraLookAt(playerTransform:GetPosition())
	end
	Dungeons_UpdateCamera()
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
		if(Fastqueue.hasItems(Arrrgh_Globals.ActionQueue)) then 
			local theAction = Arrrgh_Globals.ActionQueue[Arrrgh_Globals.ActionQueue.first]
			local result = theAction.onRun(theAction)
			if(result == 'complete') then
				Fastqueue.popleft(Arrrgh_Globals.ActionQueue)
			end
		elseif(Dungeons_GetActionPointsRemaining(0) > 0) then
			Dungeons_OnChooseActions(0)
		else
			Dungeons_OnTurnEnd()
			Arrrgh_Globals.GameState = 'startturn'
		end
	end
end