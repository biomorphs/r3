#include "lights_system.h"
#include "engine/imgui_menubar_helper.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/components/point_light.h"
#include "engine/components/environment_settings.h"
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

		RegisterTick("LightsSystem::CollectAllLights", [this]() {
			return CollectAllLights();
		});
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
			m_allLightsData.Destroy(d);
		});
	}

	VkDeviceAddress LightsSystem::GetAllLightsDeviceAddress()
	{
		return m_allLightsData.GetDataDeviceAddress() + (m_currentFrame * sizeof(AllLights));
	}

	glm::vec3 LightsSystem::GetSkyColour()
	{
		return m_skyColour;
	}

	bool LightsSystem::CollectAllLights()
	{
		R3_PROF_EVENT();
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld == nullptr || !m_allPointlights.IsCreated() || !m_allLightsData.IsCreated())
		{
			return true;
		}

		if (++m_currentFrame >= c_framesInFlight)
		{
			m_currentFrame = 0;
		}

		AllLights thisFrameLightData;
		auto collectSunSkySettings = [&](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
			thisFrameLightData.m_sunColourAmbient = { cmp.m_sunColour, cmp.m_sunAmbientFactor };
			thisFrameLightData.m_skyColourAmbient = { cmp.m_skyColour, cmp.m_skyAmbientFactor };
			thisFrameLightData.m_sunDirectionBrightness = { glm::normalize(cmp.m_sunDirection), cmp.m_sunBrightness };
			return true;
		};
		Entities::Queries::ForEach<EnvironmentSettingsComponent>(activeWorld, collectSunSkySettings);
		m_skyColour = glm::vec3(thisFrameLightData.m_skyColourAmbient);	

		const uint32_t pointLightBaseOffset = m_currentFrame * c_maxLights;
		thisFrameLightData.m_pointLightsBufferAddress = m_allPointlights.GetDataDeviceAddress() + pointLightBaseOffset * sizeof(Pointlight);
		auto collectPointLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
			if (pl.m_enabled)
			{
				Pointlight newlight;
				newlight.m_colourBrightness = { pl.m_colour, pl.m_brightness };
				newlight.m_positionDistance = { glm::vec3(t.GetWorldspaceInterpolated(e, *activeWorld)[3]), pl.m_distance };
				m_allPointlights.Write(pointLightBaseOffset + thisFrameLightData.m_pointlightCount, 1, &newlight);
				thisFrameLightData.m_pointlightCount++;
			}
			return true;
		};
		Entities::Queries::ForEach<PointLightComponent, TransformComponent>(activeWorld, collectPointLights);
		m_allLightsData.Write(m_currentFrame, 1, &thisFrameLightData);
		m_totalPointlightsThisFrame = thisFrameLightData.m_pointlightCount;

		return true;
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
			auto activeWorld = entities->GetActiveWorld();
			auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
			auto drawLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
				imRender->AddSphere(glm::vec3(t.GetWorldspaceInterpolated(e, *activeWorld)[3]), pl.m_distance, { pl.m_colour, pl.m_enabled ? 1.0f : 0.25f });
				return true;
			};
			if (activeWorld)
			{
				Entities::Queries::ForEach<PointLightComponent, TransformComponent>(activeWorld, drawLights);
			}
		}
		return true;
	}

	void LightsSystem::CollectLightsForDrawing(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		if (!m_allPointlights.IsCreated())
		{
			m_allPointlights.SetDebugName("Point lights");
			if (!m_allPointlights.Create(*ctx.m_device, c_maxLights * c_framesInFlight, c_maxLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create point light buffer");
			}
			m_allPointlights.Allocate(c_maxLights * c_framesInFlight);
		}
		if (!m_allLightsData.IsCreated())
		{
			m_allLightsData.SetDebugName("All Light Data");
			if (!m_allLightsData.Create(*ctx.m_device, c_framesInFlight, c_framesInFlight, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create all light buffer");
			}
			m_allLightsData.Allocate(c_framesInFlight);
		}

		m_allPointlights.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		m_allLightsData.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
}