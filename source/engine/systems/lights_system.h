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
		glm::vec4 m_colourBrightness;	// w = brightness, could be pre-multiplied
	};

	struct Spotlight					// uploaded to gpu
	{
		glm::vec4 m_positionDistance;		// w = distance used for attenuation/culling
		glm::vec4 m_colourOuterAngle;		// colour is premultiplied by brightness, w = outer angle
		glm::vec4 m_directionInnerAngle;	// direction is normalized, w = inner angle
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
		VkDeviceAddress m_pointLightsBufferAddress = 0;	// address of the point light buffer
		VkDeviceAddress m_spotLightsBufferAddress = 0;	// address of the spot lights buffer
		uint32_t m_pointlightCount = 0;					// total point lights this frame
		uint32_t m_spotlightCount = 0;					// total spot lights this frame
	};

	class DescriptorSetSimpleAllocator;
	class LightsSystem : public System
	{
	public:
		static std::string_view GetName() { return "LightsSystem"; }
		virtual void RegisterTickFns();
		void PrepareForDrawing(class RenderPassContext& ctx);		// call this from render graph before accessing light data
		void PrepareShadowMaps(class RenderPassContext& ctx);		// call this in a pass with shadow maps as inputs
		VkDeviceAddress GetAllLightsDeviceAddress();				// get the address of m_allLightsData for this frame
		glm::vec3 GetSkyColour();
		glm::mat4 GetSunShadowMatrix(float minDepth, float maxDepth, int shadowMapResolution);	// light-space matrix for sun shadows
		uint32_t GetActivePointLights() { return m_activePointLights; }
		
		int GetShadowCascadeCount();
		glm::mat4 GetShadowCascadeMatrix(int cascade);
		RenderTargetInfo GetShadowCascadeTargetInfo(int cascade);
		void GetShadowCascadeDepthBiasSettings(int cascade, float& constant, float& clamp, float& slope);

		VkDescriptorSetLayout_T* GetShadowMapDescriptorLayout();		// used to create pipelines that accept the array of shadow maps
		VkDescriptorSet_T* GetAllShadowMapsSet();						// descriptor set for this frame containing all shadow maps

	private:
		glm::mat4 CalculateSpotlightMatrix(glm::vec3 position, glm::vec3 direction, float maxDistance, float outerAngle);	// returns a projection*view matrix for a given spotlight
		bool CollectAllLights();
		bool ShowGui();
		bool DrawLightBounds();
		bool m_showGui = false;
		bool m_drawBounds = false;
		bool m_drawCascadeFrusta = false;
		bool m_lockDebugFrustums = false;

		struct ShadowCascadeSettings {
			float m_distance = 0.0f;		// 0 - 1, fraction of near-far plane of main camera
			int m_resolution = 2048;		// texture size
			float m_depthBiasConstantFactor = 0.0f;	// constant bias
			float m_depthBiasClamp = 0.0f;	// bias clamp
			float m_depthSlopeBias = 0.0f;	// slope bias
		};
		std::vector<ShadowCascadeSettings> m_sunShadowCascades;

		// maintain a descriptor set containing all shadow map textures, and a buffer of data describing the shadow maps
		VkSampler_T* m_shadowSampler = nullptr;		// passed via descriptor set
		VkDescriptorSetLayout_T* m_shadowMapDescriptorLayout = nullptr;
		std::unique_ptr<DescriptorSetSimpleAllocator> m_descriptorAllocator;
		VkDescriptorSet_T* m_allShadowMaps = nullptr;	// 

		glm::vec3 m_skyColour = { 0,0,0 };		// sky colour from envi
		glm::vec3 m_sunDirection = { 0,-1,0 };	// sun direction from env
		const uint32_t c_maxPointLights = 1024 * 4;
		const uint32_t c_maxSpotLights = 1024 * 4;
		const uint32_t c_framesInFlight = 3;	// lights update every frame, need multiple buffers
		uint32_t m_currentFrame = 0;			// offset into m_allPointlights and m_allLightsData
		uint32_t m_activePointLights = 0;		// track how many point lights are active this frame
		uint32_t m_activeSpotLights = 0;
		WriteOnlyGpuArray<Pointlight> m_allPointlights;	// c_maxPointLights x c_framesInFlight point lights
		WriteOnlyGpuArray<Spotlight> m_allSpotlights;	// c_maxSpotLights x c_framesInFlight spot lights
		WriteOnlyGpuArray<AllLights> m_allLightsData;	// c_framesInFlight * AllLights
	};
}