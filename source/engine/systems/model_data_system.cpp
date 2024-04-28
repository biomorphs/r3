#include "model_data_system.h"
#include "engine/async.h"
#include "core/profiler.h"
#include <imgui.h>
#include <format>

namespace R3
{
	void ModelDataSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("ModelData::ShowGui", [this]() {
			return ShowGui();
		});
	}

	bool ModelDataSystem::Init()
	{
		return true;
	}

	bool ModelDataSystem::ShowGui()
	{
		R3_PROF_EVENT();
		ImGui::Begin("ModelData");
		std::string str = std::format("{} models pending", m_pendingModels.load());
		ImGui::Text(str.c_str());

		{
			ScopedLock lockModels(m_allModelsMutex);
			for (const auto& m : m_allModels)
			{
				std::string statusText = "";
				if (m.m_loadState == StoredModel::LoadedState::Loading)
				{
					statusText = "Loading";
				}
				else if (m.m_loadState == StoredModel::LoadedState::LoadFailed)
				{
					statusText = "Failed";
				}
				else if (m.m_loadState == StoredModel::LoadedState::LoadOk)
				{
					statusText = "Loaded";
				}
				std::string txt = std::format("{} - {}", m.m_name, statusText);
				ImGui::Text(txt.c_str());
			}
		}

		ImGui::End();
		return true;
	}

	ModelDataValues ModelDataSystem::GetModelData(const ModelDataHandle& h)
	{
		R3_PROF_EVENT();
		if (h.m_index == -1)
		{
			return {};
		}
		{
			m_allModelsMutex.Lock();	// the ModelDataValues may be responsible for unlocking
			if (h.m_index != -1 && h.m_index < m_allModels.size() && m_allModels[h.m_index].m_modelData != nullptr)
			{
				return ModelDataValues(&m_allModelsMutex, m_allModels[h.m_index].m_modelData.get());	//  mutex is locked + passed via values
			}
			else
			{
				m_allModelsMutex.Unlock();
				return {};
			}
		}
	}

	std::string ModelDataSystem::GetModelName(const ModelDataHandle& h)
	{
		R3_PROF_EVENT();
		std::string modelName = "";
		if (h.m_index != -1)
		{
			ScopedLock lock(m_allModelsMutex);
			if (h.m_index != -1 && h.m_index < m_allModels.size())
			{
				modelName = m_allModels[h.m_index].m_name;
			}
		}
		return modelName;
	}

	ModelDataSystem::ModelLoadedCallbacks::Token ModelDataSystem::RegisterLoadedCallback(const ModelLoadedCallback& fn)
	{
		ScopedLock l(m_loadedCallbacksMutex);
		return m_modelLoadedCbs.AddCallback(fn);
	}

	bool ModelDataSystem::UnregisterLoadedCallback(ModelLoadedCallbacks::Token token)
	{
		ScopedLock l(m_loadedCallbacksMutex);
		return m_modelLoadedCbs.RemoveCallback(token);
	}

	ModelDataHandle ModelDataSystem::LoadModel(const char* path)
	{
		R3_PROF_EVENT();
		ModelDataHandle modelHandle;
		if (FindOrCreate(path, modelHandle))
		{
			return modelHandle;		// a handle alredy exists, it might not be loaded yet but we dont care
		}

		auto loadModelJob = [this, modelHandle, path]()
		{
			assert(m_allModels[modelHandle.m_index].m_loadState == StoredModel::LoadedState::Loading);
			assert(m_allModels[modelHandle.m_index].m_modelData == nullptr);
			auto newData = std::make_unique<ModelData>();
			bool modelLoaded = LoadModelData(path, *newData);
			{
				ScopedLock lock(m_allModelsMutex);
				if (modelLoaded)
				{
					m_allModels[modelHandle.m_index].m_modelData = std::move(newData);
					m_allModels[modelHandle.m_index].m_loadState = StoredModel::LoadedState::LoadOk;
				}
				else
				{
					m_allModels[modelHandle.m_index].m_loadState = StoredModel::LoadedState::LoadFailed;
				}
			}
			{
				ScopedLock cbLock(m_loadedCallbacksMutex);
				m_modelLoadedCbs.Run(modelHandle, modelLoaded);
			}
			m_pendingModels.fetch_add(-1);
		};
		m_pendingModels.fetch_add(1);
		RunAsync(std::move(loadModelJob), JobSystem::ThreadPool::SlowJobs);

		return modelHandle;
	}

	bool ModelDataSystem::FindOrCreate(std::string_view name, ModelDataHandle& h)
	{
		R3_PROF_EVENT();
		ScopedLock lock(m_allModelsMutex);
		for (uint32_t m = 0; m < m_allModels.size(); ++m)
		{
			if (name == m_allModels[m].m_name)
			{
				h = ModelDataHandle(m);
				return true;	// found a handle
			}
		}
		m_allModels.push_back({name.data(), nullptr});
		h = ModelDataHandle{ static_cast<uint32_t>(m_allModels.size() - 1) };
		return false;	// new handle created
	}

	ModelDataHandle ModelDataSystem::FindModel(std::string_view name)
	{
		R3_PROF_EVENT();
		ScopedLock lock(m_allModelsMutex);
		for (uint32_t m=0; m<m_allModels.size(); ++m)
		{
			if (name == m_allModels[m].m_name)
			{
				return ModelDataHandle(m);
			}
		}
		return ModelDataHandle::Invalid();
	}

	ModelDataValues::ModelDataValues(Mutex* m, ModelData* p)
		: m_data(p)
		, m_valuesMutex(m)
	{
	}

	ModelDataValues::ModelDataValues(ModelDataValues&& other)
	{
		m_valuesMutex = other.m_valuesMutex;
		other.m_valuesMutex = nullptr;
		m_data = other.m_data;
		other.m_data = nullptr;
	}

	ModelDataValues::~ModelDataValues()
	{
		if (m_valuesMutex)
		{
			m_valuesMutex->Unlock();
		}
	}
}