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
			m_allLightsData.Destroy(d);
			vkDestroySampler(d.GetVkDevice(), m_shadowSampler, nullptr);
			m_descriptorAllocator = {};
			vkDestroyDescriptorSetLayout(d.GetVkDevice(), m_shadowMapDescriptorLayout, nullptr);
		});
	}

	bool LightsSystem::Init()
	{
		// Cascades defined as a fixed-distance along view frustum z axis
		m_sunShadowCascades.push_back(0.0f);
		m_sunShadowCascades.push_back(0.15f);
		m_sunShadowCascades.push_back(0.3f);
		m_sunShadowCascades.push_back(0.6f);
		assert(m_sunShadowCascades.size() <= ShadowMetadata::c_maxShadowCascades);

		return true;
	}

	VkDeviceAddress LightsSystem::GetAllLightsDeviceAddress()
	{
		return m_allLightsData.GetDataDeviceAddress() + (m_currentFrame * sizeof(AllLights));
	}

	glm::vec3 LightsSystem::GetSkyColour()
	{
		return m_skyColour;
	}

	glm::mat4 LightsSystem::GetSunShadowMatrix(float minDepth, float maxDepth)
	{
		R3_PROF_EVENT();

		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		
		// adjust near + far to match this cascade
		float clipPlaneDistance = mainCamera.FarPlane() - mainCamera.NearPlane();
		float newNearPlane = mainCamera.NearPlane() + (clipPlaneDistance * minDepth);
		float newFarPlane = mainCamera.NearPlane() + (clipPlaneDistance * maxDepth);
		mainCamera.SetClipPlanes(newNearPlane, newFarPlane);

		Frustum mainFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
		
		// extract corners of frustum in world space + calculate center
		glm::vec3 frustumCenter(0, 0, 0);	// find center point of frustum
		for (int frustumPoint = 0; frustumPoint < 8; ++frustumPoint)
		{
			const auto thisPoint = mainFrustum.GetPoints()[frustumPoint];
			frustumCenter += thisPoint;
		}
		frustumCenter /= 8.0f;

		// calculate distance of each corner from frustum center to get bounds of projection
		float boundsRadius = 0.0f;
		for (int frustumPoint = 0; frustumPoint < 8; ++frustumPoint)
		{
			const auto thisPoint = mainFrustum.GetPoints()[frustumPoint];
			float distance = glm::length(thisPoint - frustumCenter);
			boundsRadius = glm::max(boundsRadius, distance);
		}

		// stabilise bounds by rounding extents
		boundsRadius = ceil(boundsRadius * 16.0f) / 16.0f;

		// AABB centered around frustum mid point
		glm::vec3 maxExtents(boundsRadius);
		glm::vec3 minExtents = -maxExtents;

		// build view matrix centered around frustum center point
		glm::vec3 sunDir = glm::normalize(m_sunDirection);
		glm::mat4 lightView = glm::lookAt(frustumCenter - sunDir * maxExtents.z, frustumCenter, { 0,1,0 });

		// build projection from bounds
		glm::mat4 lightProjection = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		return lightProjection * lightView;
	}

	int LightsSystem::GetShadowCascadeCount()
	{
		return (int)m_sunShadowCascades.size();
	}

	glm::mat4 LightsSystem::GetShadowCascadeMatrix(int cascade)
	{
		glm::mat4 finalMatrix;
		if (cascade < m_sunShadowCascades.size())
		{
			float maxDepth = (cascade + 1) < m_sunShadowCascades.size() ? m_sunShadowCascades[cascade + 1] : 1.0f;
			finalMatrix = GetSunShadowMatrix(m_sunShadowCascades[cascade], maxDepth);
		}
		return finalMatrix;
	}

	RenderTargetInfo LightsSystem::GetShadowCascadeTargetInfo(int cascade)
	{
		RenderTargetInfo cascadeTarget(std::format("Shadow cascade {}", cascade));
		cascadeTarget.m_format = VK_FORMAT_D32_SFLOAT;			// may be overkill
		cascadeTarget.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		cascadeTarget.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		cascadeTarget.m_sizeType = RenderTargetInfo::SizeType::Fixed;
		cascadeTarget.m_size = { 2048, 2048 };
		return cascadeTarget;
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

		m_allPointLightsCPU.clear();

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
		m_sunDirection = glm::vec3(thisFrameLightData.m_sunDirectionBrightness);

		auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
		Frustum viewFrustum(mainCamera.ProjectionMatrix() * mainCamera.ViewMatrix());
		const uint32_t pointLightBaseOffset = m_currentFrame * c_maxLights;
		thisFrameLightData.m_pointLightsBufferAddress = m_allPointlights.GetDataDeviceAddress() + pointLightBaseOffset * sizeof(Pointlight);
		auto collectPointLights = [&](const Entities::EntityHandle& e, PointLightComponent& pl, TransformComponent& t) {
			if (pl.m_enabled)
			{
				glm::vec3 lightCenter = glm::vec3(t.GetWorldspaceInterpolated(e, *activeWorld)[3]);
				if (viewFrustum.IsSphereVisible(lightCenter, pl.m_distance))
				{
					Pointlight newlight;
					newlight.m_colourBrightness = { pl.m_colour, pl.m_brightness };
					newlight.m_positionDistance = { lightCenter, pl.m_distance };
					m_allPointLightsCPU.emplace_back(newlight);
					thisFrameLightData.m_pointlightCount++;
				}
			}
			return true;
		};
		Entities::Queries::ForEach<PointLightComponent, TransformComponent>(activeWorld, collectPointLights);

		// write shadow metadata for this frame
		float nearFarDistance = mainCamera.FarPlane() - mainCamera.NearPlane();
		thisFrameLightData.m_shadows.m_sunShadowCascadeCount = (uint32_t)m_sunShadowCascades.size();
		for (int i = 0; i < m_sunShadowCascades.size(); ++i)
		{
			thisFrameLightData.m_shadows.m_sunShadowCascadeMatrices[i] = GetShadowCascadeMatrix(i);
			thisFrameLightData.m_shadows.m_sunShadowCascadeDistances[i] = nearFarDistance * m_sunShadowCascades[i];	// pass view-space distance values
		}

		if (m_allPointLightsCPU.size() > 0)
		{
			m_allPointlights.Write(pointLightBaseOffset, m_allPointLightsCPU.size(), m_allPointLightsCPU.data());
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

	void LightsSystem::PrepareForDrawing(RenderPassContext& ctx)
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
			sampler.magFilter = VK_FILTER_NEAREST;		// no filtering of depth values
			sampler.minFilter = VK_FILTER_NEAREST;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;		// return border colour if we sample outside (0,1)
			sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			sampler.maxLod = VK_LOD_CLAMP_NONE;
			sampler.minLod = 0;
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