#pragma once 

#include "engine/systems.h"

namespace R3
{
	class StaticMeshSystem : public System
	{
	public:
		static std::string_view GetName() { return "StaticMeshes"; }
		virtual void RegisterTickFns();
		virtual bool Init();
	private:
		bool ShowGui();
	};
}