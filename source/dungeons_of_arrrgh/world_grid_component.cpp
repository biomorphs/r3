#include "world_grid_component.h"
#include "engine/systems/lua_system.h"
#include "engine/dda.h"
#include <imgui.h>

void DungeonsWorldGridComponent::WorldTileContents::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("RawData", m_rawData);
	s("Entities", m_entitiesInTile);
}

void DungeonsWorldGridComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsWorldGridComponent>("DungeonsWorldGridComponent",
		"FindVisibleTiles", &DungeonsWorldGridComponent::FindVisibleTiles,
		"ResizeGrid", &DungeonsWorldGridComponent::ResizeGrid,
		"GetDimensions", &DungeonsWorldGridComponent::GetDimensions,
		"GetTileType", &DungeonsWorldGridComponent::GetTileType,
		"IsTilePassable", &DungeonsWorldGridComponent::IsTilePassable,
		"Fill", &DungeonsWorldGridComponent::Fill,
		"AllTilesMatchType", &DungeonsWorldGridComponent::AllTilesMatchType,
		"m_debugDraw", &DungeonsWorldGridComponent::m_debugDraw,
		"m_isDirty", &DungeonsWorldGridComponent::m_isDirty
	);
}

void DungeonsWorldGridComponent::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("Dimensions", m_gridDimensions);
	s("Contents", m_worldGridContents);
	if (s.GetMode() == R3::JsonSerialiser::Read)
	{
		m_isDirty = true;
	}
}

void DungeonsWorldGridComponent::Inspect(const R3::Entities::EntityHandle& e, R3::Entities::World* w, R3::ValueInspector& i)
{
	R3_PROF_EVENT();
	i.Inspect("Dimensions", m_gridDimensions, R3::InspectProperty(&DungeonsWorldGridComponent::ResizeGrid, e, w), { 0,0 }, {4096, 4096});
	i.Inspect("Debug Draw", m_debugDraw, R3::InspectProperty(&DungeonsWorldGridComponent::m_debugDraw, e, w));
	if (ImGui::Button("Regenerate"))
	{
		m_isDirty = true;
	}
}

DungeonsWorldGridComponent::VisibleTiles DungeonsWorldGridComponent::FindVisibleTiles(glm::ivec2 startTile, uint32_t distance )
{
	R3_PROF_EVENT();
	VisibleTiles results;

	// first find the area surrouding this point to test
	glm::ivec2 iTotalDims(m_gridDimensions);
	glm::ivec2 vStart = glm::clamp(startTile, { 0,0 }, iTotalDims);
	glm::ivec2 iterStart = vStart - glm::ivec2(distance);
	glm::ivec2 iterEnd = vStart + glm::ivec2(distance);
	glm::vec2 vCenter = glm::vec2(vStart) + 0.5f;
	iterStart = glm::clamp(iterStart, { 0,0 }, iTotalDims);
	iterEnd = glm::clamp(iterEnd, { 0,0 }, iTotalDims);

	// if the initial tile blocks visibility, we are done
	if (auto thisTile = GetContents(startTile.x, startTile.y); thisTile && thisTile->m_tileData.m_blockVisibility)
	{
		return results;
	}

	for (auto z = iterStart.y; z < iterEnd.y; ++z)
	{
		for (auto x = iterStart.x; x < iterEnd.x; ++x)
		{
			// find the distance to the center of this cell
			const glm::vec2 cellCenter(x + 0.5f, z + 0.5f);
			const int voxelsPerCell = 2;	// more voxels = more accurate results, but slower
			const glm::vec3 voxelSize(1.0f / (float)voxelsPerCell);
			const float distanceToPoint = glm::ceil(glm::distance(glm::vec2(vCenter), cellCenter));
			const bool inRadius = distanceToPoint <= distance;
			if (inRadius)
			{
				if (auto content = GetContents(x, z); !content->m_tileData.m_blockVisibility)
				{
					// use DDA to 'draw' a line from the start to this tile
					// if any vision-blocking tiles are encountered, the current tile is not visible
					glm::vec3 walkTileStart(vCenter.x, 0, vCenter.y);
					glm::vec3 walkTileEnd(cellCenter.x, 0, cellCenter.y);
					auto noBlockVision = [this, voxelsPerCell](const glm::ivec3& tile) {
						if (auto tileData = GetContents(tile.x / voxelsPerCell, tile.z / voxelsPerCell))
						{
							return !tileData->m_tileData.m_blockVisibility;
						}
						return true;
					};
					auto visionBlocked = R3::DDAIntersect(walkTileStart, walkTileEnd, voxelSize, noBlockVision);
					if (!visionBlocked.has_value())
					{
						results.insert({ x,z });
					}
					else
					{
						results.insert({ visionBlocked->x * voxelSize.x, visionBlocked->z * voxelSize.z });
					}
				}
			}
		}
	}
	return results;
}

