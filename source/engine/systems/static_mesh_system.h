#pragma once 

#include "engine/systems.h"
#include "engine/model_data_handle.h"
#include "entities/entity_handle.h"
#include <concurrentqueue/concurrentqueue.h>

namespace R3
{
	class StaticMeshSystem : public System
	{
	public:
		static std::string_view GetName() { return "StaticMeshes"; }
		virtual void RegisterTickFns();
		virtual bool Init();
		virtual void Shutdown();
	private:
		bool ShowGui();
		void OnModelDataLoaded(const ModelDataHandle& handle, bool loaded);
		uint64_t m_onModelDataLoadedCbToken = -1;
		moodycamel::ConcurrentQueue<ModelDataHandle> m_loadedModels;;
	};
}