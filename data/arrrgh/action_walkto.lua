function Dungeons_NewWalkAction(walkActor, pathToWalk)
	local newAction = {}
	newAction.name = "Walk"
	newAction.moveSpeedWorldspace = 8.0
	newAction.target = walkActor
	newAction.walkPath = pathToWalk
	newAction.currentTargetNode = 1
	newAction.onRun = Dungeons_ActionWalkTo
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionWalkTo(action)
	local world = R3.ActiveWorld()
	local actorTransform = world.GetComponent_Transform(action.target)
	local gridEntity = world:GetEntityByName('World Grid')
	local gridcmp = world.GetComponent_Dungeons_WorldGridComponent(gridEntity)
	if(actorTransform ~= nil and gridcmp ~= nil and #action.walkPath > 0 and action.currentTargetNode <= #action.walkPath) then 
		local actorPos = actorTransform:GetPosition()
		local targetTile = action.walkPath[action.currentTargetNode]
		local targetPos = vec3.new(targetTile.x * Arrrgh_Globals.TileDimensions.x, 0, targetTile.y * Arrrgh_Globals.TileDimensions.y)
		targetPos.x = targetPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		targetPos.z = targetPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		local targetDir = vec3.new(targetPos.x - actorPos.x, targetPos.y - actorPos.y, targetPos.z - actorPos.z)
		local targetLength = R3.Vec3Length(targetDir)
		if(targetLength < 0.1) then -- target reached
			Arrrgh.SetEntityTilePosition(gridcmp, action.target, targetTile.x, targetTile.y)
			local vision = world.GetComponent_DungeonsVisionComponent(action.target)
			if(action.currentTargetNode ~= 1 and vision ~= nil) then	-- update vision when tile changed (hacks), should happen somewhere central
				vision.m_needsUpdate = true
			end
			actorTransform:SetPosition(targetPos)
			if(action.currentTargetNode ~= 1) then
				Dungeons_SpendActionPoint()	-- each node after the first costs an action point
			end
			action.currentTargetNode = action.currentTargetNode + 1
			if(action.currentTargetNode > #action.walkPath) then -- final goal reached
				return 'complete'
			end
		else
			local actualSpeed = action.moveSpeedWorldspace
			local tDelta = R3.GetFixedUpdateDelta()
			targetDir.x = actualSpeed * (targetDir.x / targetLength)	
			targetDir.y = actualSpeed * (targetDir.y / targetLength) 
			targetDir.z = actualSpeed * (targetDir.z / targetLength) 
			actorPos.x = actorPos.x + targetDir.x * tDelta
			actorPos.y = actorPos.y + targetDir.y * tDelta
			actorPos.z = actorPos.z + targetDir.z * tDelta
			actorTransform:SetPosition(actorPos)
			Dungeons_CameraLookAt(actorPos)
			return 'continue'
		end
	end
	return 'error'
end

