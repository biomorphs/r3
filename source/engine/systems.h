#pragma once

namespace R3
{
	// Base system
	// 3 phase init
	// single phase shutdown
	class System
	{
	public:
		virtual ~System() = default;
		virtual bool PreInit() { return true; }
		virtual bool Initialise() { return true; }
		virtual bool PostInit() { return true; }
		virtual void Shutdown() { }
	};

	// Singleton, owns all systems + knows about frame graph fns
	class Systems
	{
	public:
		static Systems& GetInstance();
		template<class T> void RegisterSystem();

	private:

	};

	template<class T> void RegisterSystem()
	{

	}
}