function Dungeons_NewConsumeAction(consumer, itemToConsume)
	local newAction = {}
	newAction.name = "Consume Item"
	newAction.consumer = consumer
	newAction.consumable = itemToConsume
	newAction.onRun = Dungeons_ActionConsume
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionConsume(action)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local consumable = world.GetComponent_Dungeons_ConsumableItem(action.consumable)
	local consumerInventory = world.GetComponent_Dungeons_Inventory(action.consumer)
	local consumerStats = world.GetComponent_Dungeons_BaseActorStats(action.consumer)
	if(consumable ~= nil and consumerStats) then 
		print(world:GetEntityName(action.consumer), ' consumes ', world:GetEntityName(action.consumable))
		if(consumable.m_hpOnUse > 0) then 
			Dungeons_HealActor(world, action.consumer, consumable.m_hpOnUse)
		elseif(consumable.m_hpOnUse < 0) then
			Dungeons_TakeDamage(world, action.consumer, math.abs(consumable.m_hpOnUse))
		end
		Arrrgh.SetEntityTilePosition(gridcmp, action.consumable, -1, -1)	-- remove item from grid if it was still on there
		consumerInventory:RemoveItem(action.consumable)
		world:RemoveEntity(action.consumable, false)
	end
	return 'complete'
end