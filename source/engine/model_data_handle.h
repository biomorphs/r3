#pragma once 
#include <stdint.h>

namespace R3
{
	// Handle to model data
	struct ModelDataHandle
	{
		uint32_t m_index = -1;
		static ModelDataHandle Invalid() { return { (uint32_t)-1 }; };
		void SerialiseJson(class JsonSerialiser& s);
	};
}