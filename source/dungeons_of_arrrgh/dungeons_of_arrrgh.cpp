#include "dungeons_of_arrrgh.h"
#include "monster_component.h"
#include "world_grid_component.h"
#include "world_grid_position.h"
#include "vision_component.h"
#include "inspectable_component.h"
#include "blocks_tile_component.h"
#include "base_actor_stats_component.h"
#include "item_component.h"
#include "wearable_item_component.h"
#include "equipped_items_component.h"
#include "inventory_component.h"
#include "consumable_item_component.h"
#include "item_stats_component.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/systems/mesh_renderer.h"
#include "engine/systems/lua_system.h"
#include "engine/systems/input_system.h"
#include "engine/systems/camera_system.h"
#include "engine/components/transform.h"
#include "engine/components/static_mesh.h"
#include "engine/utils/intersection_tests.h"
#include "render/render_system.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "core/profiler.h"
#include "core/log.h"
#include <sol/sol.hpp>

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
	entities->RegisterComponentType<DungeonsWorldGridPosition>(64 * 1024);
	entities->RegisterComponentType<DungeonsMonsterComponent>(2 * 1024);
	entities->RegisterComponentType<DungeonsInspectableComponent>(4 * 1024);
	entities->RegisterComponentType<DungeonsBlocksTileComponent>(8 * 1024);
	entities->RegisterComponentType<DungeonsBaseActorStatsComponent>(2 * 1024);
	entities->RegisterComponentType<DungeonsItemComponent>(4 * 1024);
	entities->RegisterComponentType<DungeonsWearableItemComponent>(4 * 1024);
	entities->RegisterComponentType<DungeonsInventoryComponent>(2 * 1024);
	entities->RegisterComponentType<DungeonsEquippedItemsComponent>(1024);
	entities->RegisterComponentType<DungeonsConsumableItemComponent>(2 * 1024);
	entities->RegisterComponentType<DungeonsItemStatsComponent>(2 * 1024);

	auto scriptNamespace = "Arrrgh";
	auto scripts = GetSystem<R3::LuaSystem>();
	scripts->RegisterFunction("DebugDrawTiles", [this](DungeonsWorldGridComponent* grid, std::vector<glm::uvec2>& tiles) {
		DebugDrawTiles(*grid, tiles);
	}, scriptNamespace);
	scripts->RegisterFunction("MoveEntitiesWorldspace", [this](const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset) {
		MoveEntitiesWorldspace(targets, offset);
	}, scriptNamespace);
	scripts->RegisterFunction("GetTileFromWorldspace", [this](DungeonsWorldGridComponent* grid, glm::vec3 worldspace) {
		return GetTileFromWorldspace(*grid, worldspace);
	}, scriptNamespace);
	scripts->RegisterFunction("GetTileUnderMouseCursor", [this](DungeonsWorldGridComponent* grid) {
		return GetTileUnderMouseCursor(*grid);
	}, scriptNamespace);
	scripts->RegisterFunction("SetEntityTilePosition", [this, entities](DungeonsWorldGridComponent* grid, R3::Entities::EntityHandle e, uint32_t tileX, uint32_t tileZ) {
		SetEntityTilePosition(*grid, *entities->GetActiveWorld(), e, tileX, tileZ);
	}, scriptNamespace);
	scripts->RegisterFunction("GetEntityTilePosition", [this, entities](R3::Entities::EntityHandle e) {
		return GetEntityTilePosition(*entities->GetActiveWorld(), e);
	}, scriptNamespace);
	scripts->RegisterFunction("SetFogOfWarEnabled", [this](bool enabled) {
		m_enableFogOfWar = enabled;
	}, scriptNamespace);
	scripts->RegisterFunction("GetAllEquippedItemStats", [this, entities](R3::Entities::EntityHandle actor) {
		return GetAllEquippedItemStats(*entities->GetActiveWorld(), actor);
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
	glm::vec3 tileOffset = { (float)x * scale.x, 0.2f, (float)z * scale.y };
	glm::vec3 basePos = offset + tileOffset;
	auto contents = grid.GetContents(x, z);
	if (contents)
	{
		glm::vec4 colour = { 1,0,0,.25 };
		if (contents->m_flags.m_passable)
		{
			colour = { 0,1,0,.25 };
		}
		if (contents->m_tags.m_count > 0)
		{
			colour.a = 0.5;
		}
		verts.push_back({ {basePos, 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,0), 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,scale.y), 1}, colour });
		verts.push_back({ {basePos, 1}, colour });
		verts.push_back({ {basePos + glm::vec3(scale.x,0,scale.y), 1}, colour });
		verts.push_back({ {basePos + glm::vec3(0,0,scale.y), 1}, colour });
		if (contents->m_flags.m_blockVisibility)
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

template<class Container>
void DungeonsOfArrrgh::DebugDrawTiles(const DungeonsWorldGridComponent& grid, Container& tiles)
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

std::unordered_map<R3::Tag, int32_t> DungeonsOfArrrgh::GetAllEquippedItemStats(R3::Entities::World& w, R3::Entities::EntityHandle actor)
{
	std::unordered_map<R3::Tag, int32_t> results;
	if (auto equipment = w.GetComponent<DungeonsEquippedItemsComponent>(actor))
	{
		for (const auto& slot : equipment->m_slots)
		{
			if (auto itemStats = w.GetComponent<DungeonsItemStatsComponent>(slot.second))
			{
				for (const auto& stat : itemStats->m_stats)
				{
					results[stat.m_tag] = results[stat.m_tag] + stat.m_value;
				}
			}
		}
		
	}
	return results;
}

std::optional<glm::uvec2> DungeonsOfArrrgh::GetEntityTilePosition(R3::Entities::World& w, R3::Entities::EntityHandle e)
{
	auto tilePosComponent = w.GetComponent<DungeonsWorldGridPosition>(e);
	if (tilePosComponent)
	{
		return tilePosComponent->GetPosition();
	}
	return {};
}

void DungeonsOfArrrgh::SetEntityTilePosition(DungeonsWorldGridComponent& grid, R3::Entities::World& w, R3::Entities::EntityHandle e, uint32_t tileX, uint32_t tileZ)
{
	R3_PROF_EVENT();
	auto tilePosComponent = w.GetComponent<DungeonsWorldGridPosition>(e);
	if (!tilePosComponent && tileX != -1 && tileZ != -1)
	{
		w.AddComponent<DungeonsWorldGridPosition>(e);
		tilePosComponent = w.GetComponent<DungeonsWorldGridPosition>(e);
	}
	if (tilePosComponent && tilePosComponent->GetPosition() != glm::uvec2(tileX,tileZ))
	{
		bool isTileBlocker = w.GetComponent<DungeonsBlocksTileComponent>(e) != nullptr;
		auto oldContents = grid.GetContents(tilePosComponent->GetPosition().x, tilePosComponent->GetPosition().y);
		auto newContents = grid.GetContents(tileX, tileZ);

		// make sure a blocking entity cannot enter a tile that is already blocking
		if (newContents && isTileBlocker && newContents->m_flags.m_passable == false)
		{
			R3::LogError("Error - blocking entity {} entering a blocking tile {},{}", w.GetEntityName(e), tileX, tileZ);
		}

		if (oldContents)	// remove from old tile
		{
			auto found = std::find(oldContents->m_entitiesInTile.begin(), oldContents->m_entitiesInTile.end(), e);
			assert(found != oldContents->m_entitiesInTile.end());
			if (found != oldContents->m_entitiesInTile.end())
			{
				oldContents->m_entitiesInTile.erase(found);
				if (isTileBlocker)
				{
					oldContents->m_flags.m_passable = true;		// no longer blocking this tile
				}
			}
		}
		if (newContents)
		{
			// add to new tile with validation for double-adds
			auto found = std::find(newContents->m_entitiesInTile.begin(), newContents->m_entitiesInTile.end(), e);
			assert(found == newContents->m_entitiesInTile.end());
			if (found == newContents->m_entitiesInTile.end())
			{
				newContents->m_entitiesInTile.push_back(e);
				if (isTileBlocker)
				{
					newContents->m_flags.m_passable = false;	// blocking this tile
				}
			}
		}
		tilePosComponent->m_position = { tileX, tileZ };
	}
}

void DungeonsOfArrrgh::SetVisualEntitiesVisible(const DungeonsWorldGridComponent& grid, R3::Entities::World& w, const std::unordered_set<glm::uvec2>& tiles, bool visible)
{
	R3_PROF_EVENT();
	for (const auto& tile : tiles)
	{
		auto contents = grid.GetContents(tile.x, tile.y);
		if (contents)
		{
			std::vector<R3::Entities::EntityHandle> children;
			w.GetAllChildren(contents->m_visualEntity, children);
			for (const auto& child : children)
			{
				if (auto meshComponent = w.GetComponent<R3::StaticMeshComponent>(child))
				{
					meshComponent->SetShouldDraw(visible);
				}
			}
			for (const auto& actor : contents->m_entitiesInTile)
			{
				std::vector<R3::Entities::EntityHandle> children;
				w.GetAllChildren(actor, children);
				for (const auto& child : children)
				{
					if (auto meshComponent = w.GetComponent<R3::StaticMeshComponent>(child))
					{
						meshComponent->SetShouldDraw(visible);
					}
				}
				if (auto meshComponent = w.GetComponent<R3::StaticMeshComponent>(actor))
				{
					meshComponent->SetShouldDraw(visible);
				}
			}
		}
	}
	GetSystem<R3::MeshRenderer>()->SetStaticsDirty();
}

void DungeonsOfArrrgh::UpdateVision(DungeonsWorldGridComponent& grid, R3::Entities::World& w)
{
	R3_PROF_EVENT();
	auto forEachVision = [&](const R3::Entities::EntityHandle& e, DungeonsVisionComponent& v, R3::TransformComponent& t) {
		if (v.m_needsUpdate)
		{
			v.m_visibleTiles.clear();
			glm::vec3 worldSpacePos(t.GetWorldspaceMatrix(e, w)[3]);	// don't need interpolation, we are in fixed update
			auto tileMaybe = GetTileFromWorldspace(grid, worldSpacePos);	
			if (tileMaybe.has_value() && v.m_visionMaxDistance > 0)
			{
				const uint32_t visionDistance = (uint32_t)ceil(v.m_visionMaxDistance);
				v.m_visibleTiles = grid.FindVisibleTiles(glm::ivec2(tileMaybe.value()), visionDistance);
				if (m_enableFogOfWar && v.m_affectsFogOfWar)
				{
					SetVisualEntitiesVisible(grid, w, v.m_visibleTiles, true);
				}
			}
			v.m_needsUpdate = false;
		}
		return true;
	};
	R3::Entities::Queries::ForEachAsync<DungeonsVisionComponent, R3::TransformComponent>(&w, 1, forEachVision);
}

void DungeonsOfArrrgh::MoveEntitiesWorldspace(const std::vector<R3::Entities::EntityHandle>& targets, glm::vec3 offset)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	for (auto target : targets)
	{
		// only move entities that have no parent transform
		if (auto transCmp = activeWorld->GetComponent<R3::TransformComponent>(target))
		{
			if (!transCmp->IsRelativeToParent())
			{
				auto oldPos = glm::vec3(transCmp->GetWorldspaceMatrix(target, *activeWorld)[3]);
				transCmp->SetPositionWorldSpaceNoInterpolation(target, *activeWorld, oldPos + offset);
			}
		}		
	}
	GetSystem<R3::MeshRenderer>()->SetStaticsDirty();
}

