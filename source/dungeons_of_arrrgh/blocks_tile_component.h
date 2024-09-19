#pragma once 

#pragma once

#include "entities/component_helpers.h"

// any actor that contains this component will set its parent tile to be blocked for movement
// attach it to anything that should block movement
class DungeonsBlocksTileComponent
{
public:
	static std::string_view GetTypeName() { return "Dungeons_BlocksTile"; }
	static void RegisterScripts(R3::LuaSystem&);
	void SerialiseJson(R3::JsonSerialiser& s);
};