function Dungeons_NewPickupItemAction(newOwnerActor, itemToPickup)
	local newAction = {}
	newAction.name = "Pick Up Item"
	newAction.newOwner = EntityHandle.new(newOwnerActor)
	newAction.itemToPickup = EntityHandle.new(itemToPickup)
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
			local meshComponent = world.GetComponent_DynamicMesh(allChildren[c])
			if(meshComponent ~= nil) then 
				meshComponent:SetShouldDraw(false)
			end
			Arrrgh.SetEntityTilePosition(gridcmp, allChildren[c], -1, -1)	-- remove children from grid
		end
		Arrrgh.SetEntityTilePosition(gridcmp, action.itemToPickup, -1, -1)	-- remove item from grid
		world:SetParent(action.itemToPickup, EntityHandle.new()) -- items in inventory have no parent (so they cannot become visible as part of actor)
		if(pickupItem.m_onPickupFn ~= "") then -- call onPickup for the item
			if(_G[pickupItem.m_onPickupFn] ~= nil) then		-- _G = assumes global scope
				_G[pickupItem.m_onPickupFn](action.newOwner, action.itemToPickup)
			end
		end
		R3.RebuildStaticScene()
	end
	return 'complete'
end