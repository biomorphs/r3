
function Dungeons_ShowPlayerOverlay()
	local world = R3.ActiveWorld()
	local playerEntity = world:GetEntityByName('PlayerActor')
	local playerStats = world.GetComponent_Dungeons_BaseActorStats(playerEntity)
	local playerEquipment = world.GetComponent_Dungeons_EquippedItems(playerEntity)
	if(playerStats ~= nil and playerEquipment ~= nil) then 
		local keepOpen = true
		ImGui.Begin("Player Status", keepOpen)
		local maxHP = Dungeons_CalculateMaxHP(playerStats)
		local currentHP = playerStats.m_currentHP
		local text = '' .. currentHP .. ' / ' .. maxHP
		ImGui.ProgressBar(text, currentHP, maxHP, vec2.new(128,24))
		ImGui.Text("Level: " .. playerStats.m_level)
		ImGui.Text("Strength: " .. playerStats.m_strength)
		ImGui.Text("Endurance: " .. playerStats.m_endurance)
		ImGui.Text("Base Hit Chance: " .. playerStats.m_baseHitChance)
		ImGui.Separator()
		for slotName,item in pairs(playerEquipment.m_slots) do 
			ImGui.Text(slotName:GetString() .. ": " .. world:GetEntityName(item))
		end
		ImGui.End()
	end
end