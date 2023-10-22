#include "imgui_menubar_helper.h"
#include "imgui.h"

namespace R3
{
	void MenuBar::AddItem(std::string name, std::function<void()> onSelected, std::string shortcut)
	{
		MenuItem mi;
		mi.m_label = name;
		mi.m_shortcut = shortcut;
		mi.m_onSelected = onSelected;
		m_menuItems.push_back(mi);
	}

	MenuBar& MenuBar::GetSubmenu(std::string label)
	{
		auto found = std::find_if(m_subMenus.begin(), m_subMenus.end(), [&label](const MenuBar& m) {
			return m.m_label == label;
		});
		if (found != m_subMenus.end())
		{
			return (*found);
		}
		else
		{
			MenuBar newMenu;
			newMenu.m_label = label;
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

	void MenuBar::Display(bool appendToMainMenu)
	{
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