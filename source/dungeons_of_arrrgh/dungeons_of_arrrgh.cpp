#include "dungeons_of_arrrgh.h"
#include "world_grid_component.h"
#include "vision_component.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/systems/lua_system.h"
#include "engine/components/transform.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "core/profiler.h"
#include "core/random.h"
#include "core/log.h"
#include <sol/sol.hpp>

// This needs to go
// Generation should be all in lua
// Tiles should just have flags (visible, passable, etc)
// Visuals should be completely determined by lua
// Gameplay stuff should be via entities/components
enum WorldTileType : uint8_t
{
	Empty,
	Wall,
	FloorInterior,
	FloorExterior,
	PlayerSpawnPoint,
	LevelExit,
	MaxTypes
};

DungeonsOfArrrgh::DungeonsOfArrrgh()
{
}

DungeonsOfArrrgh::~DungeonsOfArrrgh()
{
}

void DungeonsOfArrrgh::RegisterTickFns()
{
	R3_PROF_EVENT();
	RegisterTick("DungeonsOfArrrgh::VariableUpdate", [this]() {
		return VariableUpdate();
	});
	RegisterTick("DungeonsOfArrrgh::FixedUpdate", [this]() {
		return FixedUpdate();
	});
}

bool DungeonsOfArrrgh::Init()
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	entities->RegisterComponentType<DungeonsWorldGridComponent>(16);	// probably only need 1 per world, but eh
	entities->RegisterComponentType<DungeonsVisionComponent>(8092);

	auto scriptNamespace = "Arrrgh";
	auto scripts = GetSystem<R3::LuaSystem>();
	scripts->RegisterFunction("DebugDrawTiles", [this](DungeonsWorldGridComponent* grid, std::unordered_set<glm::uvec2>& tiles) {
		DebugDrawTiles(*grid, tiles);
	}, scriptNamespace);
	scripts->RegisterFunction("MoveEntities", [this](const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset) {
		MoveEntities(targets, offset);
	}, scriptNamespace);

	return true;
}

void DungeonsOfArrrgh::Shutdown()
{
	R3_PROF_EVENT();
}

void DebugDrawTile(R3::ImmediateRenderSystem& imRender, 
	const DungeonsWorldGridComponent& grid, 
	uint32_t x, uint32_t z, 
	glm::vec3 offset, glm::vec2 scale, 
	std::vector<R3::ImmediateRenderer::PosColourVertex>& verts)
{
	R3_PROF_EVENT();
	glm::vec3 tileOffset = { (float)x * scale.x, 0.1f, (float)z * scale.y };
	glm::vec3 basePos = offset + tileOffset;
	auto contents = grid.GetContents(x, z);
	if (contents && contents->m_tileData.m_tileType != WorldTileType::Empty)
	{
		glm::vec4 colour = { 1,0,0,.25 };
		if (contents->m_tileData.m_passable)
		{
			colour = { 0,1,0,.25 };
		}
		if (contents->m_tileData.m_tileType == WorldTileType::PlayerSpawnPoint)
		{
			colour = { 1,0,1,.25 };
		}
		if (contents->m_tileData.m_tileType == WorldTileType::LevelExit)
		{
			colour = { 1,1,1,0.25f };
		}
		verts.push_back({ {basePos, 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,0), 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,scale.y), 1}, colour });
		verts.push_back({ {basePos, 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,scale.y), 1}, colour });
		verts.push_back({ {basePos + glm::vec3(0,0,scale.y), 1}, colour });
		if (!contents->m_tileData.m_passable)
		{
			glm::vec3 cubeScale = { scale.x * 0.5f, 2.0f, scale.y * 0.5f };
			glm::mat4 transform = glm::scale(glm::translate(basePos + glm::vec3(cubeScale.x, 1.0f, cubeScale.z)), cubeScale);
			imRender.m_imRender->AddCubeWireframe(transform, colour, true);
		}
	}
}

