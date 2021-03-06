#pragma once

#include "geometry.hpp"

namespace gfx
{
	inline namespace v1
	{
		template<typename T, size_t Dim, size_t Align>
		bool intersects(gfx::bounds<T, Dim, Align> a, const gfx::bounds<T, Dim, Align>& b) noexcept
		{
			return a.clip(b).empty();
		}
	}
}