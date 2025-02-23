require 'arrrgh/fastqueue'
require 'arrrgh/arrrgh_shared'
require 'arrrgh/camera'
require 'arrrgh/action_walkto'
require 'arrrgh/action_inspect'
require 'arrrgh/action_melee_attack'
require 'arrrgh/action_pickup_item'
require 'arrrgh/action_consume'
require 'arrrgh/action_equip'
require 'arrrgh/world_generator'
require 'arrrgh/monster_spawning'
require 'arrrgh/item_spawning'
require 'arrrgh/prop_spawning'
require 'arrrgh/monster_ai'
require 'arrrgh/tile_debug_ui'
require 'arrrgh/monster_overlay'
require 'arrrgh/player_overlay'
require 'arrrgh/mouse_tile_state'
require 'arrrgh/inventory_screen'

Arrrgh_Globals.GameState = nil
Arrrgh_Globals.ActionQueue = Fastqueue.new()
Arrrgh_Globals.ShowActionsUi = nil	-- set to a uvec2 tile coord when open
Arrrgh_Globals.ShowInventory = false	-- if true, a state change will happen
Arrrgh_Globals.PlayerEntity = EntityHandle.new()

-- todo 
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

function Dungeons_CalculateMaxHP(entity, statsCmp)
	local allStats = Arrrgh.GetAllEquippedItemStats(entity)
	if(statsCmp ~= nil) then 
		local endurance = statsCmp.m_endurance + (allStats[Tag.new("Endurance")] or 0)
		return statsCmp.m_baseMaxHP + (2 * endurance)		-- 1 endurance = 2 max hp
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
	local equippedItemStats = Arrrgh.GetAllEquippedItemStats(attackerActor)
	local itemMeleeDamage = equippedItemStats[Tag.new("Melee Damage")] or 0
	local itemStrength = equippedItemStats[Tag.new("Strength")] or 0
	return 1 + attackerStats.m_strength + itemStrength + itemMeleeDamage
end

function Dungeons_DropItemsOnDeath(gridcmp, world, entity)
	-- monsters can drop loot 
	local tilePosCmp = world.GetComponent_Dungeons_WorldGridPosition(entity)
	local monsterCmp = world.GetComponent_Dungeons_Monster(entity)
	if(monsterCmp ~= nil and tilePosCmp ~= nil) then 
		local worldPos = vec3.new(tilePosCmp:GetPosition().x * Arrrgh_Globals.TileDimensions.x, 0, tilePosCmp:GetPosition().y * Arrrgh_Globals.TileDimensions.y)
		worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		worldPos.y = 0
		local itemName = Dungeons_SpawnItem(gridcmp, "", tilePosCmp:GetPosition(), worldPos, true)
		if(itemName ~= nil) then 
			print(world:GetEntityName(entity) .. " dropped " .. itemName .. " on death.")
		end
	end
end

function Dungeons_OnActorDeath(world, entity)
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	print(world:GetEntityName(entity), ' died!')
	
	if(R3.RandomFloat(0.0,1.0) < 0.5) then 
		Dungeons_DropItemsOnDeath(gridcmp, world, entity)
	end

	Arrrgh.SetEntityTilePosition(gridcmp, entity, -1, -1)	-- remove from grid
	local allChildren = world:GetAllChildren(entity)
	for c=1,#allChildren do 
		world:RemoveEntity(allChildren[c], false)
	end
	world:RemoveEntity(entity, false)
end

function Dungeons_HealActor(world, entity, hp)
	local targetStats = world.GetComponent_Dungeons_BaseActorStats(entity)
	if(targetStats ~= nil) then 
		local maxHP = Dungeons_CalculateMaxHP(entity, targetStats)
		if(targetStats.m_currentHP < maxHP) then
			local newHP = math.max(0, targetStats.m_currentHP + hp)
			print(world:GetEntityName(entity), ' was healed for  ', newHP - targetStats.m_currentHP, ' HP')
			targetStats.m_currentHP = newHP
		end
	else
		print('target does not have stats')
	end
end

-- calculate whether a melee attack would hit right now
function Dungeons_DidMeleeAttackHit(world, attacker, defender)
	local attackerStats = world.GetComponent_Dungeons_BaseActorStats(attacker)
	local defenderStats = world.GetComponent_Dungeons_BaseActorStats(defender)
	
	-- for now we only care about hit chance
	if(attackerStats ~= nil and defenderStats ~= nil) then 
		return (R3.RandomInt(0,100) < attackerStats.m_baseHitChance)
	else
		return false
	end
end

