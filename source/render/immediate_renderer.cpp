#include "immediate_renderer.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "swap_chain.h"
#include "pipeline_builder.h"
#include "core/log.h"
#include "core/profiler.h"
#include <array>

namespace R3
{
	using VulkanHelpers::CheckResult;

	static VkVertexInputBindingDescription CreateVertexInputBindingDescription()
	{
		// we just want to bind a single buffer of vertices
		VkVertexInputBindingDescription bindingDesc = { 0 };
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(ImmediateRenderer::PerVertexData);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDesc;
	}

	static std::array<VkVertexInputAttributeDescription, 2> CreateVertexAttributeDescriptions()
	{
		// 2 attributes, position and colour stored in buffer 0
		std::array<VkVertexInputAttributeDescription, 2> attributes{};
		attributes[0].binding = 0;
		attributes[0].location = 0;	// location visible from shader
		attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;	// pos = vec4
		attributes[0].offset = offsetof(ImmediateRenderer::PerVertexData, m_position);
		attributes[1].binding = 0;
		attributes[1].location = 1;
		attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;	// colour = vec4
		attributes[1].offset = offsetof(ImmediateRenderer::PerVertexData, m_colour);
		return attributes;
	}

	ImmediateRenderer::ImmediateRenderer()
	{
	}

	ImmediateRenderer::~ImmediateRenderer()
	{
	}

	void ImmediateRenderer::AddLine(const PerVertexData vertices[2])
	{
		DrawData newLine;
		newLine.m_startVertexOffset = static_cast<uint32_t>(m_thisFrameVertices.size());
		newLine.m_vertexCount = 2;
		m_thisFrameVertices.emplace_back(vertices[0]);
		m_thisFrameVertices.emplace_back(vertices[1]);
		m_thisFrameLines.emplace_back(newLine);
	}

	void ImmediateRenderer::AddTriangle(const PerVertexData vertices[3])
	{
		DrawData newTri;
		newTri.m_startVertexOffset = static_cast<uint32_t>(m_thisFrameVertices.size());
		newTri.m_vertexCount = 3;
		m_thisFrameVertices.emplace_back(vertices[0]);
		m_thisFrameVertices.emplace_back(vertices[1]);
		m_thisFrameVertices.emplace_back(vertices[2]);
		m_thisFrameTriangles.emplace_back(newTri);
	}

	void ImmediateRenderer::WriteVertexData(Device& d, VkCommandBuffer& cmdBuffer)
	{
		R3_PROF_EVENT();
		auto& pfd = m_perFrameData[m_currentFrameIndex];
		size_t dataToCopy = glm::min(m_maxVertices, m_thisFrameVertices.size()) * sizeof(PerVertexData);
		if (dataToCopy > 0)
		{
			// host write is coherant, so no flush is required, just memcpy the current frame data
			// the write is guaranteed to complete when the cmd buffer is submitted to the queue
			memcpy(m_mappedStagingBuffer, m_thisFrameVertices.data(), dataToCopy);

			// now we need to copy the data from staging to the actual buffer using the graphics queue
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = pfd.m_vertexOffset * sizeof(PerVertexData);
			copyRegion.size = dataToCopy;	// may need to adjust for alignment?
			vkCmdCopyBuffer(cmdBuffer, m_stagingVertexData.m_buffer, m_allVertexData.m_buffer, 1, &copyRegion);

			// use a memory barrier to ensure the transfer finishes before we draw
			VkMemoryBarrier writeBarrier = { 0 };
			writeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			writeBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			writeBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			vkCmdPipelineBarrier(cmdBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,		// src stage = transfer
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,	// dst stage = vertex shader input
				0,									// dependency flags
				1,
				&writeBarrier,
				0, nullptr, 0, nullptr
			);
		}
	}

