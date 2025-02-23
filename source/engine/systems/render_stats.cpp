#include "render_stats.h"
#include "engine/ui/imgui_menubar_helper.h"
#include "engine/systems/texture_system.h"
#include "engine/systems/mesh_renderer.h"
#include "render/render_system.h"
#include "render/render_target_cache.h"
#include "render/buffer_pool.h"
#include "render/device.h"
#include "core/profiler.h"
#include <imgui.h>
#include <format>

namespace R3
{
	auto BytesToMb = [](size_t bytes)
	{
		return (float)bytes / (1024.0f * 1024.0f);
	};

	auto GetValueBudgetColour = [](size_t bytes, size_t budget, glm::vec4 budgets = glm::vec4(0.0f, 0.5f, 0.75f, 1.0f))
	{
		const glm::vec3 colours[] = {
			{0,1,0},
			{0.15,1,0},
			{1,1,0},
			{1,0,0}
		};
		float fraction = (float)bytes / (float)budget;
		glm::vec3 finalColour = colours[3];
		for (int i = 0; i < 3; ++i)
		{
			float minBudget = budgets[i];
			float maxBudget = budgets[i + 1];
			if (fraction >= minBudget && fraction < maxBudget)
			{
				float t = (fraction - minBudget) / (maxBudget - minBudget);
				finalColour = glm::mix(colours[i], colours[i+1], t);
				break;
			}
		}
		return ImVec4(finalColour.x, finalColour.y, finalColour.z, 1);
	};

	void RenderStatsSystem::RegisterTickFns()
	{
		R3_PROF_EVENT();
		RegisterTick("RenderStats::ShowGui", [this]() {
			return ShowGui();
		});
	}

	bool RenderStatsSystem::ShowGui()
	{
		R3_PROF_EVENT();

		auto& debugMenu = MenuBar::MainMenu().GetSubmenu("Debug");
		debugMenu.AddItem("Show Render Stats", [this]() {
			m_displayStats = true;
		});

		if (m_displayStats)
		{
			ImGui::Begin("Render Stats", &m_displayStats);
			ShowVMAStats();
			ImVec2 contentSize = ImGui::GetContentRegionAvail();
			ImGui::BeginChild("MemoryWin", ImVec2(contentSize.x / 2, contentSize.y), ImGuiChildFlags_Borders, 0);
			ShowBufferPoolStats();
			ShowTextureStats();
			ShowRenderTargetStats();
			ImGui::EndChild();
			ImGui::SameLine();
			contentSize = ImGui::GetContentRegionAvail();
			ImGui::BeginChild("PerfWin", contentSize, ImGuiChildFlags_Borders, 0);
			ShowGpuPerfStats();
			ShowMeshRenderPerfStats();
			ImGui::EndChild();
			ImGui::End();
		}

		return true;
	}

	void RenderStatsSystem::ShowRenderTargetStats()
	{
		R3_PROF_EVENT();
		auto rtCache = GetSystem<RenderSystem>()->GetRenderTargetCache();
		ImGui::SeparatorText("Render Targets");
		size_t totalBytesAllocated = 0;
		auto calcTotalBytes = [&](const RenderTargetInfo& info, size_t sizeBytes)
		{
			totalBytesAllocated += sizeBytes;
		};
		rtCache->EnumerateTargets(calcTotalBytes);
		std::string txt = std::format("{:.3f}Mb Total", BytesToMb(totalBytesAllocated));
		ImGui::Text(txt.c_str());

		if (ImGui::CollapsingHeader("All Targets"))
		{
			auto showTarget = [&](const RenderTargetInfo& info, size_t sizeBytes)
			{
				txt = std::format("{} - {:.3f}Mb", info.m_name, BytesToMb(sizeBytes));
				ImGui::Text(txt.c_str());
			};
			rtCache->EnumerateTargets(showTarget);
		}
	}

	void RenderStatsSystem::ShowMeshRenderPerfStats()
	{
		R3_PROF_EVENT();
		ImGui::SeparatorText("Mesh Renderer");
		GetSystem<MeshRenderer>()->ShowPerfStatsGui();
	}

	void RenderStatsSystem::ShowGpuPerfStats()
	{
		R3_PROF_EVENT();
		ImGui::SeparatorText("GPU Profiler");
		GetSystem<RenderSystem>()->ShowGpuProfilerGui();
	}

	void RenderStatsSystem::ShowTextureStats()
	{
		R3_PROF_EVENT();
		ImGui::SeparatorText("Loaded Textures");
		auto textures = GetSystem<TextureSystem>();
		std::string txt = std::format("{:.3f}Mb Total",	BytesToMb(textures->GetTotalGpuMemoryUsedBytes()));
		ImGui::Text(txt.c_str());
	}

	void RenderStatsSystem::ShowVMAStats()
	{
		R3_PROF_EVENT();
		auto renderSys = Systems::GetSystem<RenderSystem>();
		auto vma = renderSys->GetDevice()->GetVMA();

		std::vector<VmaBudget> budgets(VK_MAX_MEMORY_HEAPS);
		vmaGetHeapBudgets(vma, budgets.data());
		size_t totalAllocatedBytes = 0, totalAllocations = 0;
		std::string heapStr;
		ImGui::SeparatorText("Vulkan Memory");
		for (int memHeap = 0; memHeap < VK_MAX_MEMORY_HEAPS; ++memHeap)
		{
			size_t usage = budgets[memHeap].usage;
			size_t budget = budgets[memHeap].budget;
			if (usage > 0)
			{
				ImVec4 colour = { 0,0,0,1 };
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GetValueBudgetColour(usage, budget));

				std::string heapTxt = std::format("Heap {}: {:.3f}Mb / {:.3f}Mb Budget", memHeap, BytesToMb(usage), BytesToMb(budget));
				ImGui::ProgressBar((float)usage / (float)budget, ImVec2(-FLT_MIN, 0), heapTxt.c_str());

				ImGui::PopStyleColor();
			}
		}
	}

	void RenderStatsSystem::ShowBufferPoolStats()
	{
		R3_PROF_EVENT();
		auto renderSys = Systems::GetSystem<RenderSystem>();
		auto pool = renderSys->GetBufferPool();

		size_t allocatedBytes = pool->GetTotalAllocatedBytes();
		size_t cachedBytes = pool->GetTotalCachedBytes();
		ImGui::SeparatorText("Buffer Pool");
		std::string txt = std::format("{:.3f}Mb Total in {} buffers, {:.3f}Mb cached in {} buffers", 
			BytesToMb(allocatedBytes),
			pool->GetTotalAllocatedCount(), 
			BytesToMb(cachedBytes),
			pool->GetTotalCachedCount());
		ImGui::Text(txt.c_str());

		auto ShowBuffer = [&txt](const PooledBuffer & buf)
		{
			txt = std::format("{} - {:.3f}Mb", buf.m_name, BytesToMb(buf.sizeBytes));
			ImGui::Text(txt.c_str());
		};

		if (ImGui::CollapsingHeader("Allocated buffers"))
		{
			pool->CollectAllocatedBufferStats(ShowBuffer);
		}
		if (ImGui::CollapsingHeader("Cached buffers"))
		{
			pool->CollectCachedBufferStats(ShowBuffer);
		}
	}
}