void DungeonsWorldGridComponent::Fill(glm::uvec2 start, glm::uvec2 size, uint8_t type, bool isPassable, bool blockVisibility)
{
	R3_PROF_EVENT();
	glm::uvec2 actualSize = glm::min(start + size, m_gridDimensions);
	for (auto z = start.y; z < actualSize.y; ++z)
	{
		for (auto x = start.x; x < actualSize.x; ++x)
		{
			if (auto tile = GetContents(x, z))
			{
				tile->m_tileData.m_tileType = static_cast<uint8_t>(type);
				tile->m_tileData.m_passable = isPassable;
				tile->m_tileData.m_blockVisibility = blockVisibility;
			}
		}
	}
}

uint8_t DungeonsWorldGridComponent::GetTileType(uint32_t tilex, uint32_t tiley)
{
	if (auto c = GetContents(tilex, tiley))
	{
		return c->m_tileData.m_tileType;
	}
	return 0;
}

bool DungeonsWorldGridComponent::IsTilePassable(uint32_t tilex, uint32_t tiley)
{
	if (auto c = GetContents(tilex, tiley))
	{
		return c->m_tileData.m_passable;
	}
	return false;
}

bool DungeonsWorldGridComponent::AllTilesMatchType(glm::uvec2 start, glm::uvec2 size, uint8_t type)
{
	R3_PROF_EVENT();
	for (auto z = start.y; z < start.y + size.y; ++z)
	{
		for (auto x = start.x; x < start.x + size.x; ++x)
		{
			if (auto tile = GetContents(x, z))
			{
				if (tile->m_tileData.m_tileType != static_cast<uint8_t>(type))
				{
					return false;
				}
			}
			else
			{
				return false;	// no tile data = no match?
			}
		}
	}
	return true;
}

void DungeonsWorldGridComponent::ResizeGrid(glm::uvec2 size)
{
	R3_PROF_EVENT();
	if (size != m_gridDimensions)
	{
		m_worldGridContents.clear();
		m_worldGridContents.resize(size.x * size.y);
		m_gridDimensions = size;
		m_isDirty = true;
	}
}

const DungeonsWorldGridComponent::WorldTileContents* DungeonsWorldGridComponent::GetContents(uint32_t tileX, uint32_t tileZ) const
{
	return const_cast<DungeonsWorldGridComponent*>(this)->GetContents(tileX, tileZ);
}

DungeonsWorldGridComponent::WorldTileContents* DungeonsWorldGridComponent::GetContents(uint32_t tileX, uint32_t tileZ)
{
	if (tileX < m_gridDimensions[0] && tileZ < m_gridDimensions[1])
	{
		assert(m_worldGridContents.size() >= (m_gridDimensions[0] * m_gridDimensions[1]));
		return &m_worldGridContents[tileX + (tileZ * m_gridDimensions[0])];
	}
	return nullptr;
}
