#pragma once
#include <unordered_map>

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
		std::unordered_map<uint32_t, float> m_entityIdToWindowHeight;	// to handle arbitrary sized child windows
	};
}