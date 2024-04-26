#include "deletion_queue.h"

namespace R3
{
	void DeletionQueue::PushDeleter(Deleter&& fn)
	{
		m_deleters.push_back(fn);
	}

	void DeletionQueue::DeleteAll()
	{
		for (auto it = m_deleters.rbegin(); it != m_deleters.rend(); ++it)
		{
			(*it)();
		}
		m_deleters.clear();
	}
}