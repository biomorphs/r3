#pragma once

#include "vulkan_helpers.h"
#include "core/glm_headers.h"

namespace R3
{
	class Device;
	class Swapchain;
	class Frustum;

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
		bool Initialise(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat, uint32_t maxVerticesPerFrame = 1024 * 20);
		void Destroy(Device& d);
		
		void WriteVertexData(Device& d, VkCommandBuffer& cmdBuffer);	// call this before calling draw! must be called outside of rendering
		void Draw(glm::mat4 vertexToScreen, Device& d, VkExtent2D viewportSize, VkCommandBuffer& cmdBuffer);
		void Flush();	// call at end of frame, clears out previous tri data

		void AddTriangle(const PerVertexData vertices[3]);
		void AddLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 colour);
		void AddLine(const PerVertexData vertices[2]);
		void AddLines(const PerVertexData* vertices, int linecount);
		void AddAxisAtPoint(glm::vec3 position, float scale = 1.0f, glm::mat4 transform = glm::identity<glm::mat4>());
		void AddFrustum(const Frustum& f, glm::vec4 colour);
		void AddCubeWireframe(glm::mat4 transform, glm::vec4 colour);
		void DrawAABB(glm::vec3 minbound, glm::vec3 maxbound, glm::mat4 transform, glm::vec4 colour);

	private:
		bool CreateNoDepthReadPipelines(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat);

		static constexpr int c_framesInFlight = 2;
		struct PerFrameData {
			uint32_t m_vertexOffset = 0;			// offset into m_allVertexData
		};
		struct DrawData {
			uint32_t m_startVertexOffset = 0;		// relative to m_thisFrameVertices
			uint32_t m_vertexCount = 0;
		};
		size_t m_maxVertices = 0;
		AllocatedBuffer m_allVertexData;		// contains c_framesInFlight x max vertices
		VkDeviceAddress m_allvertsBufferAddress;
		AllocatedBuffer m_stagingVertexData;	// contains max vertices for 1 frame, host visible, coherant
		void* m_mappedStagingBuffer = nullptr;
		std::vector<PerVertexData> m_thisFrameVertices;
		std::vector<DrawData> m_thisFrameTriangles;
		std::vector<DrawData> m_thisFrameLines;
		PerFrameData m_perFrameData[c_framesInFlight];
		int m_currentFrameIndex = 0;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_depthReadDisabledTriPipeline = VK_NULL_HANDLE;
		VkPipeline m_depthReadDisabledLinesPipeline = VK_NULL_HANDLE;
	};
}