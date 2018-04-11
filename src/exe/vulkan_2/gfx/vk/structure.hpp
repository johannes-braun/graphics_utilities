#pragma once

#include <vulkan/vulkan.h>

namespace gfx::vk
{
    template<VkStructureType SType>
    struct structure {
    private:
        VkStructureType _stype = SType;
    public:
        const void* _next = nullptr;
    };
}