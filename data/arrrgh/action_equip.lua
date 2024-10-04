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
	print('aaa')
	local world = R3.ActiveWorld()
	local ownerEquipment = world.GetComponent_Dungeons_EquippedItems(action.newOwner)
	print(ownerEquipment)
	local ownerInventory = world.GetComponent_Dungeons_Inventory(action.newOwner)
	print(ownerInventory)
	local targetItem = world.GetComponent_Dungeons_Item(action.itemToEquip)
	print(targetItem)
	local wearable = world.GetComponent_Dungeons_WearableItem(action.itemToEquip)
	print(wearable)
	if(wearable == nil) then 
		print("Item is not wearable")
		return
	end
	if(ownerEquipment ~= nil and ownerInventory ~= nil and targetItem ~= nil) then 
		local foundSlot = ownerEquipment.m_slots:find(wearable.m_slot)
		print(foundSlot)
		print(ownerEquipment.m_slots)
		-- if(foundSlot == ) then 
		-- 	print("No matching slot " .. wearable.m_slot)
		-- 	return
		-- end
	end
	return 'complete'
end