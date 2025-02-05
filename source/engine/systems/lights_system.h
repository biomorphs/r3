#pragma once

#include "engine/systems.h"
#include "core/glm_headers.h"
#include "render/writeonly_gpu_buffer.h"
#include "render/render_target_cache.h"

namespace R3
{
	struct Pointlight					// uploaded to gpu every frame
	{
		glm::vec4 m_positionDistance;	// w = distance used for attenuation/culling
		glm::vec4 m_colourBrightness;	// w = brightness
	};

	struct ShadowMetadata				// all info about shadow maps, uploaded as part of AllLights data
	{
		static const int c_maxShadowCascades = 4;
		glm::mat4 m_sunShadowCascadeMatrices[c_maxShadowCascades];	// world->light transform for each cascade
		float m_sunShadowCascadeDistances[c_maxShadowCascades] = { 0 };	// distance from near plane for each cascade
		uint32_t m_sunShadowCascadeCount = 0;						// num cascades
		uint32_t m_padding[3];
	};

	struct AllLights					// uploaded to gpu every frame
	{
		ShadowMetadata m_shadows;						// shadow map data
		glm::vec4 m_sunDirectionBrightness = { 0,-1,0,0 };
		glm::vec4 m_sunColourAmbient = { 0,0,0,0 };		// sun colour + ambient factor
		glm::vec4 m_skyColourAmbient = { 0,0,0,0 };		// sky colour + ambient factor
		VkDeviceAddress m_pointLightsBufferAddress = 0;		// address of the point light buffer
		uint32_t m_pointlightCount = 0;						// total point lights this frame
	};

	class DescriptorSetSimpleAllocator;
	class LightsSystem : public System
	{
	public:
		static std::string_view GetName() { return "LightsSystem"; }
		virtual void RegisterTickFns();
		bool Init();
		void PrepareForDrawing(class RenderPassContext& ctx);		// call this from render graph before accessing light data
		VkDeviceAddress GetAllLightsDeviceAddress();				// get the address of m_allLightsData for this frame
		glm::vec3 GetSkyColour();
		glm::mat4 GetSunShadowMatrix(float minDepth, float maxDepth);	// light-space matrix for sun shadows
		const std::vector<Pointlight>& GetActivePointLights() { return m_allPointLightsCPU; }
		
		int GetShadowCascadeCount();
		glm::mat4 GetShadowCascadeMatrix(int cascade);
		RenderTargetInfo GetShadowCascadeTargetInfo(int cascade);

		VkDescriptorSetLayout_T* GetShadowMapDescriptorLayout();		// used to create pipelines that accept the array of shadow maps
		VkDescriptorSet_T* GetAllShadowMapsSet();						// descriptor set for this frame containing all shadow maps

	private:
		bool CollectAllLights();
		bool ShowGui();
		bool DrawLightBounds();
		bool m_showGui = false;
		bool m_drawBounds = false;

		std::vector<float> m_sunShadowCascades;	// z-distance for each cascade frustum (0-1)

		// maintain a descriptor set containing all shadow map textures, and a buffer of data describing the shadow maps
		VkSampler_T* m_shadowSampler = nullptr;		// passed via descriptor set
		VkDescriptorSetLayout_T* m_shadowMapDescriptorLayout = nullptr;
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSet_T* m_allShadowMaps = nullptr;	// 

		glm::vec3 m_skyColour = { 0,0,0 };		// sky colour from envi
		glm::vec3 m_sunDirection = { 0,-1,0 };	// sun direction from env
		const uint32_t c_maxLights = 1024 * 32;
		const uint32_t c_framesInFlight = 3;	// lights update every frame, need multiple buffers
		uint32_t m_currentFrame = 0;			// offset into m_allPointlights and m_allLightsData
		std::vector<Pointlight> m_allPointLightsCPU;	// keep a cpu-side array of all active point lights for this frame
		WriteOnlyGpuArray<Pointlight> m_allPointlights;	// c_maxLights x c_framesInFlight point lights
		WriteOnlyGpuArray<AllLights> m_allLightsData;	// c_framesInFlight * AllLights
		uint32_t m_totalPointlightsThisFrame = 0;
	};
}