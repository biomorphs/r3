#include "model_data_handle.h"
#include "engine/serialiser.h"
#include "engine/systems/model_data_system.h"

namespace R3
{
	void ModelDataHandle::SerialiseJson(JsonSerialiser& s)
	{
		auto mm = Systems::GetSystem<ModelDataSystem>();
		if (s.GetMode() == JsonSerialiser::Read)
		{
			std::string path = "";
			s("Path", path);
			if (!path.empty())
			{
				*this = mm->LoadModel(path.c_str());
			}
		}
		else
		{
			std::string modelPath = mm->GetModelName(*this);
			s("Path", modelPath);
		}
	}
}