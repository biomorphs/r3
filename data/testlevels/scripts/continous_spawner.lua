--- params 
function Spawner_PopulateInputs(scriptCmp)
	scriptCmp.m_inputParams:AddFloat("Spawn Frequency", 0.1)
	scriptCmp.m_inputParams:AddInt("X Dims", 8)
	scriptCmp.m_inputParams:AddInt("Z Dims", 8)
	scriptCmp.m_inputParams:AddInt("Spawn Count", 1)
end

local Spawner_LastSpawnTime = 0.0
local Spawner_SpawnCount = 0

function Spawner_DoSpawn(world, scriptCmp)	
	local xDims = scriptCmp.m_inputParams:GetInt("X Dims", 8)
	local zDims = scriptCmp.m_inputParams:GetInt("Z Dims", 8)
	
	local newCube = world:AddEntity()
	
	local newTransform = world.AddComponent_Transform(newCube)
	local xPos = math.floor(Spawner_SpawnCount % xDims) * 2.0
	local yPos = math.floor(Spawner_SpawnCount / (xDims * zDims)) * 2.0
	local zPos = math.floor((Spawner_SpawnCount / xDims) % zDims) * 2.0
	newTransform:SetPositionNoInterpolation(vec3.new(xPos, yPos, zPos))
	
	local newMesh = world.AddComponent_StaticMesh(newCube)
	newMesh:SetModelFromPath("common/models/cube.fbx")
	
	Spawner_SpawnCount = Spawner_SpawnCount + 1
end

function Spawner_FixedUpdate(e)
	local world = R3.ActiveWorld()
	local scriptCmp = world.GetComponent_LuaScript(e)
	Spawner_LastSpawnTime = Spawner_LastSpawnTime + R3.GetFixedUpdateDelta()
	if(Spawner_LastSpawnTime > scriptCmp.m_inputParams:GetFloat("Spawn Frequency", 0.25)) then 
		Spawner_LastSpawnTime = 0.0
		local spawnCount = scriptCmp.m_inputParams:GetInt("Spawn Count", 1)
		for i=1,spawnCount do 
			Spawner_DoSpawn(world, scriptCmp)
		end
		R3.RebuildStaticScene()
	end
end