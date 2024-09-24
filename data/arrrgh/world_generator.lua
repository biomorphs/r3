Dungeons_GeneratorContext = {}
Dungeons_Generator = {}

-- high-level generator functionality 
-- generator consists of a list of steps, which act on a grid
-- generator params are passed from step to step 

-- use context object for shared data
-- steps can freely write stuff here
function Dungeons_GeneratorContext.new()
	return {
		SpawnPoint = uvec2.new(0,0),
		Rooms = {},
		MonsterSpawns = {},
		Items = {}
	}
end

-- entrances - list of uvec2s representing doors/entry points
function Dungeons_GeneratorContext.AddRoom(context, pos, size, entrances)
	local newRoom = {
		Position = pos,
		Size = size,
		Entrances = entrances
	}
	table.insert(context.Rooms, newRoom)
end

-- should generator be dictating monster levels + stuff? or the game? most likely the game
function Dungeons_GeneratorContext.AddMonster(context, monsterType, pos)
	for m=1,#context.MonsterSpawns do
		if(pos.x == context.MonsterSpawns[m].Position.x and pos.y == context.MonsterSpawns[m].Position.y) then 
			R3.LogWarn("A monter already exists at " .. pos.x .. ", " .. pos.y)
			return 
		end
	end
	local newMonster = {
		Position = pos,
		TypeString = monsterType
	}
	table.insert(context.MonsterSpawns, newMonster)
end


-- game does the actual spawning
function Dungeons_GeneratorContext.AddItem(context, name, tilePos)
	local newItem = {
		Position = tilePos,
		Name = name
	}
	table.insert(context.Items, newItem)
end

-- get a generator object for a grid
function Dungeons_Generator.new(gridEntity)
	return {
		GridEntity = gridEntity,
		Steps = {},
		CurrentStep = 1,
		MainCoroutine = nil,
		Context = Dungeons_GeneratorContext.new()
	}
end

-- add a step to the generator
-- stepObject = a table/object with a member void Run(gridcmp,context)
function Dungeons_Generator.AddStep(generator,name,stepObject)
	print('Adding step ', name)
	local newStep = {
		Name = name,
		Step = stepObject
	}
	table.insert(generator.Steps, newStep)
end

-- runs all steps in the generator
function Dungeons_Generator._Run(gridcmp,generator)
	print('Running generator')
	gridcmp.m_debugDraw = true
	for step=1,#generator.Steps do 
		print('running step ', generator.Steps[step].Name)
		generator.Steps[step].Step.Run(gridcmp, generator.Context)
		Dungeons_Generator.Yield()
		generator.CurrentStep = generator.CurrentStep + 1
	end
end

function Dungeons_Generator.OnFinish(gridcmp,generator)
	print('Generator finished')
	print('Player spawn point: ', generator.Context.SpawnPoint.x, generator.Context.SpawnPoint.y)
	print(#generator.Context.Rooms, ' rooms spawned')
	print(#generator.Context.MonsterSpawns, ' monsters spawned')
	gridcmp.m_debugDraw = false
end

-- returns 'complete' on completion 
function Dungeons_Generator.Continue(generator)
	local gridCmp = R3.ActiveWorld().GetComponent_Dungeons_WorldGridComponent(generator.GridEntity)
	if(gridCmp == nil) then
		print('no grid')
		return
	end
	if(generator.MainCoroutine == nil) then 
		print('Starting dungeon generator')
		generator.MainCoroutine = coroutine.create( Dungeons_Generator._Run )
	end
	local runningStatus = coroutine.status(generator.MainCoroutine)
	if(runningStatus ~= 'dead') then
		local result, errorText = coroutine.resume(generator.MainCoroutine, gridCmp, generator)
		if(result == false) then
			R3.LogError("Dungeons_Generator.Continue failed - \n" .. errorText)
		end
		return 'working'
	else
		Dungeons_Generator.OnFinish(gridCmp, generator)
		return 'complete'
	end
end

-- yields until time passed
function Dungeons_Generator.Yield(timeToWait)
	timeToWait = timeToWait or 0.1
	local currentWaitTime = 0 
	repeat
		currentWaitTime = currentWaitTime + R3.GetVariableDelta() -- variable delta is good enough
		coroutine.yield()
	until(currentWaitTime >= timeToWait)
end