
-- shows the hp overlay at the top of the screen
function Dungeons_ShowMonsterOverlay()
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	if(gridcmp ~= nil) then 
		local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
		if(mouseTile ~= nil) then
			local tileEntities = gridcmp:GetEntitiesInTile(mouseTile.x, mouseTile.y)
			for tIndex=1,#tileEntities do 
				local monsterCmp = world.GetComponent_Dungeons_Monster(tileEntities[tIndex])
				local statsCmp = world.GetComponent_Dungeons_BaseActorStats(tileEntities[tIndex])
				if(monsterCmp ~= nil and statsCmp ~= nil) then
					local maxHP = Dungeons_CalculateMaxHP(statsCmp)
					local currentHP = statsCmp.m_currentHP
					local text = '' .. currentHP .. ' / ' .. maxHP
					local keepOpen = true
					ImGui.Begin(monsterCmp.m_name, keepOpen)
						ImGui.ProgressBar(text, currentHP, maxHP, vec2.new(128,24))
					ImGui.End()
				end
			end
		end
	end
end