function Dungeons_NewMeleeAttackAction(attacker, target)
	local newAction = {}
	newAction.name = "Melee Attack"
	newAction.attacker = attacker
	newAction.target = target
	newAction.onRun = Dungeons_ActionMeleeAttack
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionMeleeAttack(action)
	local world = R3.ActiveWorld()
	
	local attackDamage = 5
	print(world:GetEntityName(action.attacker), ' attacks ', world:GetEntityName(action.target), ' for ', attackDamage, ' damage!')
	Dungeons_SpendActionPoint()

	return 'complete'
end