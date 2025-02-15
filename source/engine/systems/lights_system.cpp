#include "lights_system.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/systems/immediate_render_system.h"
#include "engine/systems/camera_system.h"
#include "engine/components/point_light.h"
#include "engine/components/environment_settings.h"
#include "engine/components/transform.h"
#include "engine/utils/frustum.h"
#include "entities/world.h"
#include "entities/queries.h"
#include "entities/systems/entity_system.h"
#include "render/render_system.h"
#include "render/render_pass_context.h"
#include "render/descriptors.h"
#include "render/device.h"
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
			m_allSpotlights.Destroy(d);
			m_allLightsData.Destroy(d);
			vkDestroySampler(d.GetVkDevice(), m_shadowSampler, nullptr);
			m_descriptorAllocator = {};
			vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_shadowMapDescriptorLayout, nullptr);
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

	glm::mat4 LightsSystem::GetSunShadowMatrix(float minDepth, float maxDepth, int shadowMapResolution)
	{
		R3_PROF_EVENT();

		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		const glm::mat4 inverseProjView = glm::inverse(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());

		// build world-space frustum corners from camera frustum
		glm::vec3 frustumCorners[] = {	// clip space
			{ -1, -1, 0 },
			{ 1, -1, 0 },
			{ 1, 1, 0 },
			{ -1, 1, 0 },
			{ -1, -1, 1 },
			{ 1, -1, 1 },
			{ 1, 1, 1 },
			{ -1, 1, 1 },
		};
		for (int i = 0; i < std::size(frustumCorners); ++i)
		{
			auto projected = inverseProjView * glm::vec4(frustumCorners[i], 1.0f);		// camera -> world space
			frustumCorners[i] = glm::vec3(projected / projected.w);
		}

		// build light view matrix centered around world origin
		glm::vec3 sunDir = glm::normalize(m_sunDirection);
		static float s_lightDistance = 100.0f;	// parameter?
		glm::mat4 lightView = glm::lookAt(-sunDir * s_lightDistance, { 0,0,0 }, { 0,1,0 });

		// rescale based on min + max distance of this cascade
		glm::vec3 lightSpaceFrustumMin(FLT_MAX);		// calculate min/max points of view frustum in light space
		glm::vec3 lightSpaceFrustumMax(-FLT_MAX);
		float near = FLT_MAX, far = -FLT_MAX;
		for (int i = 0; i < 4; ++i)
		{
			glm::vec3 nearToFar = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + nearToFar * maxDepth;
			frustumCorners[i] = frustumCorners[i] + nearToFar * minDepth;

			glm::vec4 frustumPointInLightSpace = lightView * glm::vec4(frustumCorners[i], 1.0f);
			lightSpaceFrustumMin = glm::min(lightSpaceFrustumMin, glm::vec3(frustumPointInLightSpace));
			lightSpaceFrustumMax = glm::max(lightSpaceFrustumMax, glm::vec3(frustumPointInLightSpace));
			frustumPointInLightSpace = lightView * glm::vec4(frustumCorners[i + 4], 1.0f);
			lightSpaceFrustumMin = glm::min(lightSpaceFrustumMin, glm::vec3(frustumPointInLightSpace));
			lightSpaceFrustumMax = glm::max(lightSpaceFrustumMax, glm::vec3(frustumPointInLightSpace));
		}

		// calculate size of map texels in world space
		glm::vec3 worldUnitsPerTexel = (lightSpaceFrustumMax - lightSpaceFrustumMin) / (float)shadowMapResolution;

		// snap the min/max to texel size to avoid shimmering
		lightSpaceFrustumMin = glm::floor(lightSpaceFrustumMin / worldUnitsPerTexel) * worldUnitsPerTexel;
		lightSpaceFrustumMax = glm::floor(lightSpaceFrustumMax / worldUnitsPerTexel) * worldUnitsPerTexel;
	
		// build projection from bounds, note min + max z are flipped since camera looks down -z
		glm::mat4 lightProjection = glm::ortho(lightSpaceFrustumMin.x, lightSpaceFrustumMax.x, lightSpaceFrustumMin.y, lightSpaceFrustumMax.y, -lightSpaceFrustumMax.z, -lightSpaceFrustumMin.z);

		return lightProjection * lightView;
	}

	int LightsSystem::GetShadowCascadeCount()
	{
		return (int)m_sunShadowCascades.size();
	}

	glm::mat4 LightsSystem::GetShadowCascadeMatrix(int cascade)
	{
		glm::mat4 finalMatrix(1.0f);
		if (cascade < m_sunShadowCascades.size())
		{
			float maxDepth = (cascade + 1) < m_sunShadowCascades.size() ? m_sunShadowCascades[cascade + 1].m_distance : 1.0f;
			finalMatrix = GetSunShadowMatrix(m_sunShadowCascades[cascade].m_distance, maxDepth, m_sunShadowCascades[cascade].m_resolution);
		}
		return finalMatrix;
	}

	RenderTargetInfo LightsSystem::GetShadowCascadeTargetInfo(int cascade)
	{
		assert(cascade < ShadowMetadata::c_maxShadowCascades);
		RenderTargetInfo cascadeTarget;
		cascadeTarget.m_format = VK_FORMAT_D32_SFLOAT;			// may be overkill
		cascadeTarget.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		cascadeTarget.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		cascadeTarget.m_sizeType = RenderTargetInfo::SizeType::Fixed;
		cascadeTarget.m_size = { m_sunShadowCascades[cascade].m_resolution, m_sunShadowCascades[cascade].m_resolution };
		cascadeTarget.m_name = std::format("Shadow cascade {}_{}", cascade, m_sunShadowCascades[cascade].m_resolution);		// append resolution since RT lookup is by name
		return cascadeTarget;
	}

	void LightsSystem::GetShadowCascadeDepthBiasSettings(int cascade, float& constant, float& clamp, float& slope)
	{
		if (cascade < m_sunShadowCascades.size())
		{
			constant = m_sunShadowCascades[cascade].m_depthBiasConstantFactor;
			clamp = m_sunShadowCascades[cascade].m_depthBiasClamp;
			slope = m_sunShadowCascades[cascade].m_depthSlopeBias;
		}
		else
		{
			constant = 0.0f;
			clamp = 0.0f;
			slope = 0.0f;
		}
	}

	VkDescriptorSetLayout_T* LightsSystem::GetShadowMapDescriptorLayout()
	{
		return m_shadowMapDescriptorLayout;
	}

	VkDescriptorSet_T* LightsSystem::GetAllShadowMapsSet()
	{
		return m_allShadowMaps;
	}

	bool LightsSystem::CollectAllLights()
	{
		R3_PROF_EVENT();
		m_activePointLights = 0;
		auto activeWorld = Systems::GetSystem<Entities::EntitySystem>()->GetActiveWorld();
		if (activeWorld == nullptr || !m_allPointlights.IsCreated() || !m_allLightsData.IsCreated() || !m_allSpotlights.IsCreated())
		{
			return true;
		}
		if (++m_currentFrame >= c_framesInFlight)
		{
			m_currentFrame = 0;
		}
		// Collect global lighting data
		AllLights thisFrameLightData;
		auto collectSunSkySettings = [&](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
			thisFrameLightData.m_sunColourAmbient = { cmp.m_sunColour, cmp.m_sunAmbientFactor };
			thisFrameLightData.m_skyColourAmbient = { cmp.m_skyColour, cmp.m_skyAmbientFactor };
			thisFrameLightData.m_sunDirectionBrightness = { glm::normalize(cmp.m_sunDirection), cmp.m_sunBrightness };
			if (cmp.m_shadowCascadeCount < ShadowMetadata::c_maxShadowCascades)		// collect cascade data
			{
				m_sunShadowCascades.resize(cmp.m_shadowCascadeCount);
				for (int c = 0; c < cmp.m_shadowCascadeCount; ++c)
				{
					m_sunShadowCascades[c].m_resolution = cmp.m_shadowCascades[c].m_textureResolution;
					m_sunShadowCascades[c].m_distance = cmp.m_shadowCascades[c].m_depth;
					m_sunShadowCascades[c].m_depthBiasConstantFactor = cmp.m_shadowCascades[c].m_depthBiasConstantFactor;
					m_sunShadowCascades[c].m_depthBiasClamp = cmp.m_shadowCascades[c].m_depthBiasClamp;
					m_sunShadowCascades[c].m_depthSlopeBias = cmp.m_shadowCascades[c].m_depthSlopeBias;
				}
			}
			return true;
		};
		Entities::Queries::ForEach<EnvironmentSettingsComponent>(activeWorld, collectSunSkySettings);
		m_skyColour = glm::vec3(thisFrameLightData.m_skyColourAmbient);	
		m_sunDirection = glm::vec3(thisFrameLightData.m_sunDirectionBrightness);

		// Collect + cull point lights
		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		Frustum viewFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
		const uint32_t pointLightBaseOffset = m_currentFrame * c_maxPointLights;
		thisFrameLightData.m_pointLightsBufferAddress = m_allPointlights.GetDataDeviceAddress() + pointLightBaseOffset * sizeof(Pointlight);
		std::vector<Pointlight> activePointLights;
		activePointLights.reserve(c_maxPointLights / 4);
		auto collectPointLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
			if (pl.m_enabled)
			{
				glm::vec3 lightCenter = glm::vec3(t.GetWorldspaceInterpolated(e, *activeWorld)[3]);
				if (viewFrustum.IsSphereVisible(lightCenter, pl.m_distance))
				{
					Pointlight newlight;
					newlight.m_colourBrightness = { pl.m_colour, pl.m_brightness };
					newlight.m_positionDistance = { lightCenter, pl.m_distance };
					activePointLights.emplace_back(newlight);
					thisFrameLightData.m_pointlightCount++;
				}
			}
			return true;
		};
		Entities::Queries::ForEach<PointLightComponent, TransformComponent>(activeWorld, collectPointLights);

		// write shadow cascade metadata for this frame
		float nearFarDistance = mainCamera.FarPlane() - mainCamera.NearPlane();
		thisFrameLightData.m_shadows.m_sunShadowCascadeCount = (uint32_t)m_sunShadowCascades.size();
		for (int i = 0; i < m_sunShadowCascades.size(); ++i)
		{
			thisFrameLightData.m_shadows.m_sunShadowCascadeMatrices[i] = GetShadowCascadeMatrix(i);
			thisFrameLightData.m_shadows.m_sunShadowCascadeDistances[i] = nearFarDistance * m_sunShadowCascades[i].m_distance;	// pass view-space distance values
		}

		// Flush to gpu memory
		if (activePointLights.size() > 0)
		{
			m_allPointlights.Write(pointLightBaseOffset, activePointLights.size(), activePointLights.data());
			m_activePointLights = static_cast<uint32_t>(activePointLights.size());
		}
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
			ImGui::Checkbox("Show Cascaded Shadow Frusta", &m_drawCascadeFrusta);
			ImGui::Checkbox("Lock Debug Frusta", &m_lockDebugFrustums);
			ImGui::End();
		}
		return true;
	}

	bool LightsSystem::DrawLightBounds()
	{
		auto& imRender = Systems::GetSystem<ImmediateRenderSystem>()->m_imRender;
		if (m_drawBounds)
		{
			auto entities = Systems::GetSystem<Entities::EntitySystem>();
			auto activeWorld = entities->GetActiveWorld();
			auto drawLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
				imRender->AddSphere(glm::vec3(t.GetWorldspaceInterpolated(e, *activeWorld)[3]), pl.m_distance, { pl.m_colour, pl.m_enabled ? 1.0f : 0.25f });
				return true;
			};
			if (activeWorld)
			{
				Entities::Queries::ForEach<PointLightComponent, TransformComponent>(activeWorld, drawLights);
			}
		}

		if (m_drawCascadeFrusta)
		{
			// keep list around in case we lock the frustums
			const int cascades = (int)m_sunShadowCascades.size();
			static Camera mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
			static glm::mat4 cascadeViewProj[4];
			if (!m_lockDebugFrustums)
			{
				for (int c = 0; c < cascades && c < 4; ++c)
				{
					cascadeViewProj[c] = GetShadowCascadeMatrix(c);
				}
				mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
			}

			glm::vec4 cascadeColours[4] = {
				{ 0,1,0,1 },
				{ 1,1,0,1 },
				{ 1,0,0,1 },
				{ 1,0,1,1 }
			};
			for (int c = 0; c < cascades; ++c)
			{
				float minDepth = m_sunShadowCascades[c].m_distance;
				float maxDepth = c + 1 < cascades ? m_sunShadowCascades[c + 1].m_distance : 1.0f;

				// camera frustum
				imRender->AddFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix(), cascadeColours[c] * glm::vec4(1,1,1,0.25f), minDepth, maxDepth);

				// cascade frustum
				imRender->AddFrustum(cascadeViewProj[c], cascadeColours[c]);
			}
		}

		return true;
	}

	void LightsSystem::PrepareForDrawing(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		if (!m_allPointlights.IsCreated())
		{
			m_allPointlights.SetDebugName("Point lights");
			if (!m_allPointlights.Create(*ctx.m_device, c_maxPointLights * c_framesInFlight, c_maxPointLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create point light buffer");
			}
			m_allPointlights.Allocate(c_maxPointLights * c_framesInFlight);
		}
		if (!m_allSpotlights.IsCreated())
		{
			m_allSpotlights.SetDebugName("Spot lights");
			if (!m_allSpotlights.Create(*ctx.m_device, c_maxSpotLights * c_framesInFlight, c_maxSpotLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create spot light buffer");
			}
			m_allSpotlights.Allocate(c_maxSpotLights * c_framesInFlight);
		}
		if (!m_allLightsData.IsCreated())
		{
			m_allLightsData.SetDebugName("All Light Data");
			if (!m_allLightsData.Create(*ctx.m_device, c_framesInFlight, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
			{
				LogError("Failed to create all light buffer");
			}
			m_allLightsData.Allocate(c_framesInFlight);
		}
		if (m_shadowSampler == nullptr)
		{
			VkSamplerCreateInfo sampler = {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;		// return border colour if we sample outside (0,1)
			sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			sampler.maxLod = VK_LOD_CLAMP_NONE;
			sampler.minLod = 0;
			sampler.compareEnable = VK_TRUE;
			sampler.compareOp = VK_COMPARE_OP_LESS;
			sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;	// white = return depth of 1.0 at boundaries
			if (!VulkanHelpers::CheckResult(vkCreateSampler(ctx.m_device->GetVkDevice(), &sampler, nullptr, &m_shadowSampler)))
			{
				LogError("Failed to create shadow sampler");
				return;
			}
		}
		if (m_shadowMapDescriptorLayout == nullptr)
		{
			DescriptorLayoutBuilder layoutBuilder;
			layoutBuilder.AddBinding(0, ShadowMetadata::c_maxShadowCascades, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);		// one array of all shadow maps
			m_shadowMapDescriptorLayout = layoutBuilder.Create(*ctx.m_device, true);	// true = enable bindless descriptors
			if (m_shadowMapDescriptorLayout == nullptr)
			{
				LogError("Failed to create descriptor set layout for shadow maps");
				return;
			}
		}
		if (m_descriptorAllocator == nullptr)
		{
			m_descriptorAllocator = std::make_unique<DescriptorSetSimpleAllocator>();
			std::vector<VkDescriptorPoolSize> poolSizes = {
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)ShadowMetadata::c_maxShadowCascades }
			};
			if (!m_descriptorAllocator->Initialise(*ctx.m_device, ShadowMetadata::c_maxShadowCascades, poolSizes))
			{
				LogError("Failed to create descriptor allocator");
				return;
			}
			m_allShadowMaps = m_descriptorAllocator->Allocate(*ctx.m_device, m_shadowMapDescriptorLayout);
		}
		m_allSpotlights.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		m_allPointlights.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		m_allLightsData.Flush(*ctx.m_device, ctx.m_graphicsCmds, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void LightsSystem::PrepareShadowMaps(class RenderPassContext& ctx)
	{
		R3_PROF_EVENT();

		// update shadow map descriptor set before drawing anything
		DescriptorSetWriter writer(m_allShadowMaps);
		for (int i = 0; i < m_sunShadowCascades.size() && i < ShadowMetadata::c_maxShadowCascades; ++i)
		{
			auto shadowMap = ctx.GetResolvedTarget(GetShadowCascadeTargetInfo(i));
			if (shadowMap)
			{
				writer.WriteImage(0, i, shadowMap->m_view, m_shadowSampler, shadowMap->m_lastLayout);
			}
		}
		writer.FlushWrites();
	}
}