function Dungeons_NewInspectAction(actorToInspect)
	local newAction = {}
	newAction.name = "Inspect"
	newAction.target = EntityHandle.new(actorToInspect)	-- always make a new handle, otherwise its a reference
	newAction.onRun = Dungeons_ActionInspect
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionInspect(action)
	local world = R3.ActiveWorld()
	local inspectCmp = world.GetComponent_Dungeons_Inspectable(action.target)
	if(inspectCmp ~= nil) then 
		print(inspectCmp.m_inspectText)
	end
	return 'complete'
end