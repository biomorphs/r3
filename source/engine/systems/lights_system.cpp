#include "lights_system.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/components/point_light.h"
#include "engine/components/transform.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "render/render_pass_context.h"
#include "core/log.h"
#include "core/profiler.h"
#include <imgui.h>

namespace R3
{
	void LightsSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();

		RegisterTick("LightsSystem::DrawLightBounds", [this]() {
			return DrawLightBounds();
		});
		RegisterTick("LightsSystem::ShowGui", [this]() {
			return ShowGui();
		});

		// Register render functions
		auto render = Systems::GetSystem<RenderSystem>();
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_allPointlights.Destroy(d);
		});
	}

	VkDeviceAddress LightsSystem::GetPointlightsDeviceAddress()
	{
		return m_allPointlights.GetDataDeviceAddress();
	}

	uint32_t LightsSystem::GetFirstPointlightOffset()
	{
		return m_currentInFrameOffset;
	}

	uint32_t LightsSystem::GetTotalPointlightsThisFrame()
	{
		return m_totalPointlightsThisFrame;
	}

	bool LightsSystem::ShowGui()
	{
		R3_PROF_EVENT();
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		auto& lights = debugMenu.GetSubmenu("Lights");
		lights.AddItem("Show GUI", [this]() {
			m_showGui = !m_showGui;
		});
		lights.AddItem("Draw Bounds", [this]() {
			m_drawBounds = !m_drawBounds;
		});
		if (m_showGui)
		{
			ImGui::Begin("Lights");
			std::string txt = std::format("Active lights: {}", m_totalPointlightsThisFrame);
			ImGui::Text(txt.c_str());
			ImGui::End();
		}
		return true;
	}

	bool LightsSystem::DrawLightBounds()
	{
		if (m_drawBounds)
		{
			auto entities = Systems::GetSystem<Entities::EntitySystem>();
			auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
			auto drawLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
				imRender->AddSphere(t.GetPosition(), pl.m_distance, { pl.m_colour, 1 });
				return true;
			};
			if (entities->GetActiveWorld())
			{
				Entities::Queries::ForEach<PointLightComponent, TransformComponent>(entities->GetActiveWorld(), drawLights);
			}
		}
		return true;
	}

	void LightsSystem::CollectLightsForDrawing(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		OnMainPassBegin(*ctx.m_device, ctx.m_graphicsCmds);	// shim
	}

	void LightsSystem::OnMainPassBegin(Device& d, VkCommandBuffer cmds)
	{
		R3_PROF_EVENT();
		if (!m_allPointlights.IsCreated())
		{
			if (!m_allPointlights.Create(d, c_maxLights * c_framesInFlight, c_maxLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create vertex buffer");
			}
			m_allPointlights.Allocate(c_maxLights * c_framesInFlight);
		}
		static std::vector<Pointlight> allPointlights;
		allPointlights.reserve(1024);
		auto collectLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
			Pointlight newlight;
			newlight.m_colourBrightness = {pl.m_colour, pl.m_brightness};
			newlight.m_positionDistance = {t.GetPosition(), pl.m_distance};
			allPointlights.push_back(newlight);
			return true;
		};
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (entities->GetActiveWorld())
		{
			Entities::Queries::ForEach<PointLightComponent, TransformComponent>(entities->GetActiveWorld(), collectLights);
		}
		m_currentInFrameOffset += c_maxLights;
		if (m_currentInFrameOffset >= (c_maxLights * c_framesInFlight))
		{
			m_currentInFrameOffset = 0;
		}
		m_allPointlights.Write(m_currentInFrameOffset, allPointlights.size(), allPointlights.data());
		m_allPointlights.Flush(d, cmds);
		m_totalPointlightsThisFrame = static_cast<uint32_t>(allPointlights.size());
		allPointlights.clear();
	}
}