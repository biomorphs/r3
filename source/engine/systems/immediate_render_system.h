#pragma once
#include "engine/systems.h"
#include "render/immediate_renderer.h"
#include <memory>

namespace R3
{
	// owns + wraps immediate renderer, used for drawing shapes, lines, debug stuff
	// doesn't really do anything itself
	class ImmediateRenderSystem : public System
	{
	public:
		ImmediateRenderSystem();
		virtual ~ImmediateRenderSystem();
		static std::string_view GetName() { return "ImmediateRender"; }
		virtual bool Init();

		void OnMainPassBegin(class RenderPassContext&);
		void OnMainPassDraw(class RenderPassContext&);

		std::unique_ptr<ImmediateRenderer> m_imRender;
		bool m_initialised = false;
	};
}