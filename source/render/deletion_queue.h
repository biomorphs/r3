#pragma once
#include <functional>

namespace R3
{
	class DeletionQueue
	{
	public:
		using Deleter = std::function<void()>;
		void PushDeleter(Deleter&& fn);
		void DeleteAll();

	private:
		std::vector<Deleter> m_deleters;
	};
}