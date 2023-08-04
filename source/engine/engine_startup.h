#pragma once
#include <functional>
#include <string_view> 

namespace R3
{
	class FrameGraph;

	using RegisterSystemsFn = std::function<void()>;				// all systems must be registered
	using BuildFrameGraphFn = std::function<void(FrameGraph&)>;	// modify the default frame graph to suit your needs

	// This runs everything. Call it from main after platform::initialise()!
	int Run(std::string_view fullCmdLine, RegisterSystemsFn systemCreationCb = {}, BuildFrameGraphFn frameGraphBuildCb = {});
}