#include "console_log_widget.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	ConsoleLogWidget::ConsoleLogWidget()
	{
		auto logCb = [this](LogType type, std::string_view txt) {
			ScopedLock lock(m_historyMutex);
			m_logHistory.push_back({type, txt.data()});
			if ((type == LogType::Error && m_displayOnError) ||
				(type == LogType::Warning && m_displayOnWarning) ||
				(type == LogType::Info && m_displayOnInfo))
			{
				m_isDisplayed = true;
			}
		};
		m_logCallbackToken = LogRegisterCallback(logCb);
	}

	ConsoleLogWidget::~ConsoleLogWidget()
	{
		LogUnregisterCallback(m_logCallbackToken);
	}

	void ConsoleLogWidget::Update(bool embedAsChild)
	{
		// must match LogType enum in log.h
		static ImVec4 c_logTextColours[] = {
			{1,1,1,1},	// info
			{1,1,0,1},	// warn
			{1,0,0,1}	// error
		};
		if (!m_isDisplayed && !embedAsChild)
		{
			return;
		}
		std::string txt = "Logs/Console";
		bool isOpen = embedAsChild ? ImGui::BeginChild(txt.c_str(), { 0,0 }, true) : ImGui::Begin(txt.c_str(), &m_isDisplayed);
		if (isOpen)
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
			ImGui::BeginChild("LogText", ImVec2(contentRegion.x, contentRegion.y), false, window_flags);
			ScopedLock lock(m_historyMutex);
			for (int i = 0; i < m_logHistory.size(); i++)
			{
				ImGui::TextColored(c_logTextColours[(int32_t)m_logHistory[i].m_type], m_logHistory[i].m_txt.c_str());
			}
			// autoscrolling
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			{
				ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();
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