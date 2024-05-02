#include "lights_system.h"
#include "engine/components/point_light.h"
#include "engine/components/transform.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "render/immediate_renderer.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	void LightsSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();

		RegisterTick("LightsSystem::DrawLightBounds", [this]() {
			return DrawLightBounds();
		});

		// Register render functions
		auto render = Systems::GetSystem<RenderSystem>();
		m_onMainPassBeginToken = render->m_onMainPassBegin.AddCallback([this](Device& d, VkCommandBuffer cmds) {
			OnMainPassBegin(d, cmds);
		});
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_allPointlights.Destroy(d);
		});
	}

	void LightsSystem::Shutdown()
	{
		Systems::GetSystem<RenderSystem>()->m_onMainPassBegin.RemoveCallback(m_onMainPassBeginToken);
	}

	bool LightsSystem::DrawLightBounds()
	{
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		auto& imRender = Systems::GetSystem<RenderSystem>()->GetImRenderer();
		auto drawLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
			imRender.AddSphere(t.GetPosition(), pl.m_distance, { pl.m_colour, 1 });
			return true;
		};
		if (entities->GetActiveWorld())
		{
			Entities::Queries::ForEach<PointLightComponent, TransformComponent>(entities->GetActiveWorld(), drawLights);
		}
		return true;
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
			newlight.m_attenuation = pl.m_attenuation;
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
		allPointlights.clear();
	}
}