#include "frame_scheduler_system.h"
#include "immediate_render_system.h"
#include "imgui_system.h"
#include "lights_system.h"
#include "static_mesh_system.h"
#include "mesh_renderer.h"
#include "texture_system.h"
#include "engine/graphics/tonemap_compute.h"
#include "engine/graphics/deferred_lighting_compute.h"
#include "engine/graphics/simple_tiled_lights_compute.h"
#include "engine/graphics/depth_texture_visualiser.h"
#include "engine/components/environment_settings.h"
#include "engine/systems/camera_system.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "entities/systems/entity_system.h"
#include "entities/queries.h"
#include "render/render_graph.h"
#include "render/render_system.h"
#include "core/profiler.h"
#include "core/log.h"
#include <imgui.h>

namespace R3
{
	FrameScheduler::FrameScheduler()
	{
	}

	FrameScheduler::~FrameScheduler()
	{
	}

	void FrameScheduler::RegisterTickFns()
	{
		Systems::GetInstance().RegisterTick("FrameScheduler::UpdateTonemapper", [this]() {
			return UpdateTonemapper();
		});
		Systems::GetInstance().RegisterTick("FrameScheduler::BuildRenderGraph", [this]() {
			return BuildRenderGraph();
		});
		Systems::GetInstance().RegisterTick("FrameScheduler::ShowGui", [this]() {
			return ShowGui();
		});
	}

	bool FrameScheduler::Init()
	{
		m_tonemapComputeRenderer = std::make_unique<TonemapCompute>();
		m_deferredLightingCompute = std::make_unique<DeferredLightingCompute>();
		m_tiledLightsCompute = std::make_unique<TiledLightsCompute>();
		m_depthTextureVisualiser = std::make_unique<DepthTextureVisualiser>();
		GetSystem<RenderSystem>()->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_tonemapComputeRenderer->Cleanup(d);
			m_deferredLightingCompute->Cleanup(d);
			m_tiledLightsCompute->Cleanup(d);
			m_depthTextureVisualiser->Cleanup(d);
		});
		return true;
	}
	
	std::unique_ptr<GenericPass> FrameScheduler::MakeRenderPreparePass()
	{
		auto preparePass = std::make_unique<GenericPass>();
		preparePass->m_name = "Prepare for rendering";
		preparePass->m_onRun.AddCallback([](RenderPassContext& ctx) {
			GetSystem<TextureSystem>()->ProcessLoadedTextures(ctx);
			GetSystem<StaticMeshSystem>()->PrepareForRendering(ctx);
			GetSystem<MeshRenderer>()->PrepareForRendering(ctx);
		});
		return preparePass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeLightTilingPass(const RenderTargetInfo& mainDepth)
	{
		auto lightTilingPass = std::make_unique<ComputeDrawPass>();
		lightTilingPass->m_name = "Light Tiling";
		lightTilingPass->m_inputColourAttachments.push_back(mainDepth);
		lightTilingPass->m_onRun.AddCallback([this, mainDepth](RenderPassContext& ctx) {
			if (m_useTiledLighting)
			{
				auto screenSize = glm::uvec2(GetSystem<RenderSystem>()->GetWindowExtents());
				auto mainCamera = GetSystem<CameraSystem>()->GetMainCamera();
				VkDeviceAddress gpuData = 0;
				if (m_buildLightTilesOnCpu)
				{
					std::vector<TiledLightsCompute::LightTile> lightTiles;
					std::vector<uint32_t> lightIndices;
					m_tiledLightsCompute->BuildLightTilesCpu(screenSize, mainCamera, lightTiles, lightIndices);
					gpuData = m_tiledLightsCompute->CopyCpuDataToGpu(*ctx.m_device, ctx.m_graphicsCmds, screenSize, lightTiles, lightIndices);
				}
				else
				{
					auto depthTarget = ctx.GetResolvedTarget(mainDepth);
					gpuData = m_tiledLightsCompute->BuildTilesListCompute(*ctx.m_device, ctx.m_graphicsCmds, *depthTarget, screenSize, mainCamera);
				}
				m_deferredLightingCompute->SetTiledLightinMetadataAddress(gpuData);
				GetSystem<MeshRenderer>()->SetTiledLightinMetadataAddress(gpuData);
				m_lightTileDebugMetadataAddress = gpuData;
			}
			else
			{
				m_deferredLightingCompute->SetTiledLightinMetadataAddress(0);
				GetSystem<MeshRenderer>()->SetTiledLightinMetadataAddress(0);
				m_lightTileDebugMetadataAddress = 0;
			}
		});
		return lightTilingPass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeShadowCascadePass(const RenderTargetInfo& cascadeTarget, int cascadeIndex)
	{
		R3_PROF_EVENT();
		auto cascadePass = std::make_unique<DrawPass>();
		cascadePass->m_name = std::format("Shadow cascade {}", cascadeIndex);
		cascadePass->m_depthAttachment = { cascadeTarget, DrawPass::AttachmentLoadOp::Clear };
		cascadePass->m_getExtentsFn = [cascadeTarget]() -> glm::vec2 {
			return cascadeTarget.m_size;
		};
		cascadePass->m_getClearDepthFn = []() -> float {
			return 1.0f;
		};

		glm::mat4 cascadeMatrix = GetSystem<LightsSystem>()->GetShadowCascadeMatrix(cascadeIndex);
		cascadePass->m_onDraw.AddCallback([cascadeTarget, cascadeMatrix](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Shadow Cascade");
			GetSystem<MeshRenderer>()->OnShadowMapDraw(ctx, cascadeTarget, cascadeMatrix);
		});

		return cascadePass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeCullingPass()
	{
		auto cullingPass = std::make_unique<ComputeDrawPass>();
		cullingPass->m_name = "Instance Culling";
		cullingPass->m_onRun.AddCallback([](RenderPassContext& ctx) {
			GetSystem<MeshRenderer>()->CullInstancesOnGpu(ctx);
		});
		return cullingPass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeGBufferPass(const RenderTargetInfo& positionBuffer, const RenderTargetInfo& normalBuffer, const RenderTargetInfo& albedoBuffer, const RenderTargetInfo& mainDepth)
	{
		R3_PROF_EVENT();
		auto gbufferPass = std::make_unique<DrawPass>();
		gbufferPass->m_name = "GBuffer Pass";
		gbufferPass->m_colourAttachments.push_back({ positionBuffer, DrawPass::AttachmentLoadOp::Clear });
		gbufferPass->m_colourAttachments.push_back({ normalBuffer, DrawPass::AttachmentLoadOp::Clear });
		gbufferPass->m_colourAttachments.push_back({ albedoBuffer, DrawPass::AttachmentLoadOp::Clear });
		gbufferPass->m_depthAttachment = { mainDepth, DrawPass::AttachmentLoadOp::Clear };
		gbufferPass->m_getExtentsFn = []() -> glm::vec2 {
			return GetSystem<RenderSystem>()->GetWindowExtents();
		};
		gbufferPass->m_getClearColourFn = []() -> glm::vec4 {
			return glm::vec4(0.0f);
		};
		gbufferPass->m_onDraw.AddCallback([](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("GBuffer Pass");
			GetSystem<MeshRenderer>()->OnGBufferPassDraw(ctx);
		});
		
		return gbufferPass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeDeferredLightingPass(const RenderTargetInfo& mainDepth, const RenderTargetInfo& positionBuffer, const RenderTargetInfo& normalBuffer, const RenderTargetInfo& albedoBuffer, const RenderTargetInfo& mainColour)
	{
		R3_PROF_EVENT();
		auto lightingPass = std::make_unique<ComputeDrawPass>();
		lightingPass->m_name = "Deferred Lighting";
		lightingPass->m_inputColourAttachments.push_back(positionBuffer);
		lightingPass->m_inputColourAttachments.push_back(normalBuffer);
		lightingPass->m_inputColourAttachments.push_back(albedoBuffer);
		lightingPass->m_inputColourAttachments.push_back(mainDepth);

		// add all shadow cascades as input attachments
		auto lights = GetSystem<LightsSystem>();
		for (int i = 0; i < lights->GetShadowCascadeCount(); ++i)
		{
			lightingPass->m_inputColourAttachments.push_back(lights->GetShadowCascadeTargetInfo(i));
		}

		lightingPass->m_outputColourAttachments.push_back(mainColour);	// output to HDR colour
		lightingPass->m_onRun.AddCallback([this, mainDepth, mainColour, positionBuffer, normalBuffer, albedoBuffer](RenderPassContext& ctx) {
			auto inDepth = ctx.GetResolvedTarget(mainDepth);
			auto inPosMetal = ctx.GetResolvedTarget(positionBuffer);
			auto inNormalRoughness = ctx.GetResolvedTarget(normalBuffer);
			auto inAlbedoAO = ctx.GetResolvedTarget(albedoBuffer);
			auto outTarget = ctx.GetResolvedTarget(mainColour);
			auto outSize = ctx.m_targets->GetTargetSize(outTarget->m_info);
			GetSystem<LightsSystem>()->PrepareForDrawing(ctx);		// do this here since the shadow maps will resolve properly
			m_deferredLightingCompute->Run(*ctx.m_device, ctx.m_graphicsCmds, *inDepth, *inPosMetal, *inNormalRoughness, *inAlbedoAO, *outTarget, outSize, m_useTiledLighting);
		});
		return lightingPass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeForwardPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainDepth)
	{
		R3_PROF_EVENT();
		auto forwardPass = std::make_unique<DrawPass>();
		forwardPass->m_name = "Forward Pass";
		forwardPass->m_colourAttachments.push_back({ mainColour, DrawPass::AttachmentLoadOp::Load });	// reuse previous depth + colour data
		forwardPass->m_depthAttachment = { mainDepth, DrawPass::AttachmentLoadOp::Load };
		forwardPass->m_getExtentsFn = []() -> glm::vec2 {
			return GetSystem<RenderSystem>()->GetWindowExtents();
		};
		forwardPass->m_getClearColourFn = []() -> glm::vec4 {
			return glm::vec4(GetSystem<LightsSystem>()->GetSkyColour(), 1.0f);
		};
		forwardPass->m_onBegin.AddCallback([](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Forward Pass Begin");
			GetSystem<ImmediateRenderSystem>()->PrepareForRendering(ctx);
		});
		forwardPass->m_onDraw.AddCallback([this](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Forward Pass");
			GetSystem<MeshRenderer>()->OnForwardPassDraw(ctx, m_useTiledLighting);
			GetSystem<ImmediateRenderSystem>()->OnForwardPassDraw(ctx);
		});
		forwardPass->m_onEnd.AddCallback([](RenderPassContext& ctx) {
			GetSystem<MeshRenderer>()->OnDrawEnd(ctx);
		});
		return forwardPass;
	}

	std::unique_ptr<TransferPass> FrameScheduler::MakeColourBlitToPass(std::string_view name, const RenderTargetInfo& srcTarget, const RenderTargetInfo& destTarget)
	{
		R3_PROF_EVENT();
		auto blitPass = std::make_unique<TransferPass>();		// blit LDR colour to swap chain
		blitPass->m_name = name;
		blitPass->m_inputs.push_back(srcTarget);
		blitPass->m_outputs.push_back(destTarget);
		blitPass->m_onRun.AddCallback([this, srcTarget, destTarget](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("Main Pass blit to swap");
			glm::vec2 srcSize = ctx.m_targets->GetTargetSize(srcTarget);
			glm::vec2 dstSize = ctx.m_targets->GetTargetSize(destTarget);
			glm::vec2 srcExtents = glm::min(srcSize, dstSize);	// clip blit dimensions to size of dest target
			VkExtent2D extents((uint32_t)srcExtents.x, (uint32_t)srcExtents.y);
			VulkanHelpers::BlitColourImageToImage(ctx.m_graphicsCmds,
				ctx.GetResolvedTarget(srcTarget)->m_image, extents,
				ctx.GetResolvedTarget(destTarget)->m_image, extents);
		});
		return blitPass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeDepthTextureDebugPass(const RenderTargetInfo& depthTexture, const RenderTargetInfo& outputTexture)
	{
		auto pass = std::make_unique<ComputeDrawPass>();
		pass->m_name = "Visualise Depth Texture";
		pass->m_inputColourAttachments.push_back(depthTexture);
		pass->m_outputColourAttachments.push_back(outputTexture);	// output to HDR colour
		pass->m_onRun.AddCallback([this, depthTexture, outputTexture](RenderPassContext& ctx) {
			auto inDepth = ctx.GetResolvedTarget(depthTexture);
			auto outTarget = ctx.GetResolvedTarget(outputTexture);
			auto depthSize = ctx.m_targets->GetTargetSize(inDepth->m_info);
			auto outSize = ctx.m_targets->GetTargetSize(outTarget->m_info);
			m_depthTextureVisualiser->Run(*ctx.m_device, ctx.m_graphicsCmds, *inDepth, depthSize, *outTarget, outSize);
		});
		return pass;
	}

	std::unique_ptr<DrawPass> FrameScheduler::MakeImguiPass(const RenderTargetInfo& colourTarget)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		auto imgui = GetSystem<ImGuiSystem>();
		auto imguiPass = std::make_unique<DrawPass>();			// imgui draw direct to swap image
		imguiPass->m_name = "ImGUI Render";
		imguiPass->m_colourAttachments.push_back({ colourTarget, DrawPass::AttachmentLoadOp::Load });
		imguiPass->m_getExtentsFn = [render]() -> glm::vec2 {
			return render->GetWindowExtents();
		};
		imguiPass->m_onDraw.AddCallback([imgui](RenderPassContext& ctx) {
			R3_PROF_GPU_EVENT("IMGUI");
			imgui->OnDraw(ctx);
		});
		return imguiPass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeTonemapToLDRPass(const RenderTargetInfo& mainColour, const RenderTargetInfo& mainColourLDR)
	{
		// This pass reads the main colour image, runs tonemap operator and writes to the swap chain image
		auto tonemapPass = std::make_unique<ComputeDrawPass>();
		tonemapPass->m_name = "Tonemap Compute";
		tonemapPass->m_inputColourAttachments.push_back(mainColour);	// HDR input
		tonemapPass->m_outputColourAttachments.push_back(mainColourLDR);	// output to LDR image
		tonemapPass->m_onRun.AddCallback([this, mainColour, mainColourLDR](RenderPassContext& ctx) {
			auto inTarget = ctx.GetResolvedTarget(mainColour);
			auto inSize = ctx.m_targets->GetTargetSize(inTarget->m_info);
			auto outTarget = ctx.GetResolvedTarget(mainColourLDR);
			auto outSize = ctx.m_targets->GetTargetSize(outTarget->m_info);
			m_tonemapComputeRenderer->Run(*ctx.m_device, ctx.m_graphicsCmds, *inTarget, inSize, *outTarget, outSize);
		});
		return tonemapPass;
	}

	std::unique_ptr<ComputeDrawPass> FrameScheduler::MakeLightTileDebugPass(const RenderTargetInfo& colourTarget)
	{
		// This pass reads the main colour image, runs tonemap operator and writes to the swap chain image
		auto debugPass = std::make_unique<ComputeDrawPass>();
		debugPass->m_name = "Light Tiles Debug";
		debugPass->m_outputColourAttachments.push_back(colourTarget);
		debugPass->m_onRun.AddCallback([this, colourTarget](RenderPassContext& ctx) {
			auto outTarget = ctx.GetResolvedTarget(colourTarget);
			auto outSize = ctx.m_targets->GetTargetSize(outTarget->m_info);
			m_tiledLightsCompute->ShowTilesDebug(*ctx.m_device, ctx.m_graphicsCmds, *outTarget, outSize, m_lightTileDebugMetadataAddress);
		});
		return debugPass;
	}

	// This describes the entire renderer as a series of passes that run in series
	bool FrameScheduler::BuildRenderGraph()
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		m_allCurrentTargets.clear();

		RenderTargetInfo swapchainImage("Swapchain");	// Swap chain image, this will be presented to the screen each frame
		RenderTargetInfo mainColour("MainColour");		// HDR colour buffer, linear encoding
		mainColour.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		mainColour.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_allCurrentTargets.push_back(mainColour);

		RenderTargetInfo mainDepth("MainDepth");		// Main depth buffer
		mainDepth.m_format = VK_FORMAT_D32_SFLOAT;		// may be overkill
		mainDepth.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		mainDepth.m_aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		m_allCurrentTargets.push_back(mainDepth);

		RenderTargetInfo gBufferPosition("GBuffer_PositionMetallic");		// World space positions + metallic
		gBufferPosition.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		gBufferPosition.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_allCurrentTargets.push_back(gBufferPosition);

		RenderTargetInfo gBufferNormal("GBuffer_NormalsRoughness");		// World space normal + roughness
		gBufferNormal.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		gBufferNormal.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_allCurrentTargets.push_back(gBufferNormal);

		RenderTargetInfo gBufferAlbedo("GBuffer_AlbedoAO");		// linear albedo + AO
		gBufferAlbedo.m_format = VK_FORMAT_R8G8B8A8_UNORM;
		gBufferAlbedo.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_allCurrentTargets.push_back(gBufferAlbedo);

		RenderTargetInfo mainColourLDR("MainColourLDR");		// LDR colour buffer after tonemapping
		mainColourLDR.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;	// still using floating point storage
		mainColourLDR.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		m_allCurrentTargets.push_back(mainColourLDR);

		auto& graph = render->GetRenderGraph();
		graph.m_allPasses.clear();
		graph.m_allPasses.push_back(MakeRenderPreparePass());
		graph.m_allPasses.push_back(MakeCullingPass());

		// shadow cascade passes
		auto lights = GetSystem<LightsSystem>();
		const int cascadeCount = lights->GetShadowCascadeCount();
		for (int i = 0; i < cascadeCount; ++i)
		{
			auto cascadeTarget = lights->GetShadowCascadeTargetInfo(i);
			m_allCurrentTargets.push_back(cascadeTarget);
			graph.m_allPasses.push_back(MakeShadowCascadePass(cascadeTarget, i));
		}
		graph.m_allPasses.push_back(MakeGBufferPass(gBufferPosition, gBufferNormal, gBufferAlbedo, mainDepth));	// write gbuffer
		graph.m_allPasses.push_back(MakeLightTilingPass(mainDepth));											// light tile determination
		graph.m_allPasses.push_back(MakeDeferredLightingPass(mainDepth, gBufferPosition, gBufferNormal, gBufferAlbedo, mainColour));	// deferred lighting
		graph.m_allPasses.push_back(MakeForwardPass(mainColour, mainDepth));	// forward render to main colour
		if (m_useTiledLighting && m_showLightTilesDebug)
		{
			graph.m_allPasses.push_back(MakeLightTileDebugPass(mainColour));
		}
		graph.m_allPasses.push_back(MakeTonemapToLDRPass(mainColour, mainColourLDR));	// HDR -> LDR
		if (m_renderTargetDebuggerEnabled && m_debugRenderTargetName.length() > 0)
		{
			auto srcTarget = std::find_if(m_allCurrentTargets.begin(), m_allCurrentTargets.end(), [&](const RenderTargetInfo& info) {
				return info.m_name == m_debugRenderTargetName;
			});
			if (srcTarget != m_allCurrentTargets.end())
			{
				if (srcTarget->m_aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT && srcTarget->m_name != mainColourLDR.m_name)
				{
					graph.m_allPasses.push_back(MakeColourBlitToPass("Colour target debug", *srcTarget, mainColourLDR));
				}
				else if (srcTarget->m_aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT)
				{
					graph.m_allPasses.push_back(MakeDepthTextureDebugPass(*srcTarget, mainColourLDR));
				}
			}
		}
		graph.m_allPasses.push_back(MakeColourBlitToPass("LDR to swap", mainColourLDR, swapchainImage));	// blit LDR -> swap
		graph.m_allPasses.push_back(MakeImguiPass(swapchainImage));		// imgui to swap
		
		return true;
	}

	bool FrameScheduler::ShowGui()
	{
		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Render target visualiser", [&]() {
			m_renderTargetDebuggerEnabled = true;
		});

		auto& lights = debugMenu.GetSubmenu("Lights");
		lights.AddItem(m_useTiledLighting ? "Use non-tiled lighting" : "Use tiled lighting", [this]() {
			m_useTiledLighting = !m_useTiledLighting;
		});
		if (m_useTiledLighting)
		{
			lights.AddItem(m_buildLightTilesOnCpu ? "Build lights using compute" : "Build lights on CPU", [this] {
				m_buildLightTilesOnCpu = !m_buildLightTilesOnCpu;
			});
			lights.AddItem(m_showLightTilesDebug ? "Hide light tiles debug" : "Show light tiles debug", [this]() {
				m_showLightTilesDebug = !m_showLightTilesDebug;
			});
		}
		

		if (m_renderTargetDebuggerEnabled)
		{
			ImGui::Begin("Render target visualiser", &m_renderTargetDebuggerEnabled);
			if (ImGui::BeginCombo("Target", m_debugRenderTargetName.c_str()))
			{
				for (int target = 0; target < m_allCurrentTargets.size(); ++target)
				{
					bool selected = (m_debugRenderTargetName == m_allCurrentTargets[target].m_name);
					if (ImGui::Selectable(m_allCurrentTargets[target].m_name.c_str(), selected))
					{
						m_debugRenderTargetName = m_allCurrentTargets[target].m_name;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();	// ensure keyboard/controller navigation works
					}
				}
				ImGui::EndCombo();
			}
			m_depthTextureVisualiser->ShowGui();
			ImGui::End();
		}
		
		return true;
	}

	bool FrameScheduler::UpdateTonemapper()
	{
		R3_PROF_EVENT();
		auto entities = Systems::GetSystem<Entities::EntitySystem>();
		if (auto activeWorld = entities->GetActiveWorld())
		{
			auto collectTonemapSettings = [&](const Entities::EntityHandle& e, EnvironmentSettingsComponent& cmp) {
				if (cmp.m_tonemapType >= 0 && cmp.m_tonemapType < (int)TonemapCompute::TonemapType::MaxTonemapTypes)
				{
					m_tonemapComputeRenderer->SetTonemapType((TonemapCompute::TonemapType)cmp.m_tonemapType);
				}
				return true;
			};
			Entities::Queries::ForEach<EnvironmentSettingsComponent>(activeWorld, collectTonemapSettings);
		}
		return true;
	}
}