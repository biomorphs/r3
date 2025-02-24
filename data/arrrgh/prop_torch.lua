function Dungeons_TorchOnPickup(actor,torch)
	local world = R3.ActiveWorld()
	local allChildren = world:GetAllChildren(torch)
	for child=1,#allChildren do 
		local light = world.GetComponent_PointLight(allChildren[child])
		if(light ~= nil) then 
			light.m_enabled = false
		end
	end
end

function Dungeons_TorchOnEquip(actor,torch)
	local world = R3.ActiveWorld()
	world:SetParent(torch, actor)
	local rootTransform = world.GetComponent_Transform(torch)	-- move the entity above the actor
	if(rootTransform) then 
		rootTransform:SetPosition(vec3.new(0.9,1.5,0.2))
		rootTransform:SetIsRelative(true)
	end 
	local allChildren = world:GetAllChildren(torch)
	for child=1,#allChildren do 
		local meshComponent = world.GetComponent_DynamicMesh(allChildren[child])
		if(meshComponent ~= nil) then 
			meshComponent:SetShouldDraw(true)
		end
		local light = world.GetComponent_PointLight(allChildren[child])
		if(light ~= nil) then 
			light.m_enabled = true
		end
	end
end

function Dungeons_TorchOnUnequip(actor,torch)
	local world = R3.ActiveWorld()
	world:SetParent(torch, EntityHandle.new())
	local allChildren = world:GetAllChildren(torch)
	for child=1,#allChildren do 
		local meshComponent = world.GetComponent_DynamicMesh(allChildren[child])
		if(meshComponent ~= nil) then 
			meshComponent:SetShouldDraw(false)
		end
		local light = world.GetComponent_PointLight(allChildren[child])
		if(light ~= nil) then 
			light.m_enabled = false
		end
	end
end