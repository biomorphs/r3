#pragma once

#include "vulkan_helpers.h"
#include "core/glm_headers.h"

namespace R3
{
	class Device;
	class Swapchain;
	class Frustum;
	class BufferPool;

	// handles a dynamic vertex buffer of triangles + draw calls
	class ImmediateRenderer
	{
	public:
		ImmediateRenderer();
		~ImmediateRenderer();

		struct PosColourVertex
		{
			glm::vec4 m_position;
			glm::vec4 m_colour;
		};
		struct PosColourUVVertex
		{
			glm::vec4 m_position;
			glm::vec4 m_colour;
			glm::vec2 m_uv;
		};
		bool Initialise(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat, uint32_t maxVerticesPerFrame = 1024 * 256);
		void Destroy(Device& d);
		
		void WriteVertexData(Device& d, BufferPool& stagingBuffers, VkCommandBuffer& cmdBuffer);	// call this before calling draw! must be called outside of rendering
		void Draw(glm::mat4 vertexToScreen, Device& d, VkExtent2D viewportSize, VkCommandBuffer& cmdBuffer);
		void Flush();	// call at end of frame, clears out previous tri data

		void AddTriangles(const PosColourVertex* vertices, uint32_t count, bool depthTestEnabled = false);
		void AddTriangle(const PosColourVertex vertices[3], bool depthTestEnabled = false);
		void AddTriangle(const PosColourVertex& v0, const PosColourVertex& v1, const PosColourVertex& v2, bool depthTestEnabled = false);
		void AddLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 colour, bool depthTestEnabled = false);
		void AddLine(const PosColourVertex vertices[2], bool depthTestEnabled = false);
		void AddLines(const PosColourVertex* vertices, int linecount, bool depthTestEnabled = false);
		void AddAxisAtPoint(glm::vec3 position, float scale = 1.0f, glm::mat4 transform = glm::identity<glm::mat4>(), bool depthTestEnabled = false);
		void AddFrustum(const Frustum& f, glm::vec4 colour, bool depthTestEnabled = false);
		void AddCubeWireframe(glm::mat4 transform, glm::vec4 colour, bool depthTestEnabled = false);
		void AddSphere(glm::vec3 center, float radius, glm::vec4 colour, glm::mat4 transform = glm::identity<glm::mat4>(), bool depthTestEnabled = false);
		void DrawAABB(glm::vec3 minbound, glm::vec3 maxbound, glm::mat4 transform, glm::vec4 colour, bool depthTestEnabled = false);

	private:
		bool CreatePipelines(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat);

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
		std::vector<PosColourVertex> m_thisFramePosColVertices;
		std::vector<DrawData> m_thisFrameTriangles;
		std::vector<DrawData> m_thisFrameLines;
		std::vector<DrawData> m_thisFrameTrianglesWithDepth;
		std::vector<DrawData> m_thisFrameLinesWithDepth;
		PerFrameData m_perFrameData[c_framesInFlight];
		int m_currentFrameIndex = 0;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_depthReadDisabledTriPipeline = VK_NULL_HANDLE;
		VkPipeline m_depthReadDisabledLinesPipeline = VK_NULL_HANDLE;
		VkPipeline m_depthEnabledTriPipeline = VK_NULL_HANDLE;
		VkPipeline m_depthEnabledLinesPipeline = VK_NULL_HANDLE;
	};
}