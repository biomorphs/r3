#include "static_mesh_system.h"
#include "model_data_system.h"
#include "entities/systems/entity_system.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	void StaticMeshSystem::RegisterTickFns()
	{
		RegisterTick("StaticMeshes::ShowGui", [this]() {
			return ShowGui();
		});
	}
	void StaticMeshSystem::Shutdown()
	{
		Systems::GetSystem<ModelDataSystem>()->UnregisterLoadedCallback(m_onModelDataLoadedCbToken);
		m_onModelDataLoadedCbToken = -1;
	}

	bool StaticMeshSystem::ShowGui()
	{
		ImGui::Begin("Static Meshes");
		ImGui::End();
		return true;
	}

	void StaticMeshSystem::OnModelDataLoaded(const ModelDataHandle& handle, bool loaded)
	{
		auto models = Systems::GetSystem<ModelDataSystem>();
		auto loadedModelName = models->GetModelName(handle);
		if (loaded)
		{
			m_loadedModels.enqueue(handle);
		}
		else
		{
			LogError("Failed to load model data '{}'", loadedModelName);
		}
	}

	bool StaticMeshSystem::Init()
	{
		// Register as a listener for any new loaded models
		auto models = Systems::GetSystem<ModelDataSystem>();
		m_onModelDataLoadedCbToken = models->RegisterLoadedCallback([this](const ModelDataHandle& handle, bool loaded) {
			OnModelDataLoaded(handle, loaded);
		});
		return true;
	}
}