void DungeonsOfArrrgh::DebugDrawWorldGrid(const DungeonsWorldGridComponent& grid)
{
	R3_PROF_EVENT();
	auto imRender = GetSystem<R3::ImmediateRenderSystem>();
	static std::vector<R3::ImmediateRenderer::PosColourVertex> outVertices;	// tri vertices to draw, dont keep reallocating
	outVertices.clear();
	const auto dims = grid.GetDimensions();
	outVertices.reserve(dims.x * dims.y);
	const glm::vec4 edgeColour(1, 1, 0, 1);
	R3::ImmediateRenderer::PosColourVertex outerEdge[] = {
		{{m_wsGridOffset.x, m_wsGridOffset.y, m_wsGridOffset.z, 1}, edgeColour},
		{{m_wsGridOffset.x + m_wsGridScale.x * dims.x, m_wsGridOffset.y, m_wsGridOffset.z, 1}, edgeColour},
		{{m_wsGridOffset.x, m_wsGridOffset.y, m_wsGridOffset.z + m_wsGridScale.y * dims.y, 1}, edgeColour},
		{{m_wsGridOffset.x + m_wsGridScale.x * dims.x, m_wsGridOffset.y, m_wsGridOffset.z + m_wsGridScale.y * dims.y, 1}, edgeColour},
		{{m_wsGridOffset.x, m_wsGridOffset.y, m_wsGridOffset.z, 1}, edgeColour},
		{{m_wsGridOffset.x, m_wsGridOffset.y, m_wsGridOffset.z + m_wsGridScale.y * dims.y, 1}, edgeColour},
		{{m_wsGridOffset.x + m_wsGridScale.x * dims.x, m_wsGridOffset.y, m_wsGridOffset.z, 1}, edgeColour},
		{{m_wsGridOffset.x + m_wsGridScale.x * dims.x, m_wsGridOffset.y, m_wsGridOffset.z + m_wsGridScale.y * dims.y, 1}, edgeColour},
	};
	imRender->m_imRender->AddLines(outerEdge, 4);
	for (uint32_t z = 0; z < dims.y; ++z)
	{
		for (uint32_t x = 0; x < dims.x; ++x)
		{
			DebugDrawTile(*imRender, grid, x, z, m_wsGridOffset, m_wsGridScale, outVertices);
		}
	}
	if (outVertices.size() > 0)
	{
		imRender->m_imRender->AddTriangles(outVertices.data(), (uint32_t)outVertices.size() / 3, true);
	}
}

void DungeonsOfArrrgh::DebugDrawTiles(const DungeonsWorldGridComponent& grid, std::unordered_set<glm::uvec2>& tiles)
{
	R3_PROF_EVENT();
	auto imRender = GetSystem<R3::ImmediateRenderSystem>();
	static std::vector<R3::ImmediateRenderer::PosColourVertex> outVertices;	// tri vertices to draw, dont keep reallocating
	outVertices.clear();
	outVertices.reserve(tiles.size() * 6);
	for (const auto& tile : tiles)
	{
		DebugDrawTile(*imRender, grid, tile.x, tile.y, m_wsGridOffset, m_wsGridScale, outVertices);
	}
	if (outVertices.size() > 0)
	{
		imRender->m_imRender->AddTriangles(outVertices.data(), (uint32_t)outVertices.size() / 3, true);
	}
}

void DungeonsOfArrrgh::UpdateVision(DungeonsWorldGridComponent& grid, R3::Entities::World& w)
{
	R3_PROF_EVENT();
	auto forEachVision = [&](const R3::Entities::EntityHandle& e, DungeonsVisionComponent& v, R3::TransformComponent& t) {
		if (v.m_needsUpdate)
		{
			v.m_visibleTiles.clear();
			glm::vec3 worldSpacePos(t.GetWorldspaceMatrix()[3]);	// don't need interpolation, we are in fixed update
			auto tileMaybe = GetTileFromWorldspace(grid, worldSpacePos);	
			if (tileMaybe.has_value() && v.m_visionMaxDistance > 0)
			{
				const uint32_t visionDistance = (uint32_t)ceil(v.m_visionMaxDistance);
				v.m_visibleTiles = grid.FindVisibleTiles(glm::ivec2(tileMaybe.value()), visionDistance);
			}
		}
		return true;
	};
	R3::Entities::Queries::ForEachAsync<DungeonsVisionComponent, R3::TransformComponent>(&w, 1, forEachVision);
}

