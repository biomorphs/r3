#pragma once

#include "engine/systems.h"
#include "core/glm_headers.h"
#include "render/writeonly_gpu_buffer.h"

namespace R3
{
	struct Pointlight					// uploaded to gpu
	{
		glm::vec4 m_positionDistance;	// w = distance used for attenuation/culling
		glm::vec4 m_colourBrightness;	// w = brightness
	};

	class LightsSystem : public System
	{
	public:
		static std::string_view GetName() { return "LightsSystem"; }
		virtual void RegisterTickFns();
		virtual void Shutdown();
		VkDeviceAddress GetPointlightsDeviceAddress();
		uint32_t GetFirstPointlightOffset();
		uint32_t GetTotalPointlightsThisFrame();
	private:
		bool DrawLightBounds();
		void OnMainPassBegin(class Device& d, VkCommandBuffer cmds);
		bool m_drawBounds = false;
		uint64_t m_onMainPassBeginToken = -1;
		const uint32_t c_maxLights = 1024 * 32;
		const uint32_t c_framesInFlight = 3;	// lights update every frame, need multiple buffers
		WriteOnlyGpuArray<Pointlight> m_allPointlights;	// c_maxLights x c_framesInFlight point lights
		uint32_t m_currentInFrameOffset = 0;	// offset into m_allPointlights, changes every frame
		uint32_t m_totalPointlightsThisFrame = 0;
	};
}