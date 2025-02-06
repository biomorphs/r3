#include "environment_settings.h"
#include "entities/component_storage.h"
#include "engine/systems/lua_system.h"
#include <imgui.h>

namespace R3
{
	EnvironmentSettingsComponent::EnvironmentSettingsComponent()
	{
		// some reasonable default shadow cascade settings
		m_shadowCascadeCount = 3;
		m_shadowCascades[0].m_depth = 0.0f;
		m_shadowCascades[1].m_depth = 0.25f;
		m_shadowCascades[2].m_depth = 0.5f;
	}

	void EnvironmentSettingsComponent::RegisterScripts(LuaSystem& l)
	{
		l.RegisterType<EnvironmentSettingsComponent>(GetTypeName(),
			"m_skyColour", &EnvironmentSettingsComponent::m_skyColour,
			"m_sunDirection", &EnvironmentSettingsComponent::m_sunDirection,
			"m_sunColour", &EnvironmentSettingsComponent::m_sunColour,
			"m_sunBrightness", &EnvironmentSettingsComponent::m_sunBrightness,
			"m_skyAmbientFactor", &EnvironmentSettingsComponent::m_skyAmbientFactor,
			"m_sunAmbientFactor", &EnvironmentSettingsComponent::m_sunAmbientFactor
		);
	}

	void EnvironmentSettingsComponent::SerialiseJson(JsonSerialiser& s)
	{
		s("Sky Colour", m_skyColour);
		s("Sun Direction", m_sunDirection);
		s("Sun Colour", m_sunColour);
		s("Sun Brightness", m_sunBrightness);
		s("Sky Ambient", m_skyAmbientFactor);
		s("Sun Ambient", m_sunAmbientFactor);
		s("Tonemap Type", m_tonemapType);
		s("Flycam Draw Distance", m_flyCamFarPlane);
		s("Flycam Speed", m_flyCamMoveSpeed);
		s("Shadow Cascades", m_shadowCascadeCount);
		for (int i = 0; i < c_maxCascades; ++i)
		{
			s(std::format("Cascade_{}", i), m_shadowCascades[i]);
		}
	}

