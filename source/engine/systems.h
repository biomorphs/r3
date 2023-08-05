#pragma once

#include <string_view>
#include <functional>
#include <memory>
#include <vector>

namespace R3
{
	class FrameGraph;

	// Base system
	class System
	{
	public:
		virtual ~System() = default;
		virtual void RegisterTickFns() { }			// called after init, register tick fns with Systems
		virtual bool Init() { return true; }		// Called in order of registration!
		virtual void Shutdown() { }					// Called in reverse order to init
	protected:
		template<class T> static T* GetSystem();	// Convenience helpers
		static void RegisterTick(std::string_view name, std::function<bool()> fn);
	};

	// Singleton, owns all systems + knows about frame graph fns
	class Systems
	{
	public:
		static Systems& GetInstance();
		template<class T> void RegisterSystem();
		template<class T> static T* GetSystem();

		void RegisterTick(std::string_view name, std::function<bool()> fn);
		std::function<bool()> GetTick(std::string_view name);

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

	template<class T>
	static T* System::GetSystem()
	{
		auto& s = Systems::GetInstance();
		return s.GetSystem<T>();
	}

	template<class T>
	static T* Systems::GetSystem() 
	{
		auto& s = Systems::GetInstance();
		auto found = std::find_if(s.m_allSystems.begin(), s.m_allSystems.end(), [](const SystemRecord& s) {
			return s.m_name == T::GetName();
		});
		if (found != s.m_allSystems.end())
		{
			return static_cast<T*>(found->m_ptr.get());
		}
		else
		{
			return nullptr;
		}
	};

	template<class T> void Systems::RegisterSystem()
	{
		std::string sysName(T::GetName());
		SystemRecord newSys;
		newSys.m_name = sysName;
		newSys.m_ptr = std::make_unique<T>();
		m_allSystems.push_back(std::move(newSys));
	}
}