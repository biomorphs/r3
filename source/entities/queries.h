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
	void ForEach(World* w, const It&);

	// It = bool(const EntityHandle& e, ComponentType1& cmp, ComponentType2& cmp)
	template<class ComponentType1, class ComponentType2, class It>
	void ForEach(World* w, const It&);
}
}
}

#include "queries.inl"