// rules match tags against neighbouring tiles
struct NeighbourTagSet {
	DungeonsWorldGridComponent::WorldTileContents::TileTags m_includes;		// check all of these exist
	DungeonsWorldGridComponent::WorldTileContents::TileTags m_excludes;		// check that none of these exist
};

struct GeneratorRule
{
	using NeighbourTags = std::array<NeighbourTagSet, 9>;
	NeighbourTags m_neighbourTags;	// 3x3 grid of tags to match against
	std::vector<std::string> m_scenesToLoad;	// scenes to instantiate if tags match
	bool m_continue = false;	// if true, will continue evaluating even if this rule is matched (the scene will be loaded)
	float m_yRotation = 0.0f;	// rotate spawned scene entities around y axis
	GeneratorRule() {}
	GeneratorRule(std::string_view tag, std::string_view scene, float rot = 0.0f)		// single tag = match one tile only
	{
		SetTagsFor(4, tag);	// neighbour 4 = center tile
		m_scenesToLoad.push_back(std::string(scene));
		m_yRotation = rot;
	}
	GeneratorRule(const std::array<std::string_view, 9>& tags, std::string_view scene, float rot = 0.0f)
	{
		for (int i = 0; i < 9; ++i)
		{
			SetTagsFor(i, tags[i]);
		}
		m_scenesToLoad.push_back(std::string(scene));
		m_yRotation = rot;
	}
	void SetTagsFor(int neighbour, std::string_view tagStr)
	{
		// parse to find includes/excludes, needs to do the same thing as tagstr
		std::string tagStrCopy(tagStr);		// strip whitespace
		//tagStrCopy.erase(std::remove_if(tagStrCopy.begin(), tagStrCopy.end(), std::isspace), tagStrCopy.end());
		size_t firstChar = 0;
		while (firstChar < tagStrCopy.length())
		{
			size_t nextSeparator = std::string_view(tagStrCopy.data() + firstChar).find_first_of(',');
			if (nextSeparator == std::string_view::npos)
			{
				nextSeparator = tagStrCopy.length() - firstChar;
			}
			auto thisTag = std::string_view(tagStrCopy).substr(firstChar, nextSeparator);
			if (thisTag.length() > 0)
			{
				if (thisTag[0] == '!')
				{
					m_neighbourTags[neighbour].m_excludes.Add(std::string_view(thisTag).substr(1));	// skip ! character
				}
				else
				{
					m_neighbourTags[neighbour].m_includes.Add(thisTag);
				}
			}
			firstChar += nextSeparator + 1;
		}
	}
};