void DungeonsOfArrrgh::MoveEntities(const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	for (auto target : targets)
	{
		if (auto transCmp = activeWorld->GetComponent<R3::TransformComponent>(target))
		{
			transCmp->SetPositionNoInterpolation(transCmp->GetPosition() + offset);
		}		
	}
}

void DungeonsOfArrrgh::GenerateTileVisuals(uint32_t x, uint32_t z, DungeonsWorldGridComponent& grid, std::vector<R3::Entities::EntityHandle>& outEntities)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	auto thisTile = grid.GetContents(x, z);
	std::string tileToLoad = "arrrgh/tiles/basic_floor_tile_4x4.scn";
	switch (thisTile->m_tileData.m_tileType)
	{
	case WorldTileType::FloorExterior:
		tileToLoad = "arrrgh/tiles/basic_floor_dirt_4x4.scn";
		break;
	case WorldTileType::FloorInterior:
		tileToLoad = "arrrgh/tiles/basic_floor_wood_4x4.scn";
		break;
	case WorldTileType::Wall:
		{
			auto left = x > 1 ? grid.GetContents(x - 1, z) : nullptr;
			auto right = x + 1 < grid.GetDimensions().x ? grid.GetContents(x + 1, z) : nullptr;
			auto up = z > 1 ? grid.GetContents(x, z - 1) : nullptr;
			auto down = z + 1 < grid.GetDimensions().y ? grid.GetContents(x, z + 1) : nullptr;
			bool leftWall = left ? left->m_tileData.m_tileType == WorldTileType::Wall : false;
			bool rightWall = right ? right->m_tileData.m_tileType == WorldTileType::Wall : false;
			bool upWall = up ? up->m_tileData.m_tileType == WorldTileType::Wall : false;
			bool downWall = down ? down->m_tileData.m_tileType == WorldTileType::Wall : false;
			tileToLoad = "arrrgh/tiles/basic_crosswall_tile_4x4.scn";	// cross piece by default
			if ((leftWall || rightWall) && (upWall || downWall))	// corner detection
			{
				tileToLoad = "arrrgh/tiles/basic_crosswall_tile_4x4.scn";
			}
			else if (leftWall || rightWall)
			{
				bool useTorchWall = R3::Random::GetFloat() < 0.1f;
				if (useTorchWall)
				{
					tileToLoad = "arrrgh/tiles/basic_hwall_torch_tile_4x4.scn";
				}
				else
				{
					tileToLoad = "arrrgh/tiles/basic_hwall_tile_4x4.scn";
				}
			}
			else if (upWall || downWall)
			{
				tileToLoad = "arrrgh/tiles/basic_vwall_tile_4x4.scn";
			}
		}
		break;
	}
	auto foundInCache = m_generateVisualsEntityCache.find(tileToLoad);
	if (foundInCache == m_generateVisualsEntityCache.end())
	{
		// load the entities via the world, serialise them, then delete the temp loaded entities + store the json
		auto loadedEntities = activeWorld->Import(tileToLoad);
		R3::JsonSerialiser entityData = activeWorld->SerialiseEntities(loadedEntities);
		m_generateVisualsEntityCache[tileToLoad] = entityData.GetJson();
		foundInCache = m_generateVisualsEntityCache.find(tileToLoad);
		for (auto it : loadedEntities)
		{
			activeWorld->RemoveEntity(it);
		}
	}
	if (foundInCache != m_generateVisualsEntityCache.end())
	{
		R3::JsonSerialiser entityData(R3::JsonSerialiser::Read, foundInCache->second);
		outEntities = activeWorld->SerialiseEntities(entityData);	// clone via serialisation
	}
}

