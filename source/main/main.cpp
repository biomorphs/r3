#include "core/platform.h"
#include "engine/engine_startup.h"
#include "engine/frame_graph.h"
#include "editor/systems/editor_system.h"

#include "dungeons_of_arrrgh/dungeons_of_arrrgh.h"

std::string FullCmdLine(int argc, char** args)
{
	std::string fullCmdLine = "";
	for (int i = 1; i < argc; ++i)
	{
		fullCmdLine += args[i];
	}
	return fullCmdLine;
}

int main(int argc, char** args)
{
	std::string fullCmdLine = FullCmdLine(argc, args);
	bool runEditor = fullCmdLine.find("-editor") != std::string::npos;
	auto registerSystems = [runEditor]() {
		R3::Systems::GetInstance().RegisterSystem<DungeonsOfArrrgh>();
		if (runEditor)
		{
			R3::Systems::GetInstance().RegisterSystem<R3::EditorSystem>();
		}
	};
	auto setupFrameGraph = [runEditor](R3::FrameGraph& fg) {
		auto variableUpdateRoot = fg.m_root.FindFirst("Sequence - VariableUpdate");
		variableUpdateRoot->AddFn("DungeonsOfArrrgh::VariableUpdate");
		auto fixedUpdateRoot = fg.m_root.FindFirst("FixedUpdateSequence - FixedUpdate");
		fixedUpdateRoot->AddFn("DungeonsOfArrrgh::FixedUpdate");
		if (runEditor)
		{
			auto guiUpdateRoot = fg.m_root.FindFirst("Sequence - ImGuiUpdate");
			guiUpdateRoot->AddFn("EditorSystem::ShowGui", true);	// push to front of sequence
		}		
	};
	return R3::Run(fullCmdLine, registerSystems, setupFrameGraph);
}
