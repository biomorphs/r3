#include "imgui_menubar_helper.h"
#include "core/profiler.h"
#include "imgui.h"

namespace R3
{
	MenuBar& MenuBar::MainMenu()
	{
		static MenuBar s_mainMenu;
		return s_mainMenu;
	}

	void MenuBar::AddItemAfter(std::string_view afterItemName, std::string_view itemName, std::function<void()> onSelected, std::string shortcut, bool enabled)
	{
		auto foundAfter = std::find_if(m_menuItems.begin(), m_menuItems.end(), [&afterItemName](const MenuItem& m) {
			return m.m_label == afterItemName;
		});
		if (foundAfter != m_menuItems.end())
		{
			foundAfter++;
		}

		MenuItem mi;
		mi.m_label = itemName;
		mi.m_shortcut = shortcut;
		mi.m_onSelected = onSelected;
		mi.m_enabled = enabled;
		m_menuItems.insert(foundAfter, mi);
	}

	void MenuBar::AddItem(std::string_view name, std::function<void()> onSelected, std::string shortcut, bool enabled)
	{
		MenuItem mi;
		mi.m_label = name;
		mi.m_shortcut = shortcut;
		mi.m_onSelected = onSelected;
		mi.m_enabled = enabled;
		m_menuItems.push_back(mi);
	}

	MenuBar& MenuBar::GetSubmenu(std::string_view label, bool enabled)
	{
		auto found = std::find_if(m_subMenus.begin(), m_subMenus.end(), [&label](const MenuBar& m) {
			return m.m_label == label;
		});
		if (found != m_subMenus.end())
		{
			found->m_enabled = enabled;
			return (*found);
		}
		else
		{
			MenuBar newMenu;
			newMenu.m_label = label;
			newMenu.m_enabled = enabled;
			m_subMenus.push_back(newMenu);
			return m_subMenus.back();
		}
	}

	void DoSubMenu(const MenuBar& b)
	{
		for (auto& item : b.m_menuItems)
		{
			if (ImGui::MenuItem(item.m_label.c_str(), item.m_shortcut.c_str(), item.m_selected, item.m_enabled))
			{
				item.m_onSelected();
			}
		}
		for (auto& submenu : b.m_subMenus)
		{
			if (ImGui::BeginMenu(submenu.m_label.c_str(), submenu.m_enabled))
			{
				DoSubMenu(submenu);
				ImGui::EndMenu();
			}
		}
	}

	void MenuBar::DisplayContextMenu(bool isGlobal, const char* id)
	{
		bool isOpen = isGlobal ? ImGui::BeginPopupContextVoid(id) : ImGui::BeginPopupContextItem(id);
		if(isOpen)
		{
			DoSubMenu(*this);
			ImGui::EndPopup();
		}
	}

	void MenuBar::Display(bool appendToMainMenu)
	{
		R3_PROF_EVENT();
		if (appendToMainMenu)
		{
			if (ImGui::BeginMainMenuBar())
			{
				DoSubMenu(*this);
				ImGui::EndMainMenuBar();
			}
		}
		else
		{
			if (ImGui::BeginMenuBar())
			{
				DoSubMenu(*this);
				ImGui::EndMenuBar();
			}
		}
	}

}