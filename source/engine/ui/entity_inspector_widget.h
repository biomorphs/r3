#pragma once
#include <string_view>
#include <unordered_map>
#include <functional>

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
		
		using OnAddComponent = std::function<void(const Entities::EntityHandle& h, std::string_view typeName)>;
		using OnRemoveComponent = std::function<void(const Entities::EntityHandle& h, std::string_view typeName)>;
		using OnSetEntityName = std::function<void(const Entities::EntityHandle& h, std::string_view old, std::string_view newName)>;
		using OnInspectEntity = std::function<void(const Entities::EntityHandle& h, Entities::World& w)>;
		OnAddComponent m_onAddComponent;
		OnRemoveComponent m_onRemoveComponent;
		OnSetEntityName m_onSetEntityName;
		OnInspectEntity m_onInspectEntity;	// called whenever am entity is about to be inspected
	private:
		bool ShowEntityHeader(const Entities::EntityHandle& h, Entities::World& w);
		void UpdateEntityContextMenu(const Entities::EntityHandle& h, Entities::World& w);
		void DisplayComponent(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, int cmpTypeIndex);
		std::unordered_map<uint32_t, float> m_entityIdToWindowHeight;	// to handle arbitrary sized child windows
	};
}