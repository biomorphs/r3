#pragma once

namespace R3
{
	class Mutex
	{
	public:
		Mutex();
		Mutex(const Mutex& other) = delete;
		Mutex(Mutex&& other);
		~Mutex();

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
}