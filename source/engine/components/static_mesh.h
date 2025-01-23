#pragma once
#include "entities/component_helpers.h"
#include "engine/systems/model_data_system.h"

namespace R3
{
	struct ModelDataHandle;
	class StaticMeshComponent
	{
	public:
		StaticMeshComponent();
		~StaticMeshComponent();
		static std::string_view GetTypeName() { return "StaticMesh"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		void SetModelFromPath(std::string_view path);
		void SetModelHandle(ModelDataHandle h);
		void SetMaterialOverride(Entities::EntityHandle m);
		void SetShouldDraw(bool draw);
		inline ModelDataHandle GetModelHandle() { return m_modelHandle; }
		inline Entities::EntityHandle GetMaterialOverride() { return m_materialOverride; }
		inline bool GetShouldDraw() { return m_shouldDraw; }

	private:
		ModelDataHandle m_modelHandle;
		Entities::EntityHandle m_materialOverride;
		bool m_shouldDraw = true;
	};
}