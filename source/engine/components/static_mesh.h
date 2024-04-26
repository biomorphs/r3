#pragma once
#include "entities/component_helpers.h"

namespace R3
{
	class StaticMeshComponent
	{
	public:
		static std::string_view GetTypeName() { return "Static Mesh"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

	private:
	};
}