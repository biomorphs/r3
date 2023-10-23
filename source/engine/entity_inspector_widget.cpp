#include "entity_inspector_widget.h"
#include "engine/value_inspector.h"
#include "entities/world.h"
#include "entities/entity_handle.h"
#include "entities/component_type_registry.h"
#include "imgui.h"

namespace R3
{
	void EntityInspectorWidget::Update(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, bool embedAsChild)
	{
		std::string entityName = ">Invalid handle!<";
		if (w.IsHandleValid(h))
		{
			entityName = w.GetEntityDisplayName(h);
		}
		bool isOpen = embedAsChild ? ImGui::BeginChild(entityName.c_str(), { 0,0 }, true) : ImGui::Begin(entityName.c_str());
		if (isOpen && w.IsHandleValid(h))
		{
			Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
			for (int cmpTypeIndex = 0; cmpTypeIndex < cti.AllTypes().size(); ++cmpTypeIndex)
			{
				if (w.HasComponent(h, cti.AllTypes()[cmpTypeIndex].m_name))
				{
					DisplayComponent(h, w, v, cmpTypeIndex);
				}
			}
		}
		if (embedAsChild)
		{
			ImGui::EndChild();
		}
		else
		{
			ImGui::End();
		}
	}

	void EntityInspectorWidget::DisplayComponent(const Entities::EntityHandle& h, Entities::World& w, ValueInspector& v, int cmpTypeIndex)
	{
		Entities::ComponentTypeRegistry& cti = Entities::ComponentTypeRegistry::GetInstance();
		auto& cmpTypeData = cti.AllTypes()[cmpTypeIndex];
		if (ImGui::CollapsingHeader(cmpTypeData.m_name.c_str()))
		{
			const auto& inspectFn = cti.AllTypes()[cmpTypeIndex].m_inspectFn;
			if (inspectFn)
			{
				inspectFn(h, w, v);
			}
		}
	}
}