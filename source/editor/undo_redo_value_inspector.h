#pragma once
#include "engine/value_inspector.h"

namespace R3
{
	class EditorCommandList;
	class UndoRedoInspector : public ValueInspector
	{
	public:
		UndoRedoInspector(EditorCommandList& cmdList) : m_cmds(cmdList) {}
		virtual bool Inspect(std::string label, Tag currentValue, std::function<void(Tag)> setFn);
		virtual bool Inspect(std::string label, bool currentValue, std::function<void(bool)> setFn);
		virtual bool Inspect(std::string_view label, std::string_view currentValue, std::function<void(std::string)> setFn);
		virtual bool Inspect(std::string_view label, int currentValue, std::function<void(int)> setFn, int step, int minv, int maxv);
		virtual bool Inspect(std::string_view label, float currentValue, std::function<void(float)> setFn, float step, float minv, float maxv);
		virtual bool Inspect(std::string_view label, glm::uvec2 currentValue, std::function<void(glm::uvec2)> setFn, glm::uvec2 minv, glm::uvec2 maxv);
		virtual bool Inspect(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn, glm::vec3 minv, glm::vec3 maxv);
		virtual bool Inspect(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn, glm::vec4 minv, glm::vec4 maxv);
		virtual bool InspectColour(std::string_view label, glm::vec4 currentValue, std::function<void(glm::vec4)> setFn);
		virtual bool InspectColour(std::string_view label, glm::vec3 currentValue, std::function<void(glm::vec3)> setFn);
		virtual bool InspectFile(std::string_view label, std::string_view path, std::string_view filter, std::function<void(std::string_view)> setFn);
		virtual bool InspectEntity(std::string_view label, Entities::EntityHandle current, Entities::World* w, std::function<void(Entities::EntityHandle)> setFn);
		virtual bool InspectTexture(std::string_view label, TextureHandle current, std::function<void(TextureHandle)> setFn, glm::ivec2 dims);
		virtual bool InspectEnum(std::string_view label, int currentValue, std::function<void(int)> setFn, const std::string_view options[], int optionCount);
	private:
		EditorCommandList& m_cmds;
		bool m_entitySelectorOpen = false;
	};
}