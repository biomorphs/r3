#pragma once

#include <stdint.h>
#include <functional>

namespace R3
{
	class JsonSerialiser;
	namespace Entities
	{
		class EntityHandle
		{
		public:
			EntityHandle() = default;
			EntityHandle(uint32_t publicID, uint32_t privateIndex) : m_publicID(publicID), m_privateIndex(privateIndex) {}
			uint32_t GetID() const { return m_publicID; }
			uint32_t GetPrivateIndex() const { return m_privateIndex; }
			bool operator==(const EntityHandle& e) const
			{
				return m_publicID == e.m_publicID && m_privateIndex == e.m_privateIndex;
			}

			// We need to remap entity IDs during loading, this allows you to intercept every serialised handle
			using OnLoaded = std::function<void(EntityHandle&)>;
			static void SetOnLoadFinishCallback(OnLoaded fn) { m_onLoadedFn = fn; }
			void SerialiseJson(JsonSerialiser& json);
		private:
			uint32_t m_publicID = -1;
			uint32_t m_privateIndex = -1;
			static OnLoaded m_onLoadedFn;
		};
	}
}