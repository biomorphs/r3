#pragma once

#include "entities/component_helpers.h"
#include "engine/graphics/tonemap_compute.h"
#include "core/glm_headers.h"

namespace R3
{
	// Contains global (well, per world) environment settings
	// May get split up eventually
	class EnvironmentSettingsComponent
	{
	public:
		EnvironmentSettingsComponent();
		static std::string_view GetTypeName() { return "EnvironmentSettings"; }
		static void RegisterScripts(class LuaSystem&);
		void SerialiseJson(JsonSerialiser& s);
		void Inspect(const Entities::EntityHandle& e, Entities::World* w, ValueInspector& i);

		glm::vec3 m_skyColour = {};
		glm::vec3 m_sunDirection{ -.1,-1,-.1 };	// normalise before use
		glm::vec3 m_sunColour = {1,1,1};
		float m_sunBrightness = 1.0f;
		float m_skyAmbientFactor = 0.005f;		// ambient from the sky 
		float m_sunAmbientFactor = 0.008f;		// ambient from the sun
		int m_tonemapType = 0;					// MUST map to a value of TonemapCompute::TonemapType
		float m_flyCamFarPlane = 100.0f;		// far plane for fly-cam
		float m_flyCamMoveSpeed = 2.0f;			// default move speed

		// one of these per cascade, up to a max of 4
		struct ShadowCascadeSettings
		{
			int m_textureResolution = 2048;			// size of the cascade's render target in pixels
			float m_depth = 0.0f;					// 0-1, describes how far along z-axis the cascade starts
			float m_depthBiasConstantFactor = 0.0f;	// constant depth value added to each fragment in this cascade
			float m_depthBiasClamp = 0.0f;			// maximum (or minimum) depth bias of a fragment
			float m_depthSlopeBias = 0.0f;			// scalar factor applied to a fragment’s slope in depth bias calculations
			void SerialiseJson(JsonSerialiser& s);
		};
		static const int c_maxCascades = 4;
		ShadowCascadeSettings m_shadowCascades[c_maxCascades];
		int m_shadowCascadeCount = 0;
	};
}