-- here we can scale damage taken based on defense, etc
function Dungeons_TakeDamage(world, entity, damageAmount)
	local targetStats = world.GetComponent_Dungeons_BaseActorStats(entity)
	if(targetStats ~= nil) then 
		local equippedItemStats = Arrrgh.GetAllEquippedItemStats(entity)
		local itemArmour = equippedItemStats[Tag.new("Armour")] or 0

		-- scale damage amount by armour value 
		damageAmount = math.max(0,damageAmount - itemArmour)

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
	local playerEntity = world:ImportScene('arrrgh/actors/player_actor.scn')
	local tilePos = Arrrgh_Globals.WorldGenerator.Context.SpawnPoint
	local worldPos = vec3.new(tilePos.x * Arrrgh_Globals.TileDimensions.x, 0, tilePos.y * Arrrgh_Globals.TileDimensions.y)
	worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
	worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
	worldPos.y = 0
	Arrrgh.MoveEntitiesWorldspace(playerEntity, worldPos)
	Arrrgh.SetEntityTilePosition(gridcmp, playerEntity[1], tilePos.x, tilePos.y)

	local vision = world.GetComponent_DungeonsVisionComponent(playerEntity[1])
	vision.m_needsUpdate = true
	
	world.AddComponent_Dungeons_BaseActorStats(playerEntity[1])
	local baseStats = world.GetComponent_Dungeons_BaseActorStats(playerEntity[1])
	baseStats.m_baseMaxHP = 10
	baseStats.m_strength = 0
	baseStats.m_endurance = 0
	baseStats.m_currentHP = Dungeons_CalculateMaxHP(playerEntity[1], baseStats)
	baseStats.m_baseHitChance = 75

	local equipment = world.GetComponent_Dungeons_EquippedItems(playerEntity[1])
	equipment:AddSlot(Tag.new("Weapon"))
	equipment:AddSlot(Tag.new("Offhand"))
	equipment:AddSlot(Tag.new("Helmet"))
	equipment:AddSlot(Tag.new("Body Armour"))
	equipment:AddSlot(Tag.new("Boots"))

	Dungeons_CameraLookAt(actualPos, 40)
end

function Dungeons_SpawnMonsters()
	print("Spawning monsters")
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)

	local monsters = Arrrgh_Globals.WorldGenerator.Context.MonsterSpawns
	local world = R3.ActiveWorld()
	for spawn=1,#monsters do 
		local spawnPos = monsters[spawn].Position
		local world = R3.ActiveWorld()
		local monsterType = monsters[spawn].TypeString
		local worldPos = vec3.new(spawnPos.x * Arrrgh_Globals.TileDimensions.x, 0, spawnPos.y * Arrrgh_Globals.TileDimensions.y)
		worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		worldPos.y = 0
		Dungeons_SpawnMonster(gridcmp, monsterType, spawnPos, worldPos)	-- Monster spawner handles actual event
	end
end

function Dungeons_SpawnItems()
	print("Spawning items")
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local items = Arrrgh_Globals.WorldGenerator.Context.Items
	for spawn=1,#items do 
		local tilePos = items[spawn].Position
		local itemName = items[spawn].Name
		local worldPos = vec3.new(tilePos.x * Arrrgh_Globals.TileDimensions.x, 0, tilePos.y * Arrrgh_Globals.TileDimensions.y)
		worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		worldPos.y = 0
		Dungeons_SpawnItem(gridcmp, itemName, tilePos, worldPos)	-- item spawner handles actual event
	end
end

function Dungeons_SpawnProps()
	print("Spawning props")
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local props = Arrrgh_Globals.WorldGenerator.Context.Props
	for prop=1,#props do 
		local tilePos = props[prop].Position
		local name = props[prop].Name
		local worldPos = vec3.new(tilePos.x * Arrrgh_Globals.TileDimensions.x, 0, tilePos.y * Arrrgh_Globals.TileDimensions.y)
		Dungeons_SpawnProp(gridcmp, name, tilePos, worldPos, props[prop].Rotation)
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
		local playerTransform = world.GetComponent_Transform(Arrrgh_Globals.PlayerEntity)
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
		local world = R3.ActiveWorld()
		local playerEntity = world:GetEntityByName('PlayerActor')
		if(world:IsHandleValid(playerEntity) == false) then		-- if player already exists, assume a script reload
			Dungeons_SpawnPlayer()
			Dungeons_SpawnMonsters()
			Dungeons_SpawnItems()
			Dungeons_SpawnProps()
		end
		Arrrgh_Globals.PlayerEntity = world:GetEntityByName('PlayerActor')
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
		elseif(Arrrgh_Globals.ShowInventory) then 
			Arrrgh_Globals.GameState = 'inventory'
		end
	end
	if(Arrrgh_Globals.GameState == 'inventory') then 
		if(Arrrgh_Globals.ShowInventory == false) then	-- inventory was closed, back to doturn
			Arrrgh_Globals.GameState = 'doturn'
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
			if(ImGui.Button("Inspect##" .. entity:GetID())) then 
				Dungeons_NewInspectAction(entity)
				return true
			end
			ImGui.SameLine()
		end
		local item = world.GetComponent_Dungeons_Item(entity)
		if(item ~= nil) then 
			if(ImGui.Button("Pick Up##" .. entity:GetID())) then 
				Dungeons_NewPickupItemAction(Arrrgh_Globals.PlayerEntity, entity)
				return true
			end
			ImGui.SameLine()
		end
		ImGui.NewLine()
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

