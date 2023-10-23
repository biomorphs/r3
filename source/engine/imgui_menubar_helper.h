#pragma once

#include <vector>
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

	class MenuBar	// contains a heirarchy of MenuBar children
	{
	public:
		static MenuBar& MainMenu();	// main menu singleton for convenience

		void AddItemAfter(std::string_view afterItemName, std::string_view itemName, std::function<void()> onSelected, std::string shortcut = "");
		void AddItemBefore(std::string_view beforeItemName, std::string_view itemName, std::function<void()> onSelected, std::string shortcut = "");
		void AddItem(std::string_view name, std::function<void()> onSelected, std::string shortcut = "");
		
		MenuBar& GetSubmenu(std::string_view label);		// creates one if it doesn't exist
		void Display(bool appendToMainMenu=false);	// menu can be attached to current window or main menu, item callbacks will fire here on selection

		std::string m_label;
		bool m_enabled = true;
		std::vector<MenuItem> m_menuItems;
		std::vector<MenuBar> m_subMenus;
	};
}