#pragma once

#include <string_view>
#include <functional>
#include <memory>
#include <vector>

namespace R3
{
	class FrameGraph;

	// Base system
	// 3 phase init
	// single phase shutdown
	class System
	{
	public:
		virtual ~System() = default;
		virtual void RegisterTickFns() { }		// called after init
		virtual bool Init() { return true; }	// Called in order of registration!
		virtual void Shutdown() { }				// Called in reverse order to init
	};

	// Singleton, owns all systems + knows about frame graph fns
	class Systems
	{
	public:
		static Systems& GetInstance();
		template<class T> void RegisterSystem();
		void RegisterTick(std::string_view name, std::function<bool()> fn);

		bool Initialise();	// call after all systems registered
		void Shutdown();

	private:
		struct SystemRecord {
			std::string m_name;
			std::unique_ptr<System> m_ptr;
		};
		struct TickFnRecord {
			std::string m_name;
			std::function<bool()> m_fn;
		};
		std::vector<SystemRecord> m_allSystems;
		std::vector<TickFnRecord> m_allTicks;
	};

	template<class T> void Systems::RegisterSystem()
	{
		std::string sysName = T::GetName();
		SystemRecord newSys
		newSys.m_name = sysName;
		newSys.m_ptr = std::make_unique<t>();
		m_allSystems.push_back(std::move(newSys));
	}
}