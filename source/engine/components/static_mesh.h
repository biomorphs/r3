#pragma once
#include "entities/component_helpers.h"
#include "engine/model_data_handle.h"

namespace R3
{
	struct ModelDataHandle;
	class StaticMeshComponent
	{
	public:
		static std::string_view GetTypeName() { return "Static Mesh"; }
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);
		void SetModelFromPath(const std::string& path);

		ModelDataHandle m_modelHandle;
		Entities::EntityHandle m_materialOverride;
	};
}