	void ImmediateRenderer::Draw(glm::mat4 vertexToScreen, Device& d, Swapchain& swapChain, VkCommandBuffer& cmdBuffer)
	{
		R3_PROF_EVENT();

		auto& pfd = m_perFrameData[m_currentFrameIndex];

		// pipeline dynamic viewport + scissor based on swapchain extents
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChain.GetExtents().width;
		viewport.height = (float)swapChain.GetExtents().height;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = swapChain.GetExtents();	// draw the full image

		// draw the triangles
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthReadDisabledTriPipeline);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		
		constexpr VkDeviceSize offset = 0;	// bind the VB with 0 offset, draw calls will include per-frame offset instead
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_allVertexData.m_buffer, &offset);

		// pass the vertex-screen transform via push constants for now (laziness!)
		for (auto& tris : m_thisFrameTriangles)
		{
			vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertexToScreen), &vertexToScreen);
			vkCmdDraw(cmdBuffer, tris.m_vertexCount, 1, tris.m_startVertexOffset + pfd.m_vertexOffset, 0);
		}

		// Draw the lines
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthReadDisabledLinesPipeline);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &m_allVertexData.m_buffer, &offset);
		for (auto& lines : m_thisFrameLines)
		{
			vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vertexToScreen), &vertexToScreen);
			vkCmdDraw(cmdBuffer, lines.m_vertexCount, 1, lines.m_startVertexOffset + pfd.m_vertexOffset, 0);
		}
	}

	void ImmediateRenderer::Flush()
	{
		m_thisFrameTriangles.clear();
		m_thisFrameLines.clear();
		m_thisFrameVertices.clear();
		m_currentFrameIndex = (m_currentFrameIndex + 1) % c_framesInFlight;
	}

	bool ImmediateRenderer::CreateNoDepthReadPipelines(Device& d, Swapchain& swapChain, VkFormat depthBufferFormat)
	{
		// Load the shaders
		std::string basePath = "shaders_spirv\\common\\";
		auto vertexShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "immediate_render.vert.spv");
		auto fragShader = VulkanHelpers::LoadShaderModule(d.GetVkDevice(), basePath + "immediate_render.frag.spv");
		if (vertexShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
		{
			LogError("Failed to create shader modules");
			return false;
		}

		PipelineBuilder pb;
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
		pb.m_shaderStages.push_back(VulkanHelpers::CreatePipelineShaderState(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader));

		// dynamic state must be set each time the pipeline is bound!
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,		// we will set viewport at draw time (lets us handle window resize without recreating pipelines)
			VK_DYNAMIC_STATE_SCISSOR		// same for the scissor data
		};
		pb.m_dynamicState = VulkanHelpers::CreatePipelineDynamicState(dynamicStates);

		// Set up vertex data input state
		auto attribDescriptions = CreateVertexAttributeDescriptions();
		auto inputBindingDescriptor = CreateVertexInputBindingDescription();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 0 };
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = 2;
		vertexInputInfo.pVertexAttributeDescriptions = attribDescriptions.data();
		vertexInputInfo.pVertexBindingDescriptions = &inputBindingDescriptor;
		pb.m_vertexInputState = vertexInputInfo;

		// Input assembly describes type of geometry (lines/tris) and topology(strips,lists,etc) to draw
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		// create the viewport state for the pipeline, we only need to set the counts when using dynamic state
		VkPipelineViewportStateCreateInfo viewportState = { 0 };
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		pb.m_viewportState = viewportState;

		// Setup rasteriser state
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);

		// Multisampling state
		pb.m_multisamplingState = VulkanHelpers::CreatePipelineMultiSampleState_SingleSample();

		// Disable depth read/write
		VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		pb.m_depthStencilState = depthStencilState;

		// No colour attachment blending
		std::vector<VkPipelineColorBlendAttachmentState> allAttachments = {
			VulkanHelpers::CreatePipelineColourBlendAttachment_NoBlending()
		};
		pb.m_colourBlendState = VulkanHelpers::CreatePipelineColourBlendState(allAttachments);	// Pipeline also has some global blending state (constants, logical ops enable)

		// build the pipelines
		m_depthReadDisabledTriPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &swapChain.GetFormat().format, depthBufferFormat);
		if (m_depthReadDisabledTriPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create depth-read-disabled triangle pipeline!");
			return false;
		}

		// build another similar pipeline but with line primitives + no backface culling
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		m_depthReadDisabledLinesPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &swapChain.GetFormat().format, depthBufferFormat);
		if (m_depthReadDisabledLinesPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create depth-read-disabled lines pipeline!");
			return false;
		}

		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);

		return true;
	}

	bool ImmediateRenderer::Initialise(Device& d, Swapchain& swapChain, VkFormat depthBufferFormat, uint32_t maxVerticesPerFrame)
	{
		R3_PROF_EVENT();
		size_t bufferSize = maxVerticesPerFrame * c_framesInFlight * sizeof(PerVertexData);
		m_allVertexData = VulkanHelpers::CreateBuffer(d.GetVMA(), bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		if (m_allVertexData.m_allocation == VK_NULL_HANDLE)
		{
			LogError("Failed to create vertex buffer of size {} bytes", bufferSize);
			return false;
		}
		m_stagingVertexData = VulkanHelpers::CreateBuffer(d.GetVMA(), maxVerticesPerFrame * sizeof(PerVertexData), 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);	// host coherant, write combined)
		if (m_stagingVertexData.m_allocation == VK_NULL_HANDLE)
		{
			LogError("Failed to create staging buffer");
			return false;
		}
		if (!CheckResult(vmaMapMemory(d.GetVMA(), m_stagingVertexData.m_allocation, &m_mappedStagingBuffer)))
		{
			LogError("Failed to map staging buffer memory");
			return false;
		}
		m_thisFrameVertices.reserve(maxVerticesPerFrame);
		m_maxVertices = maxVerticesPerFrame;
		for (int f = 0; f < c_framesInFlight; ++f)
		{
			m_perFrameData[f].m_vertexOffset = f * maxVerticesPerFrame;
		}

		// shared pipeline layout, transform matrix passed via push constant
		VkPushConstantRange constantRange;
		constantRange.offset = 0;	// needs to match in the shader if >0!
		constantRange.size = sizeof(glm::mat4);
		constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		if (!CheckResult(vkCreatePipelineLayout(d.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout)))
		{
			LogError("Failed to create pipeline layout!");
			return false;
		}

		// create pipeline compatible with swap chain + depth buffer
		if (!CreateNoDepthReadPipelines(d, swapChain, depthBufferFormat))
		{
			LogError("Failed to create pipeline");
			return false;
		}

		return true;
	}

	void ImmediateRenderer::Destroy(Device& d)
	{
		R3_PROF_EVENT();

		// destroy the pipeline objects
		vkDestroyPipeline(d.GetVkDevice(), m_depthReadDisabledTriPipeline, nullptr);
		vkDestroyPipeline(d.GetVkDevice(), m_depthReadDisabledLinesPipeline, nullptr);
		vkDestroyPipelineLayout(d.GetVkDevice(), m_pipelineLayout, nullptr);

		vmaUnmapMemory(d.GetVMA(), m_stagingVertexData.m_allocation);

		// Destroy the vertex buffers
		vmaDestroyBuffer(d.GetVMA(), m_allVertexData.m_buffer, m_allVertexData.m_allocation);
		vmaDestroyBuffer(d.GetVMA(), m_stagingVertexData.m_buffer, m_stagingVertexData.m_allocation);
	}
}