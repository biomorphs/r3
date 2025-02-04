#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <functional>

// Helper for dealing with window menu bars
namespace R3
{
	class MenuItem
	{
	public:
		std::string m_label;
		std::string m_shortcut;
		std::function<void()> m_onSelected;
		bool m_selected = false;
		bool m_enabled = true;
	};

	class MenuBar	// contains a heirarchy of MenuBar children, can be used either as a window menu or as a context menu (i.e. right click)
	{
	public:
		static MenuBar& MainMenu();	// main menu singleton for convenience

		void AddItemAfter(std::string_view afterItemName, std::string_view itemName, std::function<void()> onSelected, std::string shortcut = "", bool enabled = true);
		void AddItem(std::string_view name, std::function<void()> onSelected, std::string shortcut = "", bool enabled = true);
		
		MenuBar& GetSubmenu(std::string_view label, bool enabled = true);	// creates one if it doesn't exist, enabled flag will ovewrite old value
		void Display(bool appendToMainMenu=false);			// menu can be attached to current window or main menu, item callbacks will fire here on selection

		// isGlobal = attach to 'void', otherwise attach to previous imgui widget
		// attachID = attach the menu to a previous widget
		void DisplayContextMenu(bool isGlobal = true, const char* attachID="");		

		std::string m_label;
		bool m_enabled = true;
		std::vector<MenuItem> m_menuItems;
		std::vector<MenuBar> m_subMenus;
	};
}