#pragma once

namespace R3
{
	class Mutex
	{
	public:
		Mutex();
		Mutex(const Mutex& other) = delete;
		Mutex(Mutex&& other) noexcept;
		~Mutex();

		bool TryLock();
		void Lock();
		void Unlock();

	private:
		void* m_mutex;
	};

	class ScopedLock
	{
	public:
		explicit ScopedLock(Mutex& target) : m_mutex(target) { m_mutex.Lock(); }
		ScopedLock(const ScopedLock& other) = delete;
		~ScopedLock() { m_mutex.Unlock(); }
	private:
		Mutex& m_mutex;
	};

	class ScopedTryLock
	{
	public:
		explicit ScopedTryLock(Mutex& target) : m_mutex(target) { m_locked = m_mutex.TryLock(); }
		ScopedTryLock(const ScopedTryLock& other) = delete;
		~ScopedTryLock() { if (m_locked) { m_mutex.Unlock(); } }
		bool IsLocked() { return m_locked; }
	private:
		Mutex& m_mutex;
		bool m_locked = false;
	};
}