// !tag = must not contain this tag 
// empty string = dont care about contents
const std::vector<GeneratorRule> c_wallRules = {
	GeneratorRule(		// if walls on all sides, empty tile (to avoid huge fields of walls)
		{
			"wall",		"wall",			"wall",
			"wall",		"wall",			"wall",
			"wall",		"wall",			"wall"
		}, ""
	),
	GeneratorRule(		// corner
		{
			"",			"wall",			"",
			"wall",		"wall",			"!wall",
			"",			"!wall",		""
		},  "basic_wall_corner_4x4.scn", 180.0f
	),
	GeneratorRule(		// corner
		{
			"",			"wall",			"",
			"!wall",	"wall",		"wall",
			"",			"!wall",		""
		},  "basic_wall_corner_4x4.scn", 90.0f
	),
	GeneratorRule(		// corner
		{
			"",			"!wall",			"",
			"wall",		"wall",				"!wall",
			"",			"wall",				""
		},  "basic_wall_corner_4x4.scn", -90.0f
	),
	GeneratorRule(		// corner
		{
			"",			"!wall",			"",
			"!wall",	"wall",				"wall",
			"",			"wall",				""
		},  "basic_wall_corner_4x4.scn", 0.0f
	),
	GeneratorRule(
		{
			"",		"wall",			"",
			"!wall","wall",			"",
			"",		"wall",			""
		}, "basic_vwall_tile_4x4.scn"
	),
	GeneratorRule(
		{
			"",		"wall",			"",
			"",		"wall",			"!wall",
			"",		"wall",			""
		}, "basic_vwall_tile_4x4.scn"
	),
	GeneratorRule(
		{
			"",		"!wall",	"",
			"wall",	"wall",		"wall",
			"",		"",			""
		}, "basic_hwall_tile_4x4.scn"
	),
	GeneratorRule(
		{
			"",		"",			"",
			"wall",	"wall",		"wall",
			"",		"!wall",	""
		}, "basic_hwall_tile_4x4.scn"
	),
	GeneratorRule("wall", "basic_crosswall_tile_4x4.scn")
};

