local Multiballs = {}

function Multiballs_SpawnBall(e)
	local world = R3.ActiveWorld()
	if(world) then 
		local newBalls = world:ImportScene('multiballs/templates/ball.scn')
		for k=1,#newBalls do
			local b = newBalls[k]
			local transform = world.GetComponent_Transform(b)
			local mesh = world.GetComponent_StaticMesh(b)
			transform:SetPosition(vec3.new(-128.0 + math.random() * 256,2.0 + math.random() * 64,-128.0 + math.random() * 256))
			mesh:SetMaterialOverride(e)
		end
	end
end

function Multiballs_SpawnCopcar(e)
	local world = R3.ActiveWorld()
	if(world) then 
		local newBalls = world:ImportScene('multiballs/templates/police.scn')
		for k=1,#newBalls do
			local b = newBalls[k]
			local transform = world.GetComponent_Transform(b)
			local mesh = world.GetComponent_StaticMesh(b)
			transform:SetPosition(vec3.new(-128.0 + math.random() * 256,2.0 + math.random() * 64,-128.0 + math.random() * 256))
			mesh:SetMaterialOverride(e)
		end
	end
end

function Multiballs_Main(e)
	if(not Multiballs.m_ranFirstFrame) then
		for i=1,50000 do
			Multiballs_SpawnBall(e)
		end
	end
	Multiballs.m_ranFirstFrame = true
end