#pragma once
#include <vector>
#include <functional>
#include <cassert>

namespace R3
{
	// CallbackFnDecl = usually a std::function or fn ptr
	// Not thread safe by default
	template<class CallbackFnDecl>
	class CallbackArray
	{
	public:
		using Token = uint64_t;		// used to identify + remove callback instances

		Token AddCallback(const CallbackFnDecl& fn, Token oldToken = -1)
		{
			auto foundIt = std::find_if(m_entries.begin(), m_entries.end(), [oldToken](const CallbackEntry& entry) {
				return entry.m_token == oldToken;
			});
			assert(foundIt == m_entries.end());
			if (foundIt == m_entries.end())
			{
				Token newToken = m_nextValidToken++;
				m_entries.push_back({ newToken, fn });
				return newToken;
			}
			else
			{
				return (*foundIt).m_token;
			}
		}
		bool RemoveCallback(Token oldToken)
		{
			auto foundIt = std::find_if(m_entries.begin(), m_entries.end(), [oldToken](const CallbackEntry& entry) {
				return entry.m_token == oldToken;
			});
			assert(foundIt != m_entries.end());
			if (foundIt != m_entries.end())
			{
				m_entries.erase(foundIt);
				return true;
			}
			return false;
		}

		template<typename... Args>
		void Run(Args&&... args)
		{
			for (const auto& it : m_entries)
			{
				it.m_cb(args...);
			}
		}
	private:
		struct CallbackEntry {
			Token m_token;
			CallbackFnDecl m_cb;
		};
		std::vector<CallbackEntry> m_entries;
		Token m_nextValidToken = 0;
	};
}