const std::vector<GeneratorRule> c_floorRules = {
	GeneratorRule(
		{
			"wall",		"wall",			"wall",
			"wall",		"wall",			"wall",
			"wall",		"wall",			"wall"
		}, ""
	),
	GeneratorRule("floor,carpet", "basic_floor_carpet_tile_4x4.scn"),
	GeneratorRule("floor,interior", "basic_floor_wood_4x4.scn"),
	GeneratorRule("floor,exterior", "basic_floor_tile_4x4.scn"),
	GeneratorRule("floor", "basic_floor_dirt_4x4.scn")
};

struct TileToLoad {
	std::string m_path;
	float m_yRotation = 0.0f;
};

// returns list of tiles to load
void EvaluateRules(DungeonsWorldGridComponent& grid, int32_t tileX, int32_t tileZ, const std::vector<GeneratorRule>& rules,std::vector<TileToLoad>& results)
{
	for (int r = 0; r < rules.size(); ++r)
	{
		bool ruleMatches = true;
		for (int32_t z = 0; z < 3 && ruleMatches; ++z)
		{
			for (int32_t x = 0; x < 3 && ruleMatches; ++x)
			{
				const auto& toLookFor = rules[r].m_neighbourTags[x + (z * 3)];
				if (toLookFor.m_includes.m_count > 0 || toLookFor.m_excludes.m_count > 0)
				{
					auto contents = grid.GetContents(tileX + x - 1, tileZ + z - 1);
					if (contents)
					{
						bool allIncludesValid = contents->m_tags.ContainsAll(toLookFor.m_includes);
						bool allExcludesValid = !contents->m_tags.ContainsAny(toLookFor.m_excludes);
						ruleMatches &= allIncludesValid && allExcludesValid;
					}
					else
					{
						// dont match if we contain rules for an empty tile
						ruleMatches = false;
					}
				}
			}
		}
		if (ruleMatches)
		{
			for (int i = 0; i < rules[r].m_scenesToLoad.size(); ++i)
			{
				if (rules[r].m_scenesToLoad[i].length() > 0)	// rules can have empty string to denote empty tiles
				{
					results.push_back({ rules[r].m_scenesToLoad[i], rules[r].m_yRotation });
				}
			}
			if (!rules[r].m_continue)
			{
				break;
			}
		}
	}
}

