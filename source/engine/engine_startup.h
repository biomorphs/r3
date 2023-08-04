#pragma once
#include <functional>

namespace R3
{
	class FrameGraph;

	using RegisterSystems = std::function<void()>;				// all systems must be registered
	using BuildFrameGraph = std::function<void(FrameGraph&)>;	// modify the default frame graph to suit your needs

	// This runs everything. Call it from main after platform::initialise()!
	int Run(std::string_view fullCmdLine, RegisterSystems systemCreationCb = {}, BuildFrameGraph frameGraphBuildCb = {});
}