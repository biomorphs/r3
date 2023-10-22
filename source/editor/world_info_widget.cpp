#include "world_info_widget.h"
#include "entities/world.h"
#include "core/profiler.h"
#include "external/Fork-awesome/IconsForkAwesome.h"
#include "imgui.h"
#include <format>

namespace R3
{
	void WorldInfoWidget::Update(Entities::World& w, bool embedAsChild)
	{
		R3_PROF_EVENT();
		ImVec2 childSize = { 0,ImGui::GetTextLineHeightWithSpacing() * 3.75f };
		bool isOpen = embedAsChild ? ImGui::BeginChild("##worldInfo", childSize, true)
			: ImGui::Begin((const char*)(ICON_FK_GLOBE " World"));
		if(isOpen)
		{
			std::string txtLine = std::format("World Name: {}", w.GetName());
			ImGui::Text(txtLine.c_str());
			txtLine = std::format("Active Entities: {}", w.GetActiveEntityCount());
			ImGui::Text(txtLine.c_str());
			txtLine = std::format("Pending Delete: {}", w.GetPendingDeleteCount());
			ImGui::Text(txtLine.c_str());
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
}