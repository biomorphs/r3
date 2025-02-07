require 'arrrgh/arrrgh_shared'

Arrrgh_Globals.CameraHeight = Arrrgh_Globals.CameraHeight or 50
Arrrgh_Globals.CameraLookAt = Arrrgh_Globals.CameraLookAt or vec3.new(0,0,0)
Arrrgh_Globals.CameraSpeed = Arrrgh_Globals.CameraSpeed or vec3.new(128,128,128)

-- camera will animate to look at target position
function Dungeons_CameraLookAt(worldPos, camHeight)
	local newCamHeight = camHeight or Arrrgh_Globals.CameraHeight
	Arrrgh_Globals.CameraLookAt = worldPos
	Arrrgh_Globals.CameraHeight = newCamHeight
end

function Dungeons_CameraActivate()
	local world = R3.ActiveWorld()
	local cameraEntity = world:GetEntityByName('GameCamera')
	R3.SetActiveCamera(cameraEntity)
end

-- camera fixed update
function Dungeons_UpdateCamera(e)
	local camHeight = Arrrgh_Globals.CameraHeight
	local mouseScroll = R3.GetMouseWheelScroll()
	camHeight = camHeight + (mouseScroll * -4)
	if(camHeight < 20) then 
		camHeight = 20
	end
	if(camHeight > 100) then 
		camHeight = 100
	end
	Arrrgh_Globals.CameraHeight = camHeight

	-- move the camera towards the lookAt point  
	local world = R3.ActiveWorld()
	local cameraEntity = world:GetEntityByName('GameCamera')
	local cameraTransform = world.GetComponent_Transform(cameraEntity)
	local cameraPos = cameraTransform:GetPosition()
	local targetPosition = vec3.new(Arrrgh_Globals.CameraLookAt.x, Arrrgh_Globals.CameraLookAt.y + Arrrgh_Globals.CameraHeight, Arrrgh_Globals.CameraLookAt.z - 6.0)
	local camToTarget = vec3.new(targetPosition.x - cameraPos.x,targetPosition.y - cameraPos.y,targetPosition.z - cameraPos.z)
	local targetLength = R3.Vec3Length(camToTarget)
	local speedLimit = targetLength * 2	-- scale max speed by distance to goal
	local actualSpeed = vec3.new(math.min(Arrrgh_Globals.CameraSpeed.x, speedLimit),math.min(Arrrgh_Globals.CameraSpeed.y, speedLimit),math.min(Arrrgh_Globals.CameraSpeed.z, speedLimit))
	camToTarget.x = camToTarget.x / targetLength
	camToTarget.y = camToTarget.y / targetLength
	camToTarget.z = camToTarget.z / targetLength
	if(targetLength > 0.01) then
		cameraPos.x = cameraPos.x + (camToTarget.x * actualSpeed.x * R3.GetFixedUpdateDelta())
		cameraPos.y = cameraPos.y + (camToTarget.y * actualSpeed.y * R3.GetFixedUpdateDelta())
		cameraPos.z = cameraPos.z + (camToTarget.z * actualSpeed.z * R3.GetFixedUpdateDelta())
		cameraTransform:SetPosition(cameraPos)
	end
end