void DungeonsOfArrrgh::GenerateTileVisuals(uint32_t x, uint32_t z, DungeonsWorldGridComponent& grid)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();

	std::vector<TileToLoad> tilesToLoad;
	EvaluateRules(grid, x, z, c_wallRules, tilesToLoad);
	EvaluateRules(grid, x, z, c_floorRules, tilesToLoad);
	if (tilesToLoad.size() > 0)
	{
		auto newChild = activeWorld->AddEntity();	// root node for tile visuals
		grid.GetContents(x, z)->m_visualEntity = newChild;
		activeWorld->SetEntityName(newChild, std::format("Tile {},{}", x, z));
		for (const auto& tileScene : tilesToLoad)
		{
			auto foundInCache = m_generateVisualsEntityCache.find(tileScene.m_path);
			if (foundInCache == m_generateVisualsEntityCache.end())
			{
				// load the entities via the world, serialise them, then delete the temp loaded entities + store the json
				std::string fullPath = std::format("arrrgh/tiles/{}", tileScene.m_path);
				auto loadedEntities = activeWorld->Import(fullPath);
				R3::JsonSerialiser entityData = activeWorld->SerialiseEntities(loadedEntities);
				m_generateVisualsEntityCache[tileScene.m_path] = entityData.GetJson();
				foundInCache = m_generateVisualsEntityCache.find(tileScene.m_path);
				for (auto it : loadedEntities)
				{
					activeWorld->RemoveEntity(it);
				}
			}
			if (foundInCache != m_generateVisualsEntityCache.end())
			{
				R3::JsonSerialiser entityData(R3::JsonSerialiser::Read, foundInCache->second);
				auto clonedEntities = activeWorld->SerialiseEntities(entityData);	// clone via serialisation
				if (clonedEntities.size() > 0)
				{
					glm::vec3 tileOffset = { (float)x * m_wsGridScale.x, 0.0f, (float)z * m_wsGridScale.y };
					glm::vec3 basePos = m_wsGridOffset + tileOffset;
					for (auto impChild : clonedEntities)	// make sure any imported entities have the correct parent and are not visible by default
					{
						if (auto transCmp = activeWorld->GetComponent<R3::TransformComponent>(impChild))	// move + rotate the entities
						{
							if (!transCmp->IsRelativeToParent())
							{
								auto oldPos = glm::vec3(transCmp->GetWorldspaceMatrix(impChild, *activeWorld)[3]);
								transCmp->SetPositionWorldSpaceNoInterpolation(impChild, *activeWorld, oldPos + basePos);
								glm::vec3 rot = transCmp->GetOrientationDegrees() + glm::vec3(0,tileScene.m_yRotation,0);
								transCmp->SetOrientationDegrees(rot);
							}
						}
						if (auto renderComponent = activeWorld->GetComponent<R3::StaticMeshComponent>(impChild))
						{
							renderComponent->SetShouldDraw(!m_enableFogOfWar);
						}
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

void DungeonsOfArrrgh::GenerateWorldVisuals(const R3::Entities::EntityHandle& e, class DungeonsWorldGridComponent& grid)
{
	R3_PROF_EVENT();
	auto entities = GetSystem<R3::Entities::EntitySystem>();
	auto activeWorld = entities->GetActiveWorld();
	const auto dims = grid.GetDimensions();
	for (uint32_t z = 0; z < dims.y; ++z)
	{
		for (uint32_t x = 0; x < dims.x; ++x)
		{
			if (auto tile = grid.GetContents(x, z))
			{
				if (tile->m_tags.m_count > 0)
				{
					GenerateTileVisuals(x, z, grid);
				}
			}
		}
	}
	m_generateVisualsEntityCache.clear();
	GetSystem<R3::MeshRenderer>()->SetStaticsDirty();
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

std::optional<glm::uvec2> DungeonsOfArrrgh::GetTileUnderMouseCursor(DungeonsWorldGridComponent& grid)
{
	float rayDistance = 100000.0f;
	auto input = GetSystem<R3::InputSystem>();
	const auto& mainCam = GetSystem<R3::CameraSystem>()->GetMainCamera();
	const glm::vec2 windowExtents = GetSystem<R3::RenderSystem>()->GetWindowExtents();
	const glm::vec2 cursorPos(input->GetMouseState().m_cursorX, input->GetMouseState().m_cursorY);
	glm::vec3 mouseWorldSpace = mainCam.WindowPositionToWorldSpace(cursorPos, windowExtents);
	const glm::vec3 lookDirWorldspace = glm::normalize(mouseWorldSpace - mainCam.Position());
	glm::vec3 rayStart = mainCam.Position();
	glm::vec3 rayEnd = mainCam.Position() + lookDirWorldspace * rayDistance;

	// now calculate if/where the ray passes through y=0
	float hitPoint = 0;
	if (R3::RayIntersectsPlane(rayStart, rayEnd, { 0,0,0 }, { 0,1,0 }, hitPoint))
	{
		glm::vec3 worldPos = rayStart + glm::normalize(rayEnd - rayStart) * hitPoint;
		return GetTileFromWorldspace(grid, worldPos);
	}

	return std::optional<glm::uvec2>();
}
