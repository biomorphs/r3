#include "vulkan_helpers.h"
#include "core/profiler.h"
#include "core/file_io.h"
#include "core/log.h"
#include <vulkan/vk_enum_string_helper.h>
#include <string>
#include <cassert>

namespace R3
{
	namespace VulkanHelpers
	{
		bool CheckResult(const VkResult& r)
		{
			if (r)
			{
				std::string errorString = string_VkResult(r);
				LogError("Vulkan Error! {}", errorString);
				int* crashMe = nullptr;
				*crashMe = 1;
			}
			return !r;
		}

		bool RunCommandsImmediate(VkDevice d, VkQueue cmdQueue, VkCommandPool cmdPool, VkFence waitFence, std::function<void(VkCommandBuffer&)> fn)
		{
			// create a temporary cmd buffer from the pool
			VkCommandBufferAllocateInfo allocInfo = { 0 };
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = cmdPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			if (!CheckResult(vkAllocateCommandBuffers(d, &allocInfo, &commandBuffer)))
			{
				LogError("Failed to create cmd buffer");
				return false;
			}

			VkCommandBufferBeginInfo beginInfo = { 0 };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// we will only submit this buffer once
			if (!CheckResult(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
			{
				LogError("Failed to begin cmd buffer");
				return false;
			}

			// run the passed fs
			fn(commandBuffer);

			CheckResult(vkEndCommandBuffer(commandBuffer));

			// submit the cmd buffer to the queue, passing the immediate fence
			VkSubmitInfo submitInfo = { 0 };
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			// Submit + wait for the fence
			CheckResult(vkQueueSubmit(cmdQueue, 1, &submitInfo, waitFence));
			CheckResult(vkWaitForFences(d, 1, &waitFence, true, 9999999999));
			CheckResult(vkResetFences(d, 1, &waitFence));

			// We can free the cmd buffer
			vkFreeCommandBuffers(d, cmdPool, 1, &commandBuffer);

			return true;
		}

		VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& srcSpirv)
		{
			R3_PROF_EVENT();
			assert((srcSpirv.size() % 4) == 0);	// VkShaderModuleCreateInfo wants uint32, make sure the buffer is big enough 
			VkShaderModuleCreateInfo createInfo = { 0 };
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.pCode = reinterpret_cast<const uint32_t*>(srcSpirv.data());
			createInfo.codeSize = srcSpirv.size();	// size in BYTES
			VkShaderModule shaderModule = { 0 };
			VkResult r = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
			if (CheckResult(r))
			{
				return shaderModule;
			}
			else
			{
				LogError("Failed to create shader module from src spirv");
				return VkShaderModule{ 0 };
			}
		}

		VkShaderModule LoadShaderModule(VkDevice device, std::string_view filePath)
		{
			R3_PROF_EVENT();
			VkShaderModule result = VK_NULL_HANDLE;
			std::string fullPath = std::string(R3::FileIO::GetBasePath()) + filePath.data();
			std::vector<uint8_t> spirv;
			if (!R3::FileIO::LoadBinaryFile(fullPath, spirv))
			{
				LogError("Failed to load spirv file {}", fullPath);
			}
			else
			{
				result = CreateShaderModule(device, spirv);
			}
			return result;
		}

		VkPipelineShaderStageCreateInfo CreatePipelineShaderState(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint)
		{
			R3_PROF_EVENT();
			VkPipelineShaderStageCreateInfo stageInfo = { 0 };
			stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo.stage = stage;
			stageInfo.module = shader;
			stageInfo.pName = entryPoint;
			return stageInfo;
		}

		VkPipelineDynamicStateCreateInfo CreatePipelineDynamicState(const std::vector<VkDynamicState>& statesToEnable)
		{
			R3_PROF_EVENT();
			VkPipelineDynamicStateCreateInfo dynamicState = { 0 };
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = static_cast<uint32_t>(statesToEnable.size());
			if (statesToEnable.size() > 0)
			{
				dynamicState.pDynamicStates = statesToEnable.data();
			}
			return dynamicState;
		}

		VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyState(VkPrimitiveTopology topology, bool enablePrimitiveRestart)
		{
			R3_PROF_EVENT();
			VkPipelineInputAssemblyStateCreateInfo inputAssembly = { 0 };
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = topology;
			inputAssembly.primitiveRestartEnable = enablePrimitiveRestart;	// used with strips to break up lines/tris
			return inputAssembly;
		}

		VkPipelineMultisampleStateCreateInfo CreatePipelineMultiSampleState_SingleSample()
		{
			R3_PROF_EVENT();
			VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;	// no MSAA pls
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// just 1 sample thx
			multisampling.minSampleShading = 1.0f; // Optional
			multisampling.pSampleMask = nullptr; // Optional
			multisampling.alphaToCoverageEnable = VK_FALSE; // Optional, outputs alpha to coverage for blending
			multisampling.alphaToOneEnable = VK_FALSE; // Optional, sets alpha to one for all samples
			return multisampling;
		}

		VkPipelineRasterizationStateCreateInfo CreatePipelineRasterState(VkPolygonMode polyMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
		{
			R3_PROF_EVENT();
			VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;		// if true fragments outsize max depth are clamped, not discarded
														// (requires enabling a device feature!)
			rasterizer.depthBiasEnable = VK_FALSE;			// no depth biasing pls
			rasterizer.lineWidth = 1.0f;					// sensible default
			rasterizer.rasterizerDiscardEnable = VK_FALSE;	// set to true to disable raster stage!
			rasterizer.polygonMode = polyMode;
			rasterizer.cullMode = cullMode;
			rasterizer.frontFace = frontFace;
			return rasterizer;
		}

		VkPipelineColorBlendAttachmentState CreatePipelineColourBlendAttachment_NoBlending()
		{
			R3_PROF_EVENT();
			VkPipelineColorBlendAttachmentState attachment = { 0 };
			attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			attachment.blendEnable = VK_FALSE;					// No blending
			attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
			return attachment;
		}

		VkPipelineColorBlendStateCreateInfo CreatePipelineColourBlendState(const std::vector<VkPipelineColorBlendAttachmentState>& attachments)
		{
			R3_PROF_EVENT();
			VkPipelineColorBlendStateCreateInfo blending = { 0 };
			blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blending.logicOpEnable = VK_FALSE;		// no logical ops on write pls
			blending.logicOp = VK_LOGIC_OP_COPY;	// Optional
			blending.attachmentCount = static_cast<uint32_t>(attachments.size());
			blending.pAttachments = attachments.data();
			blending.blendConstants[0] = 0.0f; // Optional (r component of blending constant)
			blending.blendConstants[1] = 0.0f; // Optional (g component of blending constant)
			blending.blendConstants[2] = 0.0f; // Optional (b component of blending constant)
			blending.blendConstants[3] = 0.0f; // Optional (a component of blending constant)
			return blending;
		}
	}
}