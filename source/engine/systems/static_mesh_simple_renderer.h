#pragma once
#include "engine/systems.h"
#include "render/vulkan_helpers.h"

namespace R3
{
	class Device;
	class StaticMeshSimpleRenderer : public System
	{
	public:
		StaticMeshSimpleRenderer();
		virtual ~StaticMeshSimpleRenderer();
		static std::string_view GetName() { return "StaticMeshSimpleRenderer"; }
		virtual void RegisterTickFns();
		virtual bool Init();
	private:
		bool ShowGui();
		void Cleanup(Device&);
		void MainPassBegin(Device&, VkCommandBuffer);
		void MainPassDraw(Device&, VkCommandBuffer, const VkExtent2D&);
		bool CreatePipelineData(Device&);

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_simpleTriPipeline = VK_NULL_HANDLE;
		bool m_showGui = false;
	};
}