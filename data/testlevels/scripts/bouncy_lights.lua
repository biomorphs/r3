local BouncyLights = {}

function BouncyLights_PopulateInputs(scriptCmp)
	scriptCmp.m_inputParams:AddInt("Light Count", 32)
	scriptCmp.m_inputParams:AddFloat("X Min", -16.0)
	scriptCmp.m_inputParams:AddFloat("X Max", 16.0)
	scriptCmp.m_inputParams:AddFloat("Y Min", 0.0)
	scriptCmp.m_inputParams:AddFloat("Y Max", 16.0)
	scriptCmp.m_inputParams:AddFloat("Z Min", -16.0)
	scriptCmp.m_inputParams:AddFloat("Z Max", 16.0)
	scriptCmp.m_inputParams:AddFloat("Min Radius", 8.0)
	scriptCmp.m_inputParams:AddFloat("Max Radius", 32.0)
	scriptCmp.m_inputParams:AddFloat("Min Brightness", 1.0)
	scriptCmp.m_inputParams:AddFloat("Max Brightness", 16.0)
	scriptCmp.m_inputParams:AddFloat("Min Spawn Velocity", -16.0)
	scriptCmp.m_inputParams:AddFloat("Max Spawn Velocity", 16.0)
	scriptCmp.m_inputParams:AddFloat("Gravity", -10.0)
end

function BouncyLights_GetSpawnPosition(blackboard)
	local minX = blackboard:GetFloat("X Min", -16.0)
	local maxX = blackboard:GetFloat("X Max", 16.0)
	local minY = blackboard:GetFloat("Y Min", -16.0)
	local maxY = blackboard:GetFloat("Y Max", 16.0)
	local minZ = blackboard:GetFloat("Z Min", -16.0)
	local maxZ = blackboard:GetFloat("Z Max", 16.0)
	return vec3.new(R3.RandomFloat(minX, maxX), R3.RandomFloat(minY, maxY), R3.RandomFloat(minZ, maxZ))
end

function BouncyLights_GetSpawnColour(blackboard)
	local colour = {0.0,0.0,0.0}
	while colour[1] == 0 and colour[2] == 0 and colour[3] == 0 do
		colour = { R3.RandomFloat(0.0,1.0), R3.RandomFloat(0.0,1.0), R3.RandomFloat(0.0,1.0) }
	end
	return vec3.new(colour[1], colour[2], colour[3])
end

function BouncyLights_GetSpawnBrightness(blackboard)
	local minBrightness = blackboard:GetFloat("Min Brightness", 1.0)
	local maxBrightness = blackboard:GetFloat("Max Brightness", 16.0)
	return R3.RandomFloat(minBrightness,maxBrightness)
end

function BouncyLights_GetSpawnRadius(blackboard)
	local minRadius = blackboard:GetFloat("Min Radius", 8.0)
	local maxRadius = blackboard:GetFloat("Max Radius", 32.0)
	return R3.RandomFloat(minRadius,maxRadius)
end

function BouncyLights_GetSpawnVelocity(blackboard)
	local minVelocity = blackboard:GetFloat("Min Spawn Velocity", -16.0)
	local maxVelocity = blackboard:GetFloat("Max Spawn Velocity", 16.0)
	return { R3.RandomFloat(minVelocity,maxVelocity), R3.RandomFloat(minVelocity,maxVelocity), R3.RandomFloat(minVelocity,maxVelocity) }
end

function BouncyLights_SpawnLights(world, blackboard)
	local lightCount = blackboard:GetInt("Light Count", 32)
	for i=1,lightCount do 
		local lightEntity = world:AddEntity()
		world.AddComponent_PointLight(lightEntity)
		world.AddComponent_Transform(lightEntity)
		
		local transform = world.GetComponent_Transform(lightEntity)
		transform:SetPositionNoInterpolation(BouncyLights_GetSpawnPosition(blackboard))
		
		local pointLight = world.GetComponent_PointLight(lightEntity)
		pointLight.m_colour = BouncyLights_GetSpawnColour(blackboard)
		pointLight.m_brightness = BouncyLights_GetSpawnBrightness(blackboard)
		pointLight.m_distance = BouncyLights_GetSpawnRadius(blackboard)
		pointLight.m_enabled = true
		
		BouncyLights[i] = {}
		BouncyLights[i].Entity = lightEntity
		BouncyLights[i].Velocity = BouncyLights_GetSpawnVelocity(blackboard)
	end
end

function BouncyLights_UpdateMovement(world, blackboard)
	local gravity = blackboard:GetFloat("Gravity", -10.0)
	local minX = blackboard:GetFloat("X Min", -16.0)
	local maxX = blackboard:GetFloat("X Max", 16.0)
	local minY = blackboard:GetFloat("Y Min", -16.0)
	local maxY = blackboard:GetFloat("Y Max", 16.0)
	local minZ = blackboard:GetFloat("Z Min", -16.0)
	local maxZ = blackboard:GetFloat("Z Max", 16.0)
	local timeDelta = R3.GetFixedUpdateDelta()
	for i=1,#BouncyLights do 
		local lightTransform = world.GetComponent_Transform(BouncyLights[i].Entity)
		local velocity = BouncyLights[i].Velocity
		local position = lightTransform:GetPosition()
		local posTable = { position.x, position.y, position.z }
		velocity[2] = velocity[2] + gravity * timeDelta
		posTable[1] = posTable[1] + velocity[1] * timeDelta
		posTable[2] = posTable[2] + velocity[2] * timeDelta
		posTable[3] = posTable[3] + velocity[3] * timeDelta
		if(posTable[1] < minX) then
			posTable[1] = minX
			velocity[1] = -velocity[1]
		end
		if(posTable[1] > maxX) then
			posTable[1] = maxX
			velocity[1] = -velocity[1]
		end
		if(posTable[2] < minY) then
			posTable[2] = minY
			velocity[2] = -velocity[2]
		end
		if(posTable[2] > maxY) then
			posTable[2] = maxY
			velocity[2] = -velocity[2]
		end
		if(posTable[3] < minZ) then
			posTable[3] = minZ
			velocity[3] = -velocity[3]
		end
		if(posTable[3] > maxZ) then
			posTable[3] = maxZ
			velocity[3] = -velocity[3]
		end
		lightTransform:SetPosition(vec3.new(posTable[1],posTable[2],posTable[3]))
	end
end

function BouncyLights_FixedUpdate(e)
	local world = R3.ActiveWorld()
	local scriptCmp = world.GetComponent_LuaScript(e)
	if(BouncyLights == nil or #BouncyLights == 0) then 
		BouncyLights_SpawnLights(world, scriptCmp.m_inputParams)
	else
		BouncyLights_UpdateMovement(world, scriptCmp.m_inputParams)
	end
end