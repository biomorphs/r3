function Dungeons_NewMeleeAttackAction(attacker, target)
	local newAction = {}
	newAction.name = "Melee Attack"
	newAction.attacker = EntityHandle.new(attacker)
	newAction.target = EntityHandle.new(target)
	newAction.onRun = Dungeons_ActionMeleeAttack
	Fastqueue.pushright(Arrrgh_Globals.ActionQueue, newAction)
end

function Dungeons_ActionMeleeAttack(action)
	local world = R3.ActiveWorld()
	local didHit = Dungeons_DidMeleeAttackHit(world, action.attacker, action.target)
	if(didHit == false) then 
		print(world:GetEntityName(action.attacker), ' missed ', world:GetEntityName(action.target), '! Whiff!')
	else
		local attackDamage = Dungeons_CalculateMeleeDamageDealt(world, action.attacker)
		print(world:GetEntityName(action.attacker), ' attacks ', world:GetEntityName(action.target), ' for ', attackDamage, ' damage!')
		Dungeons_TakeDamage(world, action.target, attackDamage)
	end
	Dungeons_SpendActionPoint()
	return 'complete'
end