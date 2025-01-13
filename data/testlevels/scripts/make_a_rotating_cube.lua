
function RotatingCubes_PopulateInputs(scriptCmp)
	scriptCmp.m_inputParams:AddInt("Cube Count", 1000)
	scriptCmp.m_inputParams:AddFloat("X Min", -32.0)
	scriptCmp.m_inputParams:AddFloat("X Max", 32.0)
	scriptCmp.m_inputParams:AddFloat("Y Min", 0.0)
	scriptCmp.m_inputParams:AddFloat("Y Max", 16.0)
	scriptCmp.m_inputParams:AddFloat("Z Min", -32.0)
	scriptCmp.m_inputParams:AddFloat("Z Max", 32.0)
end

function RotatingCubes_GetSpawnPosition(blackboard)
	local minX = blackboard:GetFloat("X Min", -16.0)
	local maxX = blackboard:GetFloat("X Max", 16.0)
	local minY = blackboard:GetFloat("Y Min", -16.0)
	local maxY = blackboard:GetFloat("Y Max", 16.0)
	local minZ = blackboard:GetFloat("Z Min", -16.0)
	local maxZ = blackboard:GetFloat("Z Max", 16.0)
	return vec3.new(R3.RandomFloat(minX, maxX), R3.RandomFloat(minY, maxY), R3.RandomFloat(minZ, maxZ))
end

function RotateCubePls(e)
	local world = R3.ActiveWorld()
	if(world == nil) then 
		print('No active world?')
		return;
	end
	local myTransform = world.GetComponent_Transform(e)
	if(myTransform == nil) then 
		print('No transform?')
		return;
	end
	local myRotation = myTransform:GetOrientation()
	myRotation = R3.RotateQuat(myRotation, R3.GetFixedUpdateDelta() * math.pi, vec3.new(0,1,0))
	myTransform:SetOrientation(myRotation)
end

function MakeACube(world, blackboard)
	local newCube = world:AddEntity()
	
	local newTransform = world.AddComponent_Transform(newCube)
	newTransform:SetPositionNoInterpolation(RotatingCubes_GetSpawnPosition(blackboard))
	
	local newMesh = world.AddComponent_StaticMesh(newCube)
	newMesh:SetModelFromPath("common/models/cube.fbx")
end

function MakeManyRotatingCubes_FixedUpdate(e)
	local world = R3.ActiveWorld()
	if(world == nil) then 
		print('No active world?')
		return;
	end
	local myScriptCmp = world.GetComponent_LuaScript(e)
	local count = myScriptCmp.m_inputParams:GetInt("Cube Count", 1000)
	for i=1,count do
		MakeACube(world, myScriptCmp.m_inputParams)
	end
	
	if(myScriptCmp == nil) then 
	 	print('No script cmp?!')
	 	return
	end
	myScriptCmp.m_isActive = false
end