void DungeonsOfArrrgh::GenerateWorldVisuals(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	const auto dims = grid.GetDimensions();
	std::vector<R3::Entities::EntityHandle> newTileEntities;
	for (uint32_t z = 0; z < dims.y; ++z)
	{
		for (uint32_t x = 0; x < dims.x; ++x)
		{
			if (auto tile = grid.GetContents(x, z))
			{
				if (tile->m_tileData.m_tileType != WorldTileType::Empty)
				{
					glm::vec3 tileOffset = { (float)x * m_wsGridScale.x, 0.0f, (float)z * m_wsGridScale.y };
					glm::vec3 basePos = m_wsGridOffset + tileOffset;
					auto childName = std::format("Tile {}x{}", x, z);
					auto newChild = activeWorld->AddEntity();
					activeWorld->SetParent(newChild, e);
					activeWorld->SetEntityName(newChild, childName);
					newTileEntities.clear();
					GenerateTileVisuals(x, z, grid, newTileEntities);
					MoveEntities(newTileEntities, basePos);	// move all new entities to the tile pos
					for (auto impChild : newTileEntities)	// make sure any imported entities have the correct parent
					{
						if (activeWorld->GetParent(impChild).GetID() == -1)
						{
							activeWorld->SetParent(impChild, newChild);
						}
					}
				}
			}
		}
	}
	m_generateVisualsEntityCache.clear();
}

void DungeonsOfArrrgh::OnWorldGridDirty(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();

	// delete all previous visual entities
	std::vector<R3::Entities::EntityHandle> children;
	activeWorld->GetAllChildren(e, children);
	for (auto child : children)
	{
		activeWorld->RemoveEntity(child);
	}

	// now run the visual generator
	GenerateWorldVisuals(e, grid);
}

void DungeonsOfArrrgh::DebugDrawVisibleTiles(const class DungeonsWorldGridComponent& grid, R3::Entities::World& w)
{
	R3_PROF_EVENT();
	auto forEachVision = [&](const R3::Entities::EntityHandle& e, DungeonsVisionComponent& v, R3::TransformComponent& t) {
		DebugDrawTiles(grid, v.m_visibleTiles);
		return true;
	};
	R3::Entities::Queries::ForEach<DungeonsVisionComponent, R3::TransformComponent>(&w, forEachVision);
}

bool DungeonsOfArrrgh::VariableUpdate()
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	if (activeWorld)
	{
		auto forEachGrid = [&](const R3::Entities::EntityHandle& e, DungeonsWorldGridComponent& cmp) {
			if (cmp.m_isDirty)
			{
				OnWorldGridDirty(e, cmp);
				cmp.m_isDirty = false;
			}
			if (cmp.m_debugDraw)
			{
				DebugDrawWorldGrid(cmp);
			}
			if (m_debugDrawVisibleTiles)
			{
				DebugDrawVisibleTiles(cmp, *activeWorld);
			}
			return true;
		};
		R3::Entities::Queries::ForEach<DungeonsWorldGridComponent>(activeWorld, forEachGrid);
	}
	return true;
}

bool DungeonsOfArrrgh::FixedUpdate()
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	if (activeWorld)
	{
		auto forEachGrid = [&](const R3::Entities::EntityHandle& e, DungeonsWorldGridComponent& cmp) {
			UpdateVision(cmp, *activeWorld);	// updates vision for all entities in this grid
			return true;
		};
		R3::Entities::Queries::ForEach<DungeonsWorldGridComponent>(activeWorld, forEachGrid);
	}
	return true;
}

std::optional<glm::uvec2> DungeonsOfArrrgh::GetTileFromWorldspace(DungeonsWorldGridComponent& grid, glm::vec3 worldspace)
{
	// scale/offset should be per grid really
	glm::vec3 tileSpace = (worldspace - m_wsGridOffset) / glm::vec3(m_wsGridScale.x, 1.0f, m_wsGridScale.y);
	if (tileSpace.x < 0.0f || tileSpace.x >= grid.GetDimensions().x || tileSpace.z < 0.0f || tileSpace.z >= grid.GetDimensions().y)
	{
		return {};
	}
	return glm::uvec2(tileSpace.x, tileSpace.z);
}
