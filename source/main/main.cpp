#include "core/platform.h"
#include "engine/engine_startup.h"
#include "engine/frame_graph.h"
#include "editor/systems/editor_system.h"

std::string FullCmdLine(int argc, char** args)
{
	std::string fullCmdLine = "";
	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			fullCmdLine += args[i];
		}
	}
	return fullCmdLine;
}

int main(int argc, char** args)
{
	std::string fullCmdLine = FullCmdLine(argc, args);

	bool runEditor = fullCmdLine.find("-editor") != std::string::npos;
	if (runEditor)
	{
		auto registerEditor = []() {
			R3::Systems::GetInstance().RegisterSystem<R3::EditorSystem>();
		};
		auto setupFrameGraph = [](R3::FrameGraph& fg) {
			auto guiUpdateRoot = fg.m_root.FindFirst("Sequence - ImGuiUpdate");
			if (guiUpdateRoot)
			{
				guiUpdateRoot->AddFn("EditorSystem::ShowGui", true);	// push to front of sequence
			}
		};
		return R3::Run(fullCmdLine, registerEditor, setupFrameGraph);
	}
	else
	{
		return R3::Run(fullCmdLine);
	}
}
