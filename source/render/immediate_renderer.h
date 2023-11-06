#pragma once

#include "vulkan_helpers.h"
#include "core/glm_headers.h"

namespace R3
{
	class Device;
	class Swapchain;

	// handles a dynamic vertex buffer of triangles + draw calls
	class ImmediateRenderer
	{
	public:
		ImmediateRenderer();
		~ImmediateRenderer();

		struct PerVertexData
		{
			glm::vec4 m_position;
			glm::vec4 m_colour;
		};
		bool Initialise(Device& d, Swapchain& swapChain, VkFormat depthBufferFormat, uint32_t maxVerticesPerFrame = 1024 * 8);
		void Destroy(Device& d);
		
		void WriteVertexData(Device& d, VkCommandBuffer& cmdBuffer);	// call this before calling draw! must be called outside of rendering
		void Draw(Device& d, Swapchain& swapChain, VkCommandBuffer& cmdBuffer);
		void Flush();	// call at end of frame, clears out previous tri data

		void AddTriangle(const PerVertexData vertices[3]);

	private:
		bool CreateNoDepthReadPipeline(Device& d, Swapchain& swapChain, VkFormat depthBufferFormat);

		static constexpr int c_framesInFlight = 2;
		struct PerFrameData {
			uint32_t m_vertexOffset = 0;			// offset into m_allVertexData
		};
		struct TriangleDrawData {
			uint32_t m_startVertexOffset = 0;		// relative to m_thisFrameVertices
			uint32_t m_vertexCount = 0;
		};
		size_t m_maxVertices = 0;
		AllocatedBuffer m_allVertexData;		// contains c_framesInFlight x max vertices
		AllocatedBuffer m_stagingVertexData;	// contains max vertices for 1 frame, host visible, coherant
		void* m_mappedStagingBuffer = nullptr;
		std::vector<PerVertexData> m_thisFrameVertices;
		std::vector<TriangleDrawData> m_thisFrameTriangles;
		PerFrameData m_perFrameData[c_framesInFlight];
		int m_currentFrameIndex = 0;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_depthReadDisabledPipeline = VK_NULL_HANDLE;
	};
}