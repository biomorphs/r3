#pragma once
#include "engine/value_inspector.h"

namespace R3
{
	class EditorCommandList;
	class UndoRedoInspector : public ValueInspector
	{
	public:
		UndoRedoInspector(EditorCommandList& cmdList) : m_cmds(cmdList) {}
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv);
		virtual bool Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv);
		virtual bool Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv);
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn);
	private:
		EditorCommandList& m_cmds;
	};
}