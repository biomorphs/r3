#include "core/platform.h"
#include "engine/engine_startup.h"

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
	return R3::Run(fullCmdLine);
}
