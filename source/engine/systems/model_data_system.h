#pragma once 

#include "engine/systems.h"
#include "engine/model_data.h"
#include "core/mutex.h"

namespace R3
{
	// Handle to model data
	struct ModelDataHandle
	{
		uint32_t m_index = -1;
		static ModelDataHandle Invalid() { return { (uint32_t)-1 }; };
	};

	// Use this to access the actual model data
	class ModelDataValues
	{
	public:
		explicit ModelDataValues(Mutex* m, ModelData* p);
		ModelDataValues() = default;
		ModelDataValues(const ModelDataValues&) = delete;
		ModelDataValues(ModelDataValues&&);
		~ModelDataValues();
		const ModelData* m_data = nullptr;	// NEVER cache/copy this ptr anywhere
	private:
		Mutex* m_valuesMutex = nullptr;	// ensure the model data is locked while we try to access it
	};

	class ModelDataSystem : public System
	{
	public:
		static std::string_view GetName() { return "ModelData"; }
		virtual void RegisterTickFns();
		virtual bool Init();

		ModelDataHandle LoadModel(const char* path, std::function<void(bool, ModelDataHandle)> onFinish = nullptr);
		ModelDataValues GetModelData(const ModelDataHandle& h);

	private:
		bool ShowGui();
		ModelDataHandle FindModel(std::string_view name);	// does not load any new ones
		bool FindOrCreate(std::string_view name, ModelDataHandle& h);	// returns true if an existing handle was found
		struct StoredModel
		{
			std::string m_name;
			std::unique_ptr<ModelData> m_modelData;
			bool m_loadError = false;
		};
		Mutex m_allModelsMutex;
		std::vector<StoredModel> m_allModels;
		std::atomic<int> m_pendingModels = 0;
	};
}