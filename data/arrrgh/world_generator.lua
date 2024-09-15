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
		MonsterSpawns = {}
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
	local newMonster = {
		Position = pos,
		TypeString = monsterType
	}
	table.insert(context.MonsterSpawns, newMonster)
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

-- note that the spawner entity is not added as an actor to the grid 
-- everything gets spawned immediately on game start then the spawners are removed
-- a single entity called 'PlayerActor' is created
function Dungeons_Generator.CreatePlayerSpawn(grid, spawnPos)
	local world = R3.ActiveWorld()
	local spawnPointEntities = world:ImportScene('arrrgh/pois/playerspawnpoint.scn')
	local worldPos = vec3.new(spawnPos.x * Arrrgh_Globals.TileDimensions.x, 0, spawnPos.y * Arrrgh_Globals.TileDimensions.y)
	worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
	worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
	Arrrgh.MoveEntitiesWorldspace(spawnPointEntities, worldPos)
end

-- monster spawners are entities with Dungeons_MonsterSpawner components 
-- the game can interpret these however it wants
-- note the spawners are not world grid actors
function Dungeons_Generator.CreateMonsterSpawns(grid,context)
	-- the monster spawns are currently just a list of typestr(s), position
	-- create spawner components / entities that can be enumerated by the game
	local world = R3.ActiveWorld()
	for spawn=1,#context.MonsterSpawns do 
		local spawnPos = context.MonsterSpawns[spawn].Position
		local world = R3.ActiveWorld()
		local spawnerEntities = world:ImportScene('arrrgh/pois/monster_spawner.scn')
		local spawnCmp = world.GetComponent_Dungeons_MonsterSpawner(spawnerEntities[1])
		spawnCmp.m_monsterType = context.MonsterSpawns[spawn].TypeString
		spawnCmp.m_spawnPosition = spawnPos
		local worldPos = vec3.new(spawnPos.x * Arrrgh_Globals.TileDimensions.x, 0, spawnPos.y * Arrrgh_Globals.TileDimensions.y)
		worldPos.x = worldPos.x + (Arrrgh_Globals.TileDimensions.x * 0.5)
		worldPos.z = worldPos.z + (Arrrgh_Globals.TileDimensions.y * 0.5)
		Arrrgh.MoveEntitiesWorldspace(spawnerEntities, worldPos)
	end
end

function Dungeons_Generator.OnFinish(gridcmp,generator)
	print('Generator finished')
	print('Player spawn point: ', generator.Context.SpawnPoint.x, generator.Context.SpawnPoint.y)
	print(#generator.Context.Rooms, ' rooms spawned')
	gridcmp.m_debugDraw = false
	Dungeons_Generator.CreatePlayerSpawn(gridcmp, generator.Context.SpawnPoint)
	Dungeons_Generator.CreateMonsterSpawns(gridcmp, generator.Context)
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
		coroutine.resume(generator.MainCoroutine, gridCmp, generator)
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