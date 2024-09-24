function Dungeons_NewPickupItemAction(newOwnerActor, itemToPickup)
	local newAction = {}
	newAction.name = "Pick Up Item"
	newAction.newOwner = newOwnerActor
	newAction.itemToPickup = itemToPickup
	newAction.onRun = Dungeons_ActionPickupItem
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionPickupItem(action)
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	local ownerInventory = world.GetComponent_Dungeons_Inventory(action.newOwner) 
	local pickupItem = world.GetComponent_Dungeons_Item(action.itemToPickup)
	if(ownerInventory ~= nil and pickupItem) then 
		print(world:GetEntityName(action.newOwner), ' picks up ', pickupItem.m_name)
		ownerInventory:AddItem(action.itemToPickup)
		local allChildren = world:GetAllChildren(action.itemToPickup)
		for c=1,#allChildren do 
			local meshComponent = world.GetComponent_StaticMesh(allChildren[c])
			if(meshComponent ~= nil) then 
				meshComponent.m_shouldDraw = false
			end
		end
		Arrrgh.SetEntityTilePosition(gridcmp, action.itemToPickup, -1, -1)	-- remove item from grid
	end
	return 'complete'
end