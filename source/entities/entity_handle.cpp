#include "entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	namespace Entities
	{
		EntityHandle::OnLoaded EntityHandle::m_onLoadedFn = nullptr;

		void EntityHandle::SerialiseJson(class JsonSerialiser& json)
		{
			json("ID", m_publicID);
			if (json.GetMode() == JsonSerialiser::Read && m_onLoadedFn)
			{
				m_onLoadedFn(*this);
			}
		}
	}
}