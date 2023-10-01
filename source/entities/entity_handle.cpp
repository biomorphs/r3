#include "entity_handle.h"
#include "engine/serialiser.h"

namespace R3
{
	namespace Entities
	{
		void EntityHandle::SerialiseJson(class JsonSerialiser& json)
		{
			json("ID", m_publicID);
		}
	}
}