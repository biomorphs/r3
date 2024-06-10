#pragma once
#include "entities/component_helpers.h"
#include "engine/model_data_handle.h"

namespace R3
{
	struct ModelDataHandle;
	class StaticMeshComponent
	{
	public:
		static std::string_view GetTypeName() { return "StaticMesh"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);
		void SetModelFromPath(std::string_view path);
		void SetMaterialOverride(Entities::EntityHandle m) { m_materialOverride = m; }

		ModelDataHandle m_modelHandle;
		Entities::EntityHandle m_materialOverride;
	};
}