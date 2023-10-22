#include "world_info_widget.h"
#include "entities/world.h"
#include "core/profiler.h"
#include "external/Fork-awesome/IconsForkAwesome.h"
#include "imgui.h"
#include <format>

namespace R3
{
	void WorldInfoWidget::Update(Entities::World& w)
	{
		R3_PROF_EVENT();
		if (ImGui::Begin((const char*)(ICON_FK_GLOBE " World")))
		{
			std::string txtLine = std::format("World Name: {}", w.GetName());
			ImGui::Text(txtLine.c_str());
			txtLine = std::format("Active Entities: {}", w.GetActiveEntityCount());
			ImGui::Text(txtLine.c_str());
			txtLine = std::format("Pending Delete: {}", w.GetPendingDeleteCount());
			ImGui::Text(txtLine.c_str());
			ImGui::End();
		}
	}
}