	void EnvironmentSettingsComponent::Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i)
	{
		ImGui::SeparatorText("Under Sun and Sky, Outlander");
		i.InspectColour("Sky Colour", m_skyColour, InspectProperty(&EnvironmentSettingsComponent::m_skyColour, e, w));
		i.Inspect("Sky Ambient", m_skyAmbientFactor, InspectProperty(&EnvironmentSettingsComponent::m_skyAmbientFactor, e, w), 0.0025f, 0.0f, 1.0f);
		i.InspectColour("Sun Colour", m_sunColour, InspectProperty(&EnvironmentSettingsComponent::m_sunColour, e, w));
		i.Inspect("Sun Brightness", m_sunBrightness, InspectProperty(&EnvironmentSettingsComponent::m_sunBrightness, e, w), 0.1f, 0.0f, FLT_MAX);
		i.Inspect("Sun Direction", m_sunDirection, InspectProperty(&EnvironmentSettingsComponent::m_sunDirection, e, w), glm::vec3(-1.0f), glm::vec3(1.0f));
		i.Inspect("Sun Ambient", m_sunAmbientFactor, InspectProperty(&EnvironmentSettingsComponent::m_sunAmbientFactor, e, w), 0.0025f, 0.0f, 1.0f);
		ImGui::SeparatorText("Render Settings");
		i.InspectEnum("Tonemapper", (int)m_tonemapType, InspectProperty(&EnvironmentSettingsComponent::m_tonemapType, e, w), TonemapCompute::c_toneMapTypeNames, (int)TonemapCompute::TonemapType::MaxTonemapTypes);
		ImGui::SeparatorText("Flycam Settings");
		i.Inspect("Draw Distance", m_flyCamFarPlane, InspectProperty(&EnvironmentSettingsComponent::m_flyCamFarPlane, e, w), 0.1f, 0.1f, FLT_MAX);
		i.Inspect("Move Speed", m_flyCamMoveSpeed, InspectProperty(&EnvironmentSettingsComponent::m_flyCamMoveSpeed, e, w), 0.1f, 0.1f, 1000.0f);
		ImGui::SeparatorText("Sun Shadows");
		i.Inspect("Num. Cascades", m_shadowCascadeCount, InspectProperty(&EnvironmentSettingsComponent::m_shadowCascadeCount, e, w), 1, 0, c_maxCascades);
		for (int cascade = 0; cascade < m_shadowCascadeCount; ++cascade)
		{
			auto inspectResolution = InspectComponentCustom<EnvironmentSettingsComponent, int>(e, w, [cascade](const Entities::EntityHandle&, EnvironmentSettingsComponent& cmp, Entities::World*, int v)
			{
				const int validResolutions[] = { 1024, 2048, 4096 };
				if (std::find(validResolutions, validResolutions + std::size(validResolutions), v))
				{
					cmp.m_shadowCascades[cascade].m_textureResolution = v;
				}
			});
			auto inspectDepth = InspectComponentCustom<EnvironmentSettingsComponent, float>(e, w, [cascade](const Entities::EntityHandle&, EnvironmentSettingsComponent& cmp, Entities::World*, float v)
			{
				cmp.m_shadowCascades[cascade].m_depth = v;
			});
			auto inspectConstBias = InspectComponentCustom<EnvironmentSettingsComponent, float>(e, w, [cascade](const Entities::EntityHandle&, EnvironmentSettingsComponent& cmp, Entities::World*, float v)
			{
				cmp.m_shadowCascades[cascade].m_depthBiasConstantFactor = v;
			});
			auto inspectBiasClamp = InspectComponentCustom<EnvironmentSettingsComponent, float>(e, w, [cascade](const Entities::EntityHandle&, EnvironmentSettingsComponent& cmp, Entities::World*, float v)
			{
				cmp.m_shadowCascades[cascade].m_depthBiasClamp = v;
			});
			auto inspectSlopeBias = InspectComponentCustom<EnvironmentSettingsComponent, float>(e, w, [cascade](const Entities::EntityHandle&, EnvironmentSettingsComponent& cmp, Entities::World*, float v)
			{
				cmp.m_shadowCascades[cascade].m_depthSlopeBias = v;
			});
			if (ImGui::CollapsingHeader(std::format("Cascade {}", cascade).c_str()))
			{
				float minDepth = cascade > 0 ? m_shadowCascades[cascade - 1].m_depth : 0.0f;
				float maxDepth = (cascade + 1) < m_shadowCascadeCount ? m_shadowCascades[cascade + 1].m_depth : 1.0f;
				i.Inspect(std::format("Distance##{}", cascade), m_shadowCascades[cascade].m_depth, inspectDepth, 0.01f, minDepth, maxDepth);
				i.Inspect(std::format("Resolution##{}", cascade), m_shadowCascades[cascade].m_textureResolution, inspectResolution, 1024, 1024, 4096);
				i.Inspect(std::format("Constant Bias##{}", cascade), m_shadowCascades[cascade].m_depthBiasConstantFactor, inspectConstBias, 0.01f, 0.0f, 10.0f);
				i.Inspect(std::format("Bias Clamp##{}", cascade), m_shadowCascades[cascade].m_depthBiasClamp, inspectBiasClamp, 0.01f, 0.0f, 10.0f);
				i.Inspect(std::format("Slope Clamp##{}", cascade), m_shadowCascades[cascade].m_depthSlopeBias, inspectSlopeBias, 0.01f, 0.0f, 10.0f);
			}
		}
	}

	void EnvironmentSettingsComponent::ShadowCascadeSettings::SerialiseJson(JsonSerialiser& s)
	{
		s("Resolution", m_textureResolution);
		s("CascadeDepth", m_depth);
		s("ConstantBias", m_depthBiasConstantFactor);
		s("BiasClamp", m_depthBiasClamp);
		s("SlopeBias", m_depthSlopeBias);
	}
}