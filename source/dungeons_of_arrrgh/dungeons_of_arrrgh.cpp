#include "dungeons_of_arrrgh.h"
#include "world_grid_component.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/systems/lua_system.h"
#include "engine/components/transform.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "core/profiler.h"
#include "core/random.h"
#include "core/log.h"
#include <sol/sol.hpp>

enum WorldTileType : uint8_t
{
	Empty,
	Wall,
	FloorInterior,
	FloorExterior,
	MaxTypes
};

void DungeonsOfArrrgh::RegisterTickFns()
{
	R3_PROF_EVENT();
	RegisterTick("DungeonsOfArrrgh::Main", [this]() {
		return VariableUpdate();
	});
}

bool DungeonsOfArrrgh::Init()
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	entities->RegisterComponentType<DungeonsWorldGridComponent>(16);	// probably only need 1 per world, but eh

	return true;
}

void DungeonsOfArrrgh::Shutdown()
{
	R3_PROF_EVENT();
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
		{{m_drawGridOffset.x, m_drawGridOffset.y, m_drawGridOffset.z, 1}, edgeColour},
		{{m_drawGridOffset.x + m_drawGridScale.x * dims.x, m_drawGridOffset.y, m_drawGridOffset.z, 1}, edgeColour},
		{{m_drawGridOffset.x, m_drawGridOffset.y, m_drawGridOffset.z + m_drawGridScale.y * dims.y, 1}, edgeColour},
		{{m_drawGridOffset.x + m_drawGridScale.x * dims.x, m_drawGridOffset.y, m_drawGridOffset.z + m_drawGridScale.y * dims.y, 1}, edgeColour},
		{{m_drawGridOffset.x, m_drawGridOffset.y, m_drawGridOffset.z, 1}, edgeColour},
		{{m_drawGridOffset.x, m_drawGridOffset.y, m_drawGridOffset.z + m_drawGridScale.y * dims.y, 1}, edgeColour},
		{{m_drawGridOffset.x + m_drawGridScale.x * dims.x, m_drawGridOffset.y, m_drawGridOffset.z, 1}, edgeColour},
		{{m_drawGridOffset.x + m_drawGridScale.x * dims.x, m_drawGridOffset.y, m_drawGridOffset.z + m_drawGridScale.y * dims.y, 1}, edgeColour},
	};
	imRender->m_imRender->AddLines(outerEdge, 4);
	for (uint32_t z = 0; z < dims.y; ++z)
	{
		for (uint32_t x = 0; x < dims.x; ++x)
		{
			glm::vec3 tileOffset = { (float)x * m_drawGridScale.x, 0.0f, (float)z * m_drawGridScale.y };
			glm::vec3 basePos = m_drawGridOffset + tileOffset;
			auto contents = grid.GetContents(x, z);
			if (contents && contents->m_tileData.m_tileType != WorldTileType::Empty)
			{
				glm::vec4 colour = { 1,0,0,.5 };
				if (contents->m_tileData.m_passable)
				{
					colour = { 0,1,0,.5 };
				}
				outVertices.push_back({ {basePos, 1}, colour });
				outVertices.push_back({ {basePos + glm::vec3(m_drawGridScale.x,0,0), 1}, colour });
				outVertices.push_back({ {basePos + glm::vec3(m_drawGridScale.x,0,m_drawGridScale.y), 1}, colour });
				outVertices.push_back({ {basePos, 1}, colour });
				outVertices.push_back({ {basePos + glm::vec3(m_drawGridScale.x,0,m_drawGridScale.y), 1}, colour } );
				outVertices.push_back({ {basePos + glm::vec3(0,0,m_drawGridScale.y), 1}, colour } );
				if (!contents->m_tileData.m_passable)
				{
					glm::vec3 cubeScale = { m_drawGridScale.x * 0.5f, m_drawBlockerHeight * 0.5f, m_drawGridScale.y * 0.5f };
					glm::mat4 transform = glm::scale(glm::translate(basePos + glm::vec3(cubeScale.x, m_drawBlockerHeight / 2, cubeScale.z)), cubeScale);
					imRender->m_imRender->AddCubeWireframe(transform, colour);
				}
			}
		}
	}
	if (outVertices.size() > 0)
	{
		imRender->m_imRender->AddTriangles(outVertices.data(), (uint32_t)outVertices.size() / 3);
	}
}

void DungeonsOfArrrgh::MoveEntities(const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset)
{
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
				tileToLoad = "arrrgh/tiles/basic_hwall_tile_4x4.scn";
			}
			else if (upWall || downWall)
			{
				tileToLoad = "arrrgh/tiles/basic_vwall_tile_4x4.scn";
			}
		}
		
		break;
	default:
		return;
	}
	outEntities = activeWorld->Import(tileToLoad);
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
					glm::vec3 tileOffset = { (float)x * m_drawGridScale.x, 0.0f, (float)z * m_drawGridScale.y };
					glm::vec3 basePos = m_drawGridOffset + tileOffset;
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

bool DungeonsOfArrrgh::VariableUpdate()
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	if (activeWorld)
	{
		auto forEachGrid = [this](const R3::Entities::EntityHandle& e, DungeonsWorldGridComponent& cmp) {
			if (cmp.m_isDirty)
			{
				OnWorldGridDirty(e, cmp);
				cmp.m_isDirty = false;
			}
			if (cmp.m_debugDraw)
			{
				DebugDrawWorldGrid(cmp);
			}
			return true;
		};
		R3::Entities::Queries::ForEach<DungeonsWorldGridComponent>(activeWorld, forEachGrid);
	}
	return true;
}
