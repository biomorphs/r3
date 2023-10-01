#pragma once

#include <stdint.h>

namespace R3
{
namespace Entities
{
	class EntityHandle
	{
	public:
		EntityHandle() = default;
		EntityHandle(uint32_t publicID, uint32_t privateIndex) : m_publicID(publicID), m_privateIndex(privateIndex) {}
		uint32_t GetID() const { return m_publicID; }
		uint32_t GetPrivateIndex() const { return m_privateIndex; }
		bool operator==(const EntityHandle& e)
		{
			return m_publicID == e.m_publicID && m_privateIndex == e.m_privateIndex;
		}

	private:
		uint32_t m_publicID = -1;
		uint32_t m_privateIndex = -1;
	};
}
}