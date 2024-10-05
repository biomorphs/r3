-- if the item is in the owner's inventory it will be moved
function Dungeons_NewEquipAction(newOwner, item)
	local newAction = {}
	newAction.name = "Equip Item"
	newAction.newOwner = EntityHandle.new(newOwner)
	newAction.itemToEquip = EntityHandle.new(item)
	newAction.onRun = Dungeons_ActionEquipItem
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionEquipItem(action)
	local world = R3.ActiveWorld()
	local ownerEquipment = world.GetComponent_Dungeons_EquippedItems(action.newOwner)
	local ownerInventory = world.GetComponent_Dungeons_Inventory(action.newOwner)
	local targetItem = world.GetComponent_Dungeons_Item(action.itemToEquip)
	local wearable = world.GetComponent_Dungeons_WearableItem(action.itemToEquip)
	if(wearable == nil) then 
		print("Item is not wearable")
		return 'complete'
	end
	if(targetItem == nil) then 
		print("Target is not an item")
		return 'complete'
	end
	if(ownerEquipment == nil or ownerInventory == nil) then 
		print("Actor has no inventory or equipment")
		return 'complete'
	end
	local targetEquipmentSlot = ownerEquipment.m_slots:find(wearable.m_slot)
	if(targetEquipmentSlot == nil) then 
		print(world:GetEntityName(action.newOwner) .. " has no equipment slot for " .. world:GetEntityName(action.itemToEquip))
		return 'complete'
	end
	if(world:IsHandleValid(targetEquipmentSlot)) then -- already has something equipped in this slot, transfer to inventory
		if(ownerInventory:AddItem(targetEquipmentSlot) == false) then
			print("Cannot transfer " .. world:GetEntityName(action.itemToEquip) .. " to inventory")
			return 'complete'
		end
		local targetSlotWearable = world.GetComponent_Dungeons_WearableItem(targetEquipmentSlot)
		if(targetSlotWearable ~= nil and targetSlotWearable.m_onRemoveFn ~= "") then	-- call onRemove
			if(_G[targetSlotWearable.m_onRemoveFn] ~= nil) then 
				_G[targetSlotWearable.m_onRemoveFn](action.newOwner, targetSlotWearable)
			end
		end
	end
	if(ownerInventory:RemoveItem(action.itemToEquip) == false) then -- only allow equip directly from inventory (may change this later)
		print("Cannot remove " .. world:GetEntityName(action.itemToEquip) .. " from inventory")
		return 'complete'
	end
	ownerEquipment.m_slots[wearable.m_slot] = action.itemToEquip
	print(world:GetEntityName(action.newOwner) .. " equipped " .. world:GetEntityName(action.itemToEquip))
	if(wearable.m_onEquipFn ~= "") then	-- call onRemove
		if(_G[wearable.m_onEquipFn] ~= nil) then 
			_G[wearable.m_onEquipFn](action.newOwner, action.itemToEquip)
		end
	end
	return 'complete'
end