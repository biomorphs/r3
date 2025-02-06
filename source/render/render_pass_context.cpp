#include "render_pass_context.h"
#include "render_target_cache.h"

namespace R3
{
	RenderTarget* RenderPassContext::GetResolvedTarget(const RenderTargetInfo& info)
	{
		return GetResolvedTarget(info.m_name);
	}

	RenderTarget* RenderPassContext::GetResolvedTarget(std::string_view targetName)
	{
		auto found = std::find_if(m_resolvedTargets.begin(), m_resolvedTargets.end(), [&](RenderTarget* t) {
			return t->m_info.m_name == targetName;
		});
		if (found != m_resolvedTargets.end())
		{
			return *found;
		}
		return nullptr;
	}
}