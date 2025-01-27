#pragma once

#include "engine/systems.h"
#include "core/glm_headers.h"
#include "render/writeonly_gpu_buffer.h"

namespace R3
{
	struct Pointlight					// uploaded to gpu every frame
	{
		glm::vec4 m_positionDistance;	// w = distance used for attenuation/culling
		glm::vec4 m_colourBrightness;	// w = brightness
	};

	struct AllLights					// uploaded to gpu every frame
	{
		glm::vec4 m_sunDirectionBrightness = { 0,-1,0,0 };
		glm::vec4 m_sunColourAmbient = { 0,0,0,0 };		// sun colour + ambient factor
		glm::vec4 m_skyColourAmbient = { 0,0,0,0 };		// sky colour + ambient factor
		VkDeviceAddress m_pointLightsBufferAddress = 0;		// address of the point light buffer
		uint32_t m_pointlightCount = 0;						// total point lights this frame
	};

	class LightsSystem : public System
	{
	public:
		static std::string_view GetName() { return "LightsSystem"; }
		virtual void RegisterTickFns();
		void PrepareForDrawing(class RenderPassContext& ctx);		// call this from render graph before accessing light data
		VkDeviceAddress GetAllLightsDeviceAddress();				// get the address of m_allLightsData for this frame
		glm::vec3 GetSkyColour();
		const std::vector<Pointlight>& GetActivePointLights() { return m_allPointLightsCPU; }
	private:
		bool CollectAllLights();
		bool ShowGui();
		bool DrawLightBounds();
		bool m_showGui = false;
		bool m_drawBounds = false;
		glm::vec3 m_skyColour = { 0,0,0 };		// sky colour from envi
		const uint32_t c_maxLights = 1024 * 32;
		const uint32_t c_framesInFlight = 3;	// lights update every frame, need multiple buffers
		uint32_t m_currentFrame = 0;			// offset into m_allPointlights and m_allLightsData
		std::vector<Pointlight> m_allPointLightsCPU;	// keep a cpu-side array of all active point lights for this frame
		WriteOnlyGpuArray<Pointlight> m_allPointlights;	// c_maxLights x c_framesInFlight point lights
		WriteOnlyGpuArray<AllLights> m_allLightsData;	// c_maxLights * AllLights
		uint32_t m_totalPointlightsThisFrame = 0;
	};
}