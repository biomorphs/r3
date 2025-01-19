#pragma once 

#include "engine/systems.h"
#include "engine/assets/model_data.h"
#include "core/callback_array.h"
#include "core/mutex.h"

namespace R3
{
	// Handle to model data
	struct ModelDataHandle
	{
		uint32_t m_index = -1;
		static ModelDataHandle Invalid() { return { (uint32_t)-1 }; };
		void SerialiseJson(class JsonSerialiser& s);
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

	// Loads model data async, fires callbacks to listeners when they are ready to use
	class ModelDataSystem : public System
	{
	public:
		static std::string_view GetName() { return "ModelData"; }
		virtual void RegisterTickFns();

		using ModelLoadedCallback = std::function<void(const ModelDataHandle&, bool)>;
		using ModelLoadedCallbacks = CallbackArray<ModelLoadedCallback>;
		ModelLoadedCallbacks::Token RegisterLoadedCallback(const ModelLoadedCallback& fn);
		bool UnregisterLoadedCallback(ModelLoadedCallbacks::Token token);

		ModelDataHandle LoadModel(const char* path);
		ModelDataValues GetModelData(const ModelDataHandle& h);
		std::string GetModelName(const ModelDataHandle& h);

	private:
		bool ShowGui();
		bool LoadModelInternal(std::string_view path, ModelData& result, ProgressCb progCb);
		ModelDataHandle FindModel(std::string_view name);	// does not load any new ones
		bool FindOrCreate(std::string_view name, ModelDataHandle& h);	// returns true if an existing handle was found
		struct StoredModel
		{
			enum class LoadedState
			{
				Loading,
				LoadFailed,
				LoadOk
			};
			std::string m_name;
			std::unique_ptr<ModelData> m_modelData;
			LoadedState m_loadState = LoadedState::Loading;
			uint8_t m_loadProgress = 0;
		};
		bool m_showGui = false;
		bool m_loadBakedModels = true;
		Mutex m_allModelsMutex;
		std::vector<StoredModel> m_allModels;
		std::atomic<int> m_pendingModels = 0;
		Mutex m_loadedCallbacksMutex;
		ModelLoadedCallbacks m_modelLoadedCbs;
	};
}