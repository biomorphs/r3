
function ShowPlayerInventory()
	local world = R3.ActiveWorld()
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerInventory = world.GetComponent_Dungeons_Inventory(playerEntity)
	local showInventory = ImGui.Begin("Player Inventory", true)
	if(playerInventory ~= nil) then 
		for i=1,#playerInventory.m_allItems do 
			local itemCmp = world.GetComponent_Dungeons_Item(playerInventory.m_allItems[i])
			local inspectable = world.GetComponent_Dungeons_Inspectable(playerInventory.m_allItems[i])
			local consumable = world.GetComponent_Dungeons_ConsumableItem(playerInventory.m_allItems[i])
			if(itemCmp ~= nil) then 
				ImGui.Text(itemCmp.m_name)
				if(inspectable ~= nil) then 
					ImGui.SameLine()
					if(ImGui.Button("Inspect")) then 
						Dungeons_NewInspectAction(playerInventory.m_allItems[i])
						showInventory = false
					end
				end
				if(consumable ~= nil) then 
					local consumeText = "Eat"
					if(consumable:IsDrink()) then 
						consumeText = "Drink"
					end
					if(ImGui.Button(consumeText)) then
						Dungeons_NewConsumeAction(playerEntity, playerInventory.m_allItems[i])
						showInventory = false
					end
				end
			end
		end
	end
	ImGui.End()
	if(R3.WasKeyReleased('KEY_i') or R3.WasKeyReleased('KEY_ESCAPE')) then
		showInventory = false
	end
	return showInventory
end