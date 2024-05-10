#pragma once
#include "engine/systems.h"

namespace R3
{
	class TransformSystem : public System
	{
	public:
		static std::string_view GetName() { return "Transforms"; }
		virtual void RegisterTickFns();

	private:
		bool OnFixedUpdate();
	};
}