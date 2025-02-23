#pragma once
#include "engine/systems.h"

namespace R3
{
	class RenderStatsSystem : public System
	{
	public:
		RenderStatsSystem() = default;
		virtual ~RenderStatsSystem() = default;
		static std::string_view GetName() { return "RenderStats"; }
		virtual void RegisterTickFns();
	private:
		bool ShowGui();
		void ShowVMAStats();
		void ShowBufferPoolStats();
		bool m_displayStats = false;
	};
}