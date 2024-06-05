#include "immediate_renderer.h"
#include "vulkan_helpers.h"
#include "device.h"
#include "swap_chain.h"
#include "pipeline_builder.h"
#include "buffer_pool.h"
#include "engine/frustum.h"
#include "core/log.h"
#include "core/profiler.h"
#include <array>

namespace R3
{
	using VulkanHelpers::CheckResult;

	struct PushConstants {
		glm::mat4 m_vertexToScreen;
		VkDeviceAddress m_vertexBufferAddress;
	};

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
		if (newLine.m_startVertexOffset + 2 <= m_maxVertices)
		{
			newLine.m_vertexCount = 2;
			m_thisFrameVertices.emplace_back(vertices[0]);
			m_thisFrameVertices.emplace_back(vertices[1]);
			m_thisFrameLines.emplace_back(newLine);
		}
	}

	void ImmediateRenderer::AddLines(const PerVertexData* vertices, int linecount)
	{
		DrawData newLines;
		newLines.m_startVertexOffset = static_cast<uint32_t>(m_thisFrameVertices.size());
		if (newLines.m_startVertexOffset + (linecount * 2) <= m_maxVertices)
		{
			newLines.m_vertexCount = linecount * 2;
			m_thisFrameVertices.insert(m_thisFrameVertices.end(), vertices, vertices + (linecount * 2));
			m_thisFrameLines.emplace_back(newLines);
		}
	}

	void ImmediateRenderer::AddAxisAtPoint(glm::vec3 position, float scale, glm::mat4 transform)
	{
		glm::vec4 p0 = glm::vec4(position, 1.0f);
		glm::vec3 extents = glm::mat3(transform) * glm::vec3(1, 1, 1);
		PerVertexData vertices[6] = {
			{ p0, {1.0f,0.0f,0.0f,1.0f} },
			{ p0 + glm::vec4(extents.x,0,0,0), {1.0f,0.0f,0.0f,1.0f} },
			{ p0, {0.0f,1.0f,0.0f,1.0f} },
			{ p0 + glm::vec4(0,extents.y,0,0), {0.0f,1.0f,0.0f,1.0f} },
			{ p0, {0.0f,0.0f,1.0f,1.0f} },
			{ p0 + glm::vec4(0,0,extents.z,0), {0.0f,0.0f,1.0f,1.0f} },
		};
		AddLines(vertices, 3);
	}

	void ImmediateRenderer::AddFrustum(const Frustum& f, glm::vec4 colour)
	{
		const auto& p = f.GetPoints();
		glm::vec4 v[] = {
			{p[0],1.0f},	{p[1],1.0f},	// lbn,	ltn
			{p[1],1.0f},	{p[3],1.0f},	// ltn, rtn
			{p[3],1.0f},	{p[2],1.0f},	// rtn, rbn
			{p[2],1.0f},	{p[0],1.0f},	// rbn, lbn
			{p[0],1.0f},	{p[4],1.0f},	// lbn, lbf
			{p[1],1.0f},	{p[5],1.0f},	// ltn, ltf
			{p[2],1.0f},	{p[6],1.0f},	// rbn, rbf
			{p[3],1.0f},	{p[7],1.0f},	// rtn, rtf
			{p[4],1.0f},	{p[5],1.0f},	// lbf,	ltf
			{p[5],1.0f},	{p[7],1.0f},	// ltf, rtf
			{p[7],1.0f},	{p[6],1.0f},	// rtf, rbf
			{p[6],1.0f},	{p[4],1.0f},	// rbf, lbf
		};
		constexpr int c_vertexCount = sizeof(v) / sizeof(v[0]);
		PerVertexData vertices[c_vertexCount];
		for (int i = 0; i < c_vertexCount; ++i)
		{
			vertices[i] = { v[i], colour };
		}
		AddLines(vertices, c_vertexCount / 2);
	}

	void ImmediateRenderer::DrawAABB(glm::vec3 minbound, glm::vec3 maxbound, glm::mat4 transform, glm::vec4 colour)
	{
		ImmediateRenderer::PerVertexData lines[] = {
			{ {minbound.x,minbound.y,minbound.z,1}, colour},
			{ {maxbound.x,minbound.y,minbound.z,1}, colour},
			{ {minbound.x,minbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,minbound.y,maxbound.z,1}, colour},
			{ {minbound.x,maxbound.y,minbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,minbound.z,1}, colour},
			{ {minbound.x,maxbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,maxbound.z,1}, colour},

			{ {minbound.x,minbound.y,minbound.z,1}, colour},
			{ {minbound.x,minbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,minbound.y,minbound.z,1}, colour},
			{ {maxbound.x,minbound.y,maxbound.z,1}, colour},
			{ {minbound.x,maxbound.y,minbound.z,1}, colour},
			{ {minbound.x,maxbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,minbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,maxbound.z,1}, colour},

			{ {minbound.x,minbound.y,minbound.z,1}, colour},
			{ {minbound.x,maxbound.y,minbound.z,1}, colour},
			{ {minbound.x,minbound.y,maxbound.z,1}, colour},
			{ {minbound.x,maxbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,minbound.y,minbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,minbound.z,1}, colour},
			{ {maxbound.x,minbound.y,maxbound.z,1}, colour},
			{ {maxbound.x,maxbound.y,maxbound.z,1}, colour},
		};
		int vertexCount = (sizeof(lines) / sizeof(lines[0]));
		for (int v = 0; v < vertexCount; ++v)
		{
			lines[v].m_position = transform * lines[v].m_position;
		}
		AddLines(lines, vertexCount / 2);
	}

	void ImmediateRenderer::AddCubeWireframe(glm::mat4 transform, glm::vec4 colour)
	{
		ImmediateRenderer::PerVertexData testLines[] = {
			{ {-1,-1,-1,1}, colour},
			{ {1,-1,-1,1}, colour},

			{ {1,-1,-1,1}, colour},
			{ {1,1,-1,1}, colour},

			{ {1,1,-1,1}, colour},
			{ {-1,1,-1,1}, colour},

			{ {-1,1,-1,1}, colour},
			{ {-1,-1,-1,1}, colour},

			{ {-1,-1,1,1}, colour},
			{ {1,-1,1,1}, colour},

			{ {1,-1,1,1}, colour},
			{ {1,1,1,1}, colour},

			{ {1,1,1,1}, colour},
			{ {-1,1,1,1}, colour},

			{ {-1,1,1,1}, colour},
			{ {-1,-1,1,1}, colour},

			{ {-1,-1,-1,1}, colour},
			{ {-1,-1,1,1}, colour},

			{ {1,-1,-1,1}, colour},
			{ {1,-1,1,1}, colour},

			{ {1,1,-1,1}, colour},
			{ {1,1,1,1}, colour},

			{ {-1,1,-1,1}, colour},
			{ {-1,1,1,1}, colour},
		};
		int vertexCount = (sizeof(testLines) / sizeof(testLines[0]));
		for (int v = 0; v < vertexCount; ++v)
		{
			testLines[v].m_position = transform * testLines[v].m_position;
		}
		AddLines(testLines, vertexCount / 2);
	}

	void ImmediateRenderer::AddSphere(glm::vec3 center, float radius, glm::vec4 colour, glm::mat4 transform)
	{
		const int c_numVerticesPerSlice = 16;
		const int c_numSlices = 16;
		const float c_thetaDelta = (2.0f * glm::pi<float>() / (float)c_numVerticesPerSlice);	// 360 degrees per slice
		const float c_sliceThetaDelta = glm::pi<float>() / (float)c_numSlices;	// -90 to 90 on vertical (top to bottom)
		for (int slice = 0; slice < c_numSlices; ++slice)
		{
			const float sliceTheta = -(glm::pi<float>() / 2.0f) + slice * c_sliceThetaDelta;
			const float nextSliceTheta = sliceTheta + c_sliceThetaDelta;
			const float y = center.y + radius * sin(sliceTheta);
			const float nextY = center.y + radius * sin(nextSliceTheta);
			const float sliceRadius = radius * cosf(sliceTheta);
			const float nextSliceRadius = radius * cosf(sliceTheta + c_sliceThetaDelta);
			for (int vertex = 0; vertex < c_numVerticesPerSlice; ++vertex)
			{
				const float thisTheta = vertex * c_thetaDelta;
				const float nextTheta = (vertex + 1) * c_thetaDelta;

				const float cosThisTheta = cosf(thisTheta);
				const float sinThisTheta = sinf(thisTheta);

				// horizontal line first
				const float thisX = center.x + cosThisTheta * sliceRadius;
				const float thisZ = center.z + sinThisTheta * sliceRadius;
				const float nextX = center.x + cosf(nextTheta) * sliceRadius;
				const float nextZ = center.z + sinf(nextTheta) * sliceRadius;
				glm::vec4 p0(thisX, y, thisZ, 1.0f), p1(nextX, y, nextZ, 1.0f);
				p0 = transform * p0;
				p1 = transform * p1;
				AddLine(glm::vec3(p0), glm::vec3(p1), colour);

				// vertical line to next slice up
				p1.x = center.x + cosThisTheta * nextSliceRadius;
				p1.y = nextY;
				p1.z = center.z + sinThisTheta * nextSliceRadius;
				p1.w = 1.0f;
				p1 = transform * p1;
				AddLine(glm::vec3(p0), glm::vec3(p1), colour);
			}
		}
	}

	void ImmediateRenderer::AddTriangle(const PerVertexData vertices[3])
	{
		DrawData newTri;
		newTri.m_startVertexOffset = static_cast<uint32_t>(m_thisFrameVertices.size());
		if (newTri.m_startVertexOffset + 3 <= m_maxVertices)
		{
			newTri.m_vertexCount = 3;
			m_thisFrameVertices.emplace_back(vertices[0]);
			m_thisFrameVertices.emplace_back(vertices[1]);
			m_thisFrameVertices.emplace_back(vertices[2]);
			m_thisFrameTriangles.emplace_back(newTri);
		}
	}

	void ImmediateRenderer::AddLine(glm::vec3 p0, glm::vec3 p1, glm::vec4 colour)
	{
		PerVertexData verts[2];
		verts[0].m_colour = colour;
		verts[0].m_position = glm::vec4(p0,1);
		verts[1].m_colour = colour;
		verts[1].m_position = glm::vec4(p1,1);
		AddLine(verts);
	}

	void ImmediateRenderer::WriteVertexData(Device& d, BufferPool& stagingBuffers, VkCommandBuffer& cmdBuffer)
	{
		R3_PROF_EVENT();
		auto& pfd = m_perFrameData[m_currentFrameIndex];
		size_t dataToCopy = glm::min(m_maxVertices, m_thisFrameVertices.size()) * sizeof(PerVertexData);
		if (dataToCopy > 0)
		{
			auto staging = stagingBuffers.GetBuffer(dataToCopy, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO);
			if (staging)
			{
				// host write is coherant, so no flush is required, just memcpy the current frame data
				// the write is guaranteed to complete when the cmd buffer is submitted to the queue
				memcpy(staging->m_mappedBuffer, m_thisFrameVertices.data(), dataToCopy);

				// now we need to copy the data from staging to the actual buffer using the graphics queue
				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = pfd.m_vertexOffset * sizeof(PerVertexData);
				copyRegion.size = dataToCopy;	// may need to adjust for alignment?
				vkCmdCopyBuffer(cmdBuffer, staging->m_buffer.m_buffer, m_allVertexData.m_buffer, 1, &copyRegion);

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

				// release the staging buffer, we are done
				stagingBuffers.Release(*staging);
			}
			else
			{
				LogError("Failed to allocate staging buffer");
			}
		}
	}

	void ImmediateRenderer::Draw(glm::mat4 vertexToScreen, Device& d, VkExtent2D viewportSize, VkCommandBuffer& cmdBuffer)
	{
		R3_PROF_EVENT();

		auto& pfd = m_perFrameData[m_currentFrameIndex];

		// pipeline dynamic viewport + scissor based on swapchain extents
		VkViewport viewport = { 0 };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)viewportSize.width;
		viewport.height = (float)viewportSize.height;
		viewport.minDepth = 0.0f;	// normalised! must be between 0 and 1
		viewport.maxDepth = 1.0f;	// ^^
		VkRect2D scissor = { 0 };
		scissor.offset = { 0, 0 };
		scissor.extent = viewportSize;	// draw the full image

		// draw the triangles
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthReadDisabledTriPipeline);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		// pass the vertex to screen transform via push constants along with the buffer address
		PushConstants pc;
		pc.m_vertexToScreen = vertexToScreen;
		pc.m_vertexBufferAddress = m_allvertsBufferAddress;
		vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
		for (auto& tris : m_thisFrameTriangles)
		{
			vkCmdDraw(cmdBuffer, tris.m_vertexCount, 1, tris.m_startVertexOffset + pfd.m_vertexOffset, 0);
		}

		// Draw the lines
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depthReadDisabledLinesPipeline);
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
		for (auto& lines : m_thisFrameLines)
		{
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

	bool ImmediateRenderer::CreateNoDepthReadPipelines(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat)
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

		// Set up empty vertex data input state
		pb.m_vertexInputState = VulkanHelpers::CreatePipelineEmptyVertexInputState();

		// Input assembly describes type of geometry (lines/tris) and topology(strips,lists,etc) to draw
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		// create the viewport state for the pipeline, we only need to set the counts when using dynamic state
		pb.m_viewportState = VulkanHelpers::CreatePipelineDynamicViewportState();

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
		m_depthReadDisabledTriPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &colourBufferFormat, depthBufferFormat);
		if (m_depthReadDisabledTriPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create depth-read-disabled triangle pipeline!");
			return false;
		}

		// build another similar pipeline but with line primitives + no backface culling
		pb.m_inputAssemblyState = VulkanHelpers::CreatePipelineInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		pb.m_rasterState = VulkanHelpers::CreatePipelineRasterState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		m_depthReadDisabledLinesPipeline = pb.Build(d.GetVkDevice(), m_pipelineLayout, 1, &colourBufferFormat, depthBufferFormat);
		if (m_depthReadDisabledLinesPipeline == VK_NULL_HANDLE)
		{
			LogError("Failed to create depth-read-disabled lines pipeline!");
			return false;
		}

		vkDestroyShaderModule(d.GetVkDevice(), vertexShader, nullptr);
		vkDestroyShaderModule(d.GetVkDevice(), fragShader, nullptr);

		return true;
	}

	bool ImmediateRenderer::Initialise(Device& d, VkFormat colourBufferFormat, VkFormat depthBufferFormat, uint32_t maxVerticesPerFrame)
	{
		R3_PROF_EVENT();
		size_t bufferSize = maxVerticesPerFrame * c_framesInFlight * sizeof(PerVertexData);
		m_allVertexData = VulkanHelpers::CreateBuffer(d.GetVMA(), bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		m_allvertsBufferAddress = VulkanHelpers::GetBufferDeviceAddress(d.GetVkDevice(), m_allVertexData);
		if (m_allVertexData.m_allocation == VK_NULL_HANDLE)
		{
			LogError("Failed to create vertex buffer of size {} bytes", bufferSize);
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
		constantRange.size = sizeof(PushConstants);
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

		// create pipeline compatible with back buffer + depth buffer
		if (!CreateNoDepthReadPipelines(d, colourBufferFormat, depthBufferFormat))
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

		// Destroy the vertex buffers
		vmaDestroyBuffer(d.GetVMA(), m_allVertexData.m_buffer, m_allVertexData.m_allocation);
	}
}