function Dungeons_KeyboardChooseActions(world, gridcmp, playerEntity, playerTile)
	local targetTile = nil
	if(R3.IsKeyDown("KEY_w")) then 
		targetTile = uvec2.new(playerTile.x, playerTile.y + 1)
	elseif(R3.IsKeyDown("KEY_s")) then 
		targetTile = uvec2.new(playerTile.x, playerTile.y - 1)
	elseif(R3.IsKeyDown("KEY_d")) then 
		targetTile = uvec2.new(playerTile.x - 1, playerTile.y)
	elseif(R3.IsKeyDown("KEY_a")) then 
		targetTile = uvec2.new(playerTile.x + 1, playerTile.y)
	else
		return false
	end
	local meleeTarget = Dungeons_GetMeleeAttackTarget(world, gridcmp, playerEntity, playerTile, targetTile)
	if(meleeTarget ~= nil) then 
		Dungeons_NewMeleeAttackAction(playerEntity, meleeTarget)
		return true
	elseif(gridcmp:IsTilePassable(targetTile.x, targetTile.y)) then
		local foundPath = gridcmp:CalculatePath(playerTile, targetTile, false)	-- can we walk to this tile?
		if(#foundPath >= 2 and (#foundPath - 2) < Dungeons_GetActionPointsRemaining())  then
			Dungeons_NewWalkAction(playerEntity, foundPath)
			return true
		end
	end
	return false
end

-- player action ui happens in variable update
function Dungeons_ChoosePlayerAction()
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local playerTransform = world.GetComponent_Transform(Arrrgh_Globals.PlayerEntity)
	if(R3.WasKeyReleased('KEY_i')) then 
		Arrrgh_Globals.ShowInventory = true	-- handle input in variable update, state change happens during fixed
	end
	if(gridcmp ~= nil and playerTransform ~= nil) then 
		local playerTile = Arrrgh.GetEntityTilePosition(Arrrgh_Globals.PlayerEntity)
		local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
		Dungeons_UpdateMouseState(mouseTile)
		Dungeons_KeyboardChooseActions(world, gridcmp, Arrrgh_Globals.PlayerEntity, playerTile)
		if(playerTile ~= nil and mouseTile ~= nil) then
			Dungeons_ShowAvailableActions(world,gridcmp,Arrrgh_Globals.PlayerEntity, playerTile,mouseTile)
			if((playerTile.x ~= mouseTile.x or playerTile.y ~= mouseTile.y)) then
				local meleeTarget = Dungeons_GetMeleeAttackTarget(world, gridcmp, Arrrgh_Globals.PlayerEntity, playerTile, mouseTile)
				if(meleeTarget ~= nil) then 
					if(Dungeons_TileWasLeftClicked(mouseTile)) then
						Dungeons_NewMeleeAttackAction(Arrrgh_Globals.PlayerEntity, meleeTarget)
						Arrrgh_Globals.ShowActionsUi = nil
					end
				else
					if(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)) then
						local foundPath = gridcmp:CalculatePath(playerTile, mouseTile, false)	-- can we walk to this tile?
						if(#foundPath >= 2 and (#foundPath - 2) < Dungeons_GetActionPointsRemaining())  then
							Arrrgh.DebugDrawTiles(gridcmp, foundPath)
							if(Dungeons_TileWasLeftClicked(mouseTile)) then
								Dungeons_NewWalkAction(Arrrgh_Globals.PlayerEntity, foundPath)
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
	Dungeons_ShowPlayerOverlay()
	if(Arrrgh_Globals.GameState == 'inventory' and Arrrgh_Globals.ShowInventory) then 
		Arrrgh_Globals.ShowInventory = ShowPlayerInventory()
	end
end