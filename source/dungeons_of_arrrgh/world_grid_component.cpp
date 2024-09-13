#include "world_grid_component.h"
#include "engine/systems/lua_system.h"
#include "engine/dda.h"
#include <imgui.h>
#include <queue>

void DungeonsWorldGridComponent::WorldTileContents::SerialiseJson(R3::JsonSerialiser& s)
{
	R3_PROF_EVENT();
	s("RawData", m_rawData);
	s("Tags", m_tags);
	s("Entities", m_entitiesInTile);
}

void DungeonsWorldGridComponent::RegisterScripts(R3::LuaSystem& l)
{
	R3_PROF_EVENT();
	l.RegisterType<DungeonsWorldGridComponent>("DungeonsWorldGridComponent",
		"FindVisibleTiles", &DungeonsWorldGridComponent::FindVisibleTiles,
		"ResizeGrid", &DungeonsWorldGridComponent::ResizeGrid,
		"GetDimensions", &DungeonsWorldGridComponent::GetDimensions,
		"IsTilePassable", &DungeonsWorldGridComponent::IsTilePassable,
		"Fill", &DungeonsWorldGridComponent::Fill,
		"AllTilesPassable", &DungeonsWorldGridComponent::AllTilesPassable,
		"CalculatePath", &DungeonsWorldGridComponent::CalculatePath,
		"m_debugDraw", &DungeonsWorldGridComponent::m_debugDraw,
		"m_isDirty", &DungeonsWorldGridComponent::m_isDirty
	);

	l.RegisterType<WorldTileContents::TileTags>("TileTagset",
		sol::constructors<WorldTileContents::TileTags(), WorldTileContents::TileTags(std::string_view), WorldTileContents::TileTags(const WorldTileContents::TileTags&)>(),
		"Add", &WorldTileContents::TileTags::Add,
		"Contains", &WorldTileContents::TileTags::Contains
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
	if (auto thisTile = GetContents(startTile.x, startTile.y); thisTile && thisTile->m_flags.m_blockVisibility)
	{
		return results;
	}

	for (auto z = iterStart.y; z < iterEnd.y; ++z)
	{
		for (auto x = iterStart.x; x < iterEnd.x; ++x)
		{
			// find the distance to the center of this cell
			const glm::vec2 cellCenter(x + 0.5f, z + 0.5f);
			const int voxelsPerCell = 1;	// more voxels = more accurate results, but slower
			const glm::vec3 voxelSize(1.0f / (float)voxelsPerCell);
			const float distanceToPoint = glm::ceil(glm::distance(glm::vec2(vCenter), cellCenter));
			const bool inRadius = distanceToPoint <= distance;
			if (inRadius)
			{
				if (auto content = GetContents(x, z); !content->m_flags.m_blockVisibility)
				{
					// use DDA to 'draw' a line from the start to this tile
					// if any vision-blocking tiles are encountered, the current tile is not visible
					glm::vec3 walkTileStart(vCenter.x, 0, vCenter.y);
					glm::vec3 walkTileEnd(cellCenter.x, 0, cellCenter.y);
					auto noBlockVision = [this, voxelsPerCell](const glm::ivec3& tile) {
						if (auto tileData = GetContents(tile.x / voxelsPerCell, tile.z / voxelsPerCell))
						{
							return !tileData->m_flags.m_blockVisibility;
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

void DungeonsWorldGridComponent::Fill(glm::uvec2 start, glm::uvec2 size, const WorldTileContents::TileTags& tags, bool isPassable, bool blockVisibility)
{
	R3_PROF_EVENT();
	glm::uvec2 actualSize = glm::min(start + size, m_gridDimensions);
	for (auto z = start.y; z < actualSize.y; ++z)
	{
		for (auto x = start.x; x < actualSize.x; ++x)
		{
			if (auto tile = GetContents(x, z))
			{
				// should take a tagset instead
				tile->m_tags = tags;
				tile->m_flags.m_passable = isPassable;
				tile->m_flags.m_blockVisibility = blockVisibility;
			}
		}
	}
}

bool DungeonsWorldGridComponent::IsTilePassable(uint32_t tilex, uint32_t tiley)
{
	if (auto c = GetContents(tilex, tiley))
	{
		return c->m_flags.m_passable;
	}
	return false;
}

bool DungeonsWorldGridComponent::AllTilesPassable(glm::uvec2 start, glm::uvec2 size)
{
	R3_PROF_EVENT();
	for (auto z = start.y; z < start.y + size.y; ++z)
	{
		for (auto x = start.x; x < start.x + size.x; ++x)
		{
			if (auto tile = GetContents(x, z))
			{
				if (!tile->m_flags.m_passable)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}

std::vector<R3::Entities::EntityHandle> DungeonsWorldGridComponent::GetEntitiesInTile(uint32_t tileX, uint32_t tileZ)
{
	if (auto contents = GetContents(tileX, tileZ))
	{
		return contents->m_entitiesInTile;
	}
	return {};
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

// basic A*
struct OpenSetEntry
{
	glm::uvec2 m_tile;
	float m_fScore;	// F = gScore[n] + H(n)
	struct Compare {
		bool operator()(const OpenSetEntry& t0, const OpenSetEntry& t1)
		{
			return t0.m_fScore > t1.m_fScore;
		}
	};
};

std::vector<glm::uvec2> DungeonsWorldGridComponent::CalculatePath(glm::uvec2 start, glm::uvec2 end)
{
	R3_PROF_EVENT();
	std::unordered_map<glm::uvec2, glm::uvec2> cameFrom;	// track what the previous node was for any visited node (used to create final path)
	std::unordered_map<glm::uvec2, float> gScore;			// G = cost of the cheapest path from start to another tile
	std::priority_queue<OpenSetEntry, std::vector<OpenSetEntry>, OpenSetEntry::Compare> openSet;	// min-set of fvalues
	auto H = [start,end](const glm::uvec2& n) -> float		// H = heuristic, estimated cost of getting from n to the goal
	{
		// taxicab distance
		return glm::abs((float)end.x - (float)n.x) + glm::abs((float)end.y - (float)n.y);
	};
	auto ReconstructPath = [&](glm::uvec2 current){			// walk backwards following the cheapest connection
		std::vector<glm::uvec2> finalPath;
		finalPath.push_back(end);
		while (1)
		{
			auto fromNode = cameFrom.find(current);
			if (fromNode != cameFrom.end())
			{
				current = fromNode->second;
				finalPath.push_back(current);
			}
			else
			{
				break;
			}
		}
		return finalPath;
	};
	openSet.push({ start, H(start) });		// begin with the first tile
	gScore[start] = 0.0f;					// distance from start
	while (!openSet.empty())
	{
		// find the node with lowest fScore
		glm::uvec2 nextNode = openSet.top().m_tile;
		if (nextNode == end)	// we hit the goal!
		{
			return ReconstructPath(nextNode);
		}
		openSet.pop();
		// now explore neighbours
		auto doNeighbour = [&](int32_t x, int32_t y) {
			if (x >= 0 && y >= 0 && x < (int32_t)m_gridDimensions.x && y < (int32_t)m_gridDimensions.y)
			{
				glm::uvec2 neighbourTile(x, y);
				if (IsTilePassable(x, y))	// we only care about walkable tiles
				{
					float moveCost = 1.0f;
					float newNeighbourG = gScore[nextNode] + moveCost;			// current g + move cost
					auto foundNeighbourG = gScore.find(neighbourTile);
					float currentNeighbourG = foundNeighbourG != gScore.end() ? foundNeighbourG->second : FLT_MAX;	// infinite score if none found
					if (newNeighbourG < currentNeighbourG)	// only continue down this path if it is cheaper than the current one
					{
						float newfScore = newNeighbourG + H(neighbourTile);
						cameFrom[neighbourTile] = nextNode;
						gScore[neighbourTile] = newNeighbourG;
						openSet.push({ neighbourTile, newfScore });
					}
				}
			}
		};
		doNeighbour((int32_t)nextNode.x - 1, (int32_t)nextNode.y);	// left
		doNeighbour((int32_t)nextNode.x + 1, (int32_t)nextNode.y);	// right
		doNeighbour((int32_t)nextNode.x, (int32_t)nextNode.y + 1);	// up
		doNeighbour((int32_t)nextNode.x, (int32_t)nextNode.y - 1);	// down
	}

	return {};	// never reached the goal!
}