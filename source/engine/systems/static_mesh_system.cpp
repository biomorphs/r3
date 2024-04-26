#include "static_mesh_system.h"
#include "model_data_system.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	ModelDataHandle sponzaModel;

	void StaticMeshSystem::RegisterTickFns()
	{
		RegisterTick("StaticMeshes::ShowGui", [this]() {
			return ShowGui();
		});
	}

	bool StaticMeshSystem::ShowGui()
	{
		ImGui::Begin("Static Meshes");

		auto values = Systems::GetSystem<ModelDataSystem>()->GetModelData(sponzaModel);
		if (values.m_data)
		{
			ImGui::Text("Loaded!");
		}
		else
		{
			ImGui::Text("Not Loaded!");
		}
		ImGui::End();

		return true;
	}

	bool StaticMeshSystem::Init()
	{
		auto onSponzaLoaded = [](bool loaded, ModelDataHandle h) {
			LogInfo("Sponza loaded {} with handle {}", loaded ? "ok" : "not ok", h.m_index);
		};

		sponzaModel = Systems::GetSystem<ModelDataSystem>()->LoadModel("sponza\\NewSponza_Main_glTF_002.gltf", onSponzaLoaded);
		return true;
	}
}