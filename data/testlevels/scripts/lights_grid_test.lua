function LightsGridTest_PopulateInputs(scriptCmp)
	scriptCmp.m_inputParams:AddInt("Light X Count", 32)
	scriptCmp.m_inputParams:AddInt("Light Z Count", 32)
	scriptCmp.m_inputParams:AddFloat("X Min", -64.0)
	scriptCmp.m_inputParams:AddFloat("X Max", 64.0)
	scriptCmp.m_inputParams:AddFloat("Y", 4.0)
	scriptCmp.m_inputParams:AddFloat("Z Min", -64.0)
	scriptCmp.m_inputParams:AddFloat("Z Max", 64.0)
	scriptCmp.m_inputParams:AddFloat("Brightness", 6.0)
	scriptCmp.m_inputParams:AddFloat("Radius", 8.0)
end

function SpawnLights(world, blackboard)
	local minX = blackboard:GetFloat("X Min", -64.0)
	local maxX = blackboard:GetFloat("X Max", 64.0)
	local spawnY = blackboard:GetFloat("Y", 4.0)
	local minZ = blackboard:GetFloat("Z Min", -64.0)
	local maxZ = blackboard:GetFloat("Z Max", 64.0)
	local lightsCountX = blackboard:GetInt("Light X Count", 32)
	local lightsCountZ = blackboard:GetInt("Light Z Count", 32)
	local brightness = blackboard:GetFloat("Brightness", 6.0)
	local radius = blackboard:GetFloat("Radius", 8.0)
	local lightXStep = (maxX - minX) / lightsCountX
	local lightZStep = (maxZ - minZ) / lightsCountZ
	for x=1, lightsCountX do 
		for z=1, lightsCountZ do 
			local lightEntity = world:AddEntity()
			world.AddComponent_PointLight(lightEntity)
			world.AddComponent_Transform(lightEntity)
			
			local spawnX = -(lightXStep * 0.5) + minX + lightXStep * x 
			local spawnZ = -(lightZStep * 0.5) + minZ + lightZStep * z
			local transform = world.GetComponent_Transform(lightEntity)
			transform:SetPositionNoInterpolation(vec3.new(spawnX, spawnY, spawnZ))
			
			local pointLight = world.GetComponent_PointLight(lightEntity)
			pointLight.m_colour = vec3.new(1,1,1)
			pointLight.m_brightness = brightness
			pointLight.m_distance = radius
			pointLight.m_enabled = true
		end
	end
end

function LightsGridTest_FixedUpdate(e)
	local world = R3.ActiveWorld()
	local scriptCmp = world.GetComponent_LuaScript(e)
	SpawnLights(world, scriptCmp.m_inputParams)
	
	local cameraEntity = world:GetEntityByName("Camera")
	R3.SetActiveCamera(cameraEntity)
	
	scriptCmp.m_isActive = false
end