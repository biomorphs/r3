-- track which tiles are clicked
Arrrgh_Globals.MouseState = { LeftMouseDownOnTile = nil, RightMouseDownOnTile = nil, LeftClickedTile = nil, RightClickedTile = nil } 

-- annoyingly we need to properly track when tiles are clicked manually
-- only use TileWasLeft/RightClicked
function Dungeons_UpdateMouseState(mouseTile)
	Arrrgh_Globals.MouseState.LeftClickedTile = nil			-- recalculate click state each frame
	Arrrgh_Globals.MouseState.RightClickedTile = nil		-- recalculate click state each frame

	if(R3.IsLeftMouseButtonPressed()) then
		if(Arrrgh_Globals.MouseState.LeftMouseDownOnTile == nil) then
			Arrrgh_Globals.MouseState.LeftMouseDownOnTile = mouseTile
		end
	else
		if(mouseTile ~= nil and Arrrgh_Globals.MouseState.LeftMouseDownOnTile ~= nil) then
			if(mouseTile.x == Arrrgh_Globals.MouseState.LeftMouseDownOnTile.x and mouseTile.y == Arrrgh_Globals.MouseState.LeftMouseDownOnTile.y) then 
				Arrrgh_Globals.MouseState.LeftClickedTile = mouseTile -- pressed and released on same tile
			end
		end
		Arrrgh_Globals.MouseState.LeftMouseDownOnTile = nil;
	end

	if(R3.IsRightMouseButtonPressed()) then
		if(Arrrgh_Globals.MouseState.RightMouseDownOnTile == nil) then
			Arrrgh_Globals.MouseState.RightMouseDownOnTile = mouseTile
		end
	else
		if(mouseTile ~= nil and Arrrgh_Globals.MouseState.RightMouseDownOnTile ~= nil) then
			if(mouseTile.x == Arrrgh_Globals.MouseState.RightMouseDownOnTile.x and mouseTile.y == Arrrgh_Globals.MouseState.RightMouseDownOnTile.y) then 
				Arrrgh_Globals.MouseState.RightClickedTile = mouseTile -- pressed and released on same tile
			end
		end
		Arrrgh_Globals.MouseState.RightMouseDownOnTile = nil;
	end
end

function Dungeons_TileWasLeftClicked(tile)
	if(Arrrgh_Globals.MouseState.LeftClickedTile ~= nil and tile ~= nil) then
		return Arrrgh_Globals.MouseState.LeftClickedTile.x == tile.x and Arrrgh_Globals.MouseState.LeftClickedTile.y == tile.y
	else
		return false
	end
end

function Dungeons_TileWasRightClicked(tile)
	if(Arrrgh_Globals.MouseState.RightClickedTile ~= nil and tile ~= nil) then
		return Arrrgh_Globals.MouseState.RightClickedTile.x == tile.x and Arrrgh_Globals.MouseState.RightClickedTile.y == tile.y
	else
		return false
	end
end