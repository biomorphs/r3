#pragma once
#include <string_view>
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
		bool ShowEntityHeader(std::string_view name, const Entities::EntityHandle& h, Entities::World& w);
		void UpdateEntityContextMenu(std::string_view name, const Entities::EntityHandle& h, Entities::World& w);
		void DisplayComponent(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, int cmpTypeIndex);
		std::unordered_map<uint32_t, float> m_entityIdToWindowHeight;	// to handle arbitrary sized child windows
	};
}