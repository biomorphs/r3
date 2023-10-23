#pragma once

namespace R3
{
	namespace Entities
	{
		class EntityHandle;
		class World;
	}
	class ValueInspector;

	class EntityInspectorWidget
	{
	public:
		void Update(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, bool embedAsChild = false);
	private:
		void DisplayComponent(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, int cmpTypeIndex);
	};
}