Arrrgh_Globals.IsTileDebugOpen = true

function ShowTileDebugger(shouldShow) 
	Arrrgh_Globals.IsTileDebugOpen = shouldShow
end

-- call from variable update
function Dungeons_TileDebuggerUpdate()
	if(Arrrgh_Globals.IsTileDebugOpen == false) then 
		return 
	end
	local world = R3.ActiveWorld()
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	if(gridcmp == nil) then 
		return
	end
	local mouseTile = Arrrgh.GetTileUnderMouseCursor(gridcmp)
	if(mouseTile == nil) then 
		return
	end
	local tileEntities = gridcmp:GetEntitiesInTile(mouseTile.x, mouseTile.y)
	local keepOpen = true
	ImGui.Begin("Tile Debugger", keepOpen)
		gridcmp.m_debugDraw = ImGui.Checkbox("Debug Draw World", gridcmp.m_debugDraw)
		ImGui.Text("Coords: [" .. mouseTile.x .. ', ' .. mouseTile.y)
		ImGui.Text("Tags: " .. gridcmp:GetTileTagsAsString(mouseTile.x, mouseTile.y))
		ImGui.Text("Passable: " .. tostring(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)))
		ImGui.Text("Blocks Vision: " .. tostring(gridcmp:IsTilePassable(mouseTile.x, mouseTile.y)))
		ImGui.Text("Visual Entity: " .. world:GetEntityName(gridcmp:GetVisualEntity(mouseTile.x, mouseTile.y)))
		ImGui.Text('' .. #tileEntities .. ' entities in tile')
		for childIndex=1,#tileEntities do 
			ImGui.Text('\t' .. world:GetEntityName(tileEntities[childIndex]))
		end
		if(ImGui.Button("Hide")) then 
			Arrrgh_Globals.IsTileDebugOpen = false
		end
	ImGui.End()
end