
function Dungeons_ShowPlayerOverlay()
	local world = R3.ActiveWorld()
	local playerStats = world.GetComponent_Dungeons_BaseActorStats(Arrrgh_Globals.PlayerEntity)
	local playerEquipment = world.GetComponent_Dungeons_EquippedItems(Arrrgh_Globals.PlayerEntity)
	if(playerStats ~= nil and playerEquipment ~= nil) then 
		local keepOpen = true
		local allStats = Arrrgh.GetAllEquippedItemStats(Arrrgh_Globals.PlayerEntity)
		ImGui.Begin("Player Status", keepOpen)
		local maxHP = Dungeons_CalculateMaxHP(Arrrgh_Globals.PlayerEntity, playerStats)
		local currentHP = playerStats.m_currentHP
		local text = '' .. currentHP .. ' / ' .. maxHP
		ImGui.ProgressBar(text, currentHP, maxHP, vec2.new(128,24))
		ImGui.Text("Level: " .. playerStats.m_level)
		ImGui.Text("Strength: " .. playerStats.m_strength + (allStats[Tag.new("Strength")] or 0))
		ImGui.Text("Endurance: " .. playerStats.m_endurance + (allStats[Tag.new("Endurance")] or 0))
		ImGui.Text("Base Hit Chance: " .. playerStats.m_baseHitChance)
		ImGui.Separator()
		for slotName,item in pairs(playerEquipment.m_slots) do 
			ImGui.Text(slotName:GetString() .. ": " .. world:GetEntityName(item))
		end
		ImGui.Separator()
		for name, value in allStats:pairs() do 
			ImGui.Text(name:GetString() .. ": " .. value)
		end 
		ImGui.End()
	end
end