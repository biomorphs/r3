
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

function MakeARotatingCube_FixedUpdate(e)
	local world = R3.ActiveWorld()
	if(world == nil) then 
		print('No active world?')
		return;
	end
	
	local newCube = world:AddEntity()
	
	local newTransform = world.AddComponent_Transform(newCube)
	newTransform:SetPositionNoInterpolation(vec3.new(-16.0 + math.random() * 32.0,1.0 + math.random() * 4.0,-16.0 + math.random() * 32.0))
	
	local newMesh = world.AddComponent_StaticMesh(newCube)
	newMesh:SetModelFromPath("common/models/cube.fbx")
	
	local newScript = world.AddComponent_LuaScript(newCube)
	newScript:SetFixedUpdateSource("testlevels/scripts/make_a_rotating_cube.lua")
	newScript:SetFixedUpdateEntrypoint("RotateCubePls")
	newScript.m_needsRecompile = true
	newScript.m_isActive = true
	
	local myScriptCmp = world.GetComponent_LuaScript(e)
	if(myScriptCmp == nil) then 
	 	print('No script cmp?!')
	 	return
	end
	myScriptCmp.m_isActive = false
	print('Bye!')
end
