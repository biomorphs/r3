#pragma once

#include "editor/editor_command.h"
#include "entities/entity_handle.h"
#include <vector>

namespace R3
{
	// Converts static mesh components to dynamic or visa-versa
	class WorldEditorWindow;
	class WorldEditorConvertMeshComponentsCmd : public EditorCommand
	{
	public:
		enum ConversionType {
			StaticToDynamic,
			DynamicToStatic
		};

		WorldEditorConvertMeshComponentsCmd(WorldEditorWindow* w, const std::vector<Entities::EntityHandle>& entities, ConversionType type) 
			: m_window(w) 
			, m_targetEntities(entities)
			, m_conversion(type)
		{}
		virtual std::string_view GetName();
		virtual Result Execute();
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo();
		virtual Result Redo();

		std::vector<Entities::EntityHandle> m_targetEntities;
		ConversionType m_conversion = ConversionType::StaticToDynamic;

	private:
		bool ConvertEntity(Entities::EntityHandle e, ConversionType conversion);

		WorldEditorWindow* m_window = nullptr;
		std::vector<Entities::EntityHandle> m_convertedToEntities;	// to undo we need to know which entities had a conversion
	};
}