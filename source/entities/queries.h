#pragma once

namespace R3
{
namespace Entities
{
	class World;
namespace Queries
{
	// It = bool(const EntityHandle& e, ComponentType& cmp)
	template<class ComponentType, class It>
	void ForEach(const World* w, const It&);
}
}
}

#include "queries.inl"