#include "immediate_render_system.h"
#include "frame_scheduler_system.h"
#include "camera_system.h"
#include "lua_system.h"
#include "render/render_system.h"
#include "render/render_graph.h"
#include "core/log.h"
#include "core/profiler.h"

namespace R3
{
	ImmediateRenderSystem::ImmediateRenderSystem()
	{
		m_imRender = std::make_unique<ImmediateRenderer>();
	}

	ImmediateRenderSystem::~ImmediateRenderSystem()
	{
	}

	bool ImmediateRenderSystem::Init()
	{
		auto frameScheduler = GetSystem<FrameScheduler>();
		auto render = GetSystem<RenderSystem>();
		// assuming this is ran after render init (dangerous)
		if (!m_imRender->Initialise(*render->GetDevice(), frameScheduler->GetMainColourTargetFormat(), frameScheduler->GetMainDepthStencilFormat()))
		{
			LogError("Failed to create immediate renderer");
			return false;
		}
		render->m_onShutdownCbs.AddCallback([this](Device& d) {
			m_imRender->Destroy(d);
			m_imRender = {};
		});
		auto scripts = GetSystem<LuaSystem>();
		scripts->RegisterFunction("IMDrawLine", [this](glm::vec3 p0, glm::vec3 p1, glm::vec4 colour) {
			m_imRender->AddLine(p0, p1, colour);
		});
		scripts->RegisterFunction("IMDrawAxis", [this](glm::vec3 pos, float scale) {
			m_imRender->AddAxisAtPoint(pos, scale);
		});
		return true;
	}

	void ImmediateRenderSystem::OnMainPassBegin(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		auto render = GetSystem<RenderSystem>();
		m_imRender->WriteVertexData(*render->GetDevice(), *render->GetStagingBufferPool(), ctx.m_graphicsCmds);
	}

	void ImmediateRenderSystem::OnMainPassDraw(RenderPassContext& ctx)
	{
		R3_PROF_EVENT();
		auto cameras = GetSystem<CameraSystem>();
		auto render = GetSystem<RenderSystem>();
		auto projViewMat = cameras->GetMainCamera().ProjectionMatrix() * cameras->GetMainCamera().ViewMatrix();
		VkExtent2D extents((uint32_t)ctx.m_renderExtents.x, (uint32_t)ctx.m_renderExtents.y);
		m_imRender->Draw(projViewMat, *render->GetDevice(), extents, ctx.m_graphicsCmds);
		m_imRender->Flush();
	}
}