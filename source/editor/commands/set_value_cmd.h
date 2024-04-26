#pragma once

#include "editor/editor_command.h"
#include <format>

namespace R3
{
	template<class ValueType>
	class SetValueCommand : public EditorCommand
	{
	public:
		using SetValueFn = std::function<void(ValueType)>;
		SetValueCommand(std::string_view name, const ValueType& currentValue, const ValueType& newValue, SetValueFn setFn)
			: m_currentValue(currentValue)
			, m_newValue(newValue)
			, m_setFn(setFn)
			, m_name(std::format("Set Value '{}'", name))
		{
		}
		virtual std::string_view GetName() 
		{ 
			return m_name;
		}
		virtual Result Execute()
		{
			m_setFn(m_newValue);
			return Result::Succeeded;
		}
		virtual bool CanUndoRedo() { return true; }
		virtual Result Undo()
		{
			m_setFn(m_currentValue);
			return Result::Succeeded;
		}
		virtual Result Redo()
		{
			return Execute();
		}

	private:
		std::string m_name;
		ValueType m_currentValue;
		ValueType m_newValue;
		SetValueFn m_setFn;
	};
}