#include "mutex.h"
#include <SDL_mutex.h>

namespace R3
{
	Mutex::Mutex()
	{
		m_mutex = SDL_CreateMutex();
	}

	Mutex::Mutex(Mutex&& other) noexcept
	{
		m_mutex = other.m_mutex;
		other.m_mutex = nullptr;
	}

	Mutex::~Mutex()
	{
		if (m_mutex != nullptr)
		{
			SDL_DestroyMutex(static_cast<SDL_mutex*>(m_mutex));
		}
	}

	bool Mutex::TryLock()
	{
		return SDL_TryLockMutex(static_cast<SDL_mutex*>(m_mutex)) != SDL_MUTEX_TIMEDOUT;
	}

	void Mutex::Lock()
	{
		SDL_LockMutex(static_cast<SDL_mutex*>(m_mutex));
	}

	void Mutex::Unlock()
	{
		SDL_UnlockMutex(static_cast<SDL_mutex*>(m_mutex));
	}
}