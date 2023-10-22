#pragma once

namespace R3
{
	namespace Entities
	{
		class World;
	}

	// Displays/allows editing of world info (name, etc)
	class WorldInfoWidget
	{
	public:
		void Update(Entities::World& w);
	};
}