require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/camera'
require 'arrrgh/action_walkto'
require 'arrrgh/action_inspect'
require 'arrrgh/action_melee_attack'
require 'arrrgh/world_generator'
require 'arrrgh/monster_spawning'
require 'arrrgh/monster_ai'
require 'arrrgh/tile_debug_ui'
require 'arrrgh/monster_overlay'
require 'arrrgh/mouse_tile_state'

Arrrgh_Globals.GameState = nil
Arrrgh_Globals.ActionQueue = Fastqueue.new()
Arrrgh_Globals.ShowActionsUi = nil	-- set to a uvec2 tile coord when open

-- todo 
--	only draw enemies within player visibility(?)
--		fog of war or just current vision?
--		how did hunters do it? 
-- make tile generator rules data driven 
-- need ability to define new rules and sets of rules 
-- UI (lots to do)
--  implement basic UI for game in lua 
--		display player stats
--	need to add textures to IM renderer
--	need script API for IM renderer 
--		one that isnt shit and slow 
--		might end up being mostly c++ anyway
--	need to add old font loader
--	add 2d pass to IM render
--  add nice draw text API 
-- inventories/items

function Dungeons_CalculateMaxHP(baseStatsCmp)
	if(baseStatsCmp ~= nil) then 
		return baseStatsCmp.m_baseMaxHP + (2 * baseStatsCmp.m_endurance)		-- 1 endurance = 2 max hp
	else 
		return 1
	end
end

-- note we separate the calculation of the base damage from the application of it
function Dungeons_CalculateMeleeDamageDealt(world, attackerActor)
	local attackerStats = world.GetComponent_Dungeons_BaseActorStats(attackerActor)
	if(attackerStats == nil) then
		print('missing stats')
		return 0
	end
	return 1 + attackerStats.m_strength
end

function Dungeons_OnActorDeath(world, entity)
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	print(world:GetEntityName(entity), ' died!')
	-- drop loot here!
	Arrrgh.SetEntityTilePosition(gridcmp, entity, -1, -1)	-- remove from grid
	world:RemoveEntity(entity, false)
end

-- here we can scale damage taken based on defense, etc
function Dungeons_TakeDamage(world, entity, damageAmount)
	local targetStats = world.GetComponent_Dungeons_BaseActorStats(entity)
	if(targetStats ~= nil) then 
		print(targetStats)
		print(targetStats.m_currentHP)
		print(world:GetEntityName(entity), ' takes ', damageAmount, ' damage')
		targetStats.m_currentHP = math.max(0, targetStats.m_currentHP - damageAmount)
		if(targetStats.m_currentHP == 0) then 
			Dungeons_OnActorDeath(world, entity)
		end
	else
		print('target does not have stats')
	end
end

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

	world.AddComponent_Dungeons_BaseActorStats(playerEntity[1])
	local baseStats = world.GetComponent_Dungeons_BaseActorStats(playerEntity[1])
	baseStats.m_baseMaxHP = 10
	baseStats.m_strength = 0
	baseStats.m_endurance = 0
	baseStats.m_currentHP = Dungeons_CalculateMaxHP(baseStats)

	Dungeons_CameraLookAt(actualPos, 40)
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
		if(playerTransform ~= nil) then
			Dungeons_CameraLookAt(playerTransform:GetPosition())
		end
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

function Dungeons_InTouchingDistance(playerTile, targetTile)
	local tileDistance = math.abs(playerTile.x - targetTile.x) + math.abs(playerTile.y - targetTile.y)
	return tileDistance <= 1
end

-- returns true if an action was picked
function Dungeons_ShowAvailableEntityActions(world, entity, playerTile, targetTile)
	if(Dungeons_InTouchingDistance(playerTile, targetTile)) then 
		local inspectable = world.GetComponent_Dungeons_Inspectable(entity)
		if(inspectable ~= nil) then 
			if(ImGui.Button("Inspect")) then 
				Dungeons_NewInspectAction(entity)
				return true
			end
		end
	end
	return false
end

function Dungeons_ShowAvailableActions(world, grid, playerEntity, playerTile, targetTile)
	if(Dungeons_TileWasRightClicked(targetTile)) then 
		Arrrgh_Globals.ShowActionsUi = uvec2.new(targetTile.x, targetTile.y)
	end

	-- are there any entities on this tile that support actions?
	local keepOpen = true
	local actionChosen = false
	if(Arrrgh_Globals.ShowActionsUi ~= nil) then 
		ImGui.PushLargeFont()
		keepOpen = ImGui.Begin("Choose an action", keepOpen)
		local tileActors = grid:GetEntitiesInTile(Arrrgh_Globals.ShowActionsUi.x, Arrrgh_Globals.ShowActionsUi.y)
		for actor=1,#tileActors do 
			ImGui.Text(world:GetEntityName(tileActors[actor]))
			if(Dungeons_ShowAvailableEntityActions(world, tileActors[actor], playerTile, Arrrgh_Globals.ShowActionsUi)) then 
				actionChosen = true
			end
		end
		ImGui.End()
		ImGui.PopFont()
	end

	if(keepOpen == false or actionChosen) then 
		Arrrgh_Globals.ShowActionsUi = nil
	end
end

-- returns an entity handle to an actor that can be melee'd this turn
function Dungeons_GetMeleeAttackTarget(world, gridcmp, playerEntity, playerTile, targetTile)
	local distance = math.abs(playerTile.x - targetTile.x) + math.abs(playerTile.y - targetTile.y)
	if(distance <= 1) then 
		local tileEntities = gridcmp:GetEntitiesInTile(targetTile.x, targetTile.y)
		for tIndex=1,#tileEntities do 
			local monsterCmp = world.GetComponent_Dungeons_Monster(tileEntities[tIndex])
			if(monsterCmp ~= nil) then 
				return tileEntities[tIndex]
			end
		end
	end
	return nil
end

-- player action ui happens in variable update
function Dungeons_ChoosePlayerAction()
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerTransform = world.GetComponent_Transform(playerEntity)
	if(gridcmp ~= nil and playerTransform ~= nil) then 
		local playerTile = Arrrgh.GetEntityTilePosition(playerEntity)
		local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
		Dungeons_UpdateMouseState(mouseTile)
		if(playerTile ~= nil and mouseTile ~= nil) then
			Dungeons_ShowAvailableActions(world,gridcmp,playerEntity, playerTile,mouseTile)
			if((playerTile.x ~= mouseTile.x or playerTile.y ~= mouseTile.y)) then
				local meleeTarget = Dungeons_GetMeleeAttackTarget(world, gridcmp, playerEntity, playerTile, mouseTile)
				if(meleeTarget ~= nil) then 
					if(Dungeons_TileWasLeftClicked(mouseTile)) then
						Dungeons_NewMeleeAttackAction(playerEntity, meleeTarget)
						Arrrgh_Globals.ShowActionsUi = nil
					end
				else
					if(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)) then
						local foundPath = gridcmp:CalculatePath(playerTile, mouseTile, false)	-- can we walk to this tile?
						if(#foundPath >= 2 and (#foundPath - 2) < Dungeons_GetActionPointsRemaining())  then
							Arrrgh.DebugDrawTiles(gridcmp, foundPath)
							if(Dungeons_TileWasLeftClicked(mouseTile)) then
								Dungeons_NewWalkAction(playerEntity, foundPath)
								Arrrgh_Globals.ShowActionsUi = nil
							end
						end
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
	Dungeons_TileDebuggerUpdate()
	Dungeons_ShowMonsterOverlay()
end