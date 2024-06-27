local Multiballs = {}

Multiballs.spawnCount = 40000
Multiballs.modelPath = 'multiballs/templates/ball.scn'
-- Multiballs.modelPath = 'multiballs/templates/cube.scn'

function Multiballs_SpawnBall(e, matEntity)
	local world = R3.ActiveWorld()
	if(world) then 
		local newBalls = world:ImportScene(Multiballs.modelPath)
		for k=1,#newBalls do
			local b = newBalls[k]
			local transform = world.GetComponent_Transform(b)
			local mesh = world.GetComponent_StaticMesh(b)
			transform:SetPosition(vec3.new(-128.0 + math.random() * 256,2.0 + math.random() * 64,-128.0 + math.random() * 256))
			mesh:SetMaterialOverride(matEntity)
		end
	end
end

function Multiballs_Main(e)
	local world = R3.ActiveWorld()
	local matNames = {"mat1", "mat2", "mat3", "mat4", "mat5", "mat6", "mat7", "mat8"}
	local matEntities = {}
	for m=1,#matNames do 
		matEntities[m] = world:GetEntityByName(matNames[m])
	end
	if(not Multiballs.m_ranFirstFrame) then
		for i=1,Multiballs.spawnCount do
			local matIndex = math.random(1,#matEntities)
			Multiballs_SpawnBall(e, matEntities[matIndex])
		end
	end
	Multiballs.m_ranFirstFrame = true
end