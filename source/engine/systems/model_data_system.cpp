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
				if (m.m_modelData == nullptr && m.m_loadError == false)
				{
					statusText = "Loading";
				}
				else if (m.m_loadError)
				{
					statusText = "Error";
				}
				else if (m.m_modelData != nullptr)
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

		ModelData* data = nullptr;
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

	ModelDataHandle ModelDataSystem::LoadModel(const char* path, std::function<void(bool, ModelDataHandle)> onFinish)
	{
		R3_PROF_EVENT();
		ModelDataHandle modelHandle;
		if (FindOrCreate(path, modelHandle))
		{
			if (onFinish)
			{
				onFinish(true, modelHandle);
			}
			return modelHandle;
		}

		auto loadModelJob = [this, modelHandle, path, onFinish]()
		{
			auto newData = std::make_unique<ModelData>();
			bool modelLoaded = LoadModelData(path, *newData);
			{
				ScopedLock lock(m_allModelsMutex);
				if (modelLoaded)
				{
					m_allModels[modelHandle.m_index].m_modelData = std::move(newData);
				}
				else
				{
					m_allModels[modelHandle.m_index].m_loadError = true;
				}
			}
			if (onFinish)
			{
				onFinish(modelLoaded, modelHandle);
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