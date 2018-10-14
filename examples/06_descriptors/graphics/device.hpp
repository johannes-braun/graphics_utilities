#pragma once

#include "extensions.hpp"
#include "gsl/span"
#include "types.hpp"
#include <vulkan/vulkan.hpp>
#include <chrono>

struct VmaAllocator_T;

namespace gfx {
inline namespace v1 {
enum class queue_type
{
    graphics,
    compute,
    transfer,
    present
};

enum class device_target
{
    igpu,
    gpu,
    cpu,
    vgpu
};

class instance;
class surface;
class commands;
class queue;
using allocator = VmaAllocator_T*;
class fence;

class device
{
public:
	explicit device(instance& i, device_target target, vk::ArrayProxy<const float> graphics_priorities,
           vk::ArrayProxy<const float> compute_priorities, opt_ref<surface> surface = std::nullopt);

    const queue& graphics_queue(u32 index = 0) const noexcept;

    const queue& compute_queue(u32 index = 0) const noexcept;
    const queue& transfer_queue() const noexcept;
    const queue& present_queue() const noexcept;

    const u32& graphics_family() const noexcept;
    const u32& compute_family() const noexcept;
    const u32& transfer_family() const noexcept;
    const u32& present_family() const noexcept;

    const vk::Device&         dev() const noexcept;
    const vk::PhysicalDevice& gpu() const noexcept;
    allocator          alloc() const noexcept;
    const extension_dispatch& dispatcher() const noexcept;

    std::vector<commands> allocate_graphics_commands(u32 count, bool primary = true) const noexcept;
    commands allocate_graphics_command(bool primary = true) const noexcept;

	void wait_for(cref_array_view<fence> fences, bool all = true, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max());
	void reset_fences(cref_array_view<fence> fences);

private:
    u32 presentation_family(instance& i, const surface& s, gsl::span<const vk::QueueFamilyProperties> props) const noexcept;

    static std::tuple<u32, u32, u32> dedicated_families(gsl::span<const vk::QueueFamilyProperties> props);

    struct vma_alloc_deleter
    {
        void operator()(allocator alloc) const noexcept;
    };

    vk::PhysicalDevice                                 _gpu = nullptr;
    vk::UniqueDevice                                   _device;
    std::unique_ptr<VmaAllocator_T, vma_alloc_deleter> _allocator;
    extension_dispatch                                 _dispatcher;
    std::array<std::vector<vk::Queue>, 4>              _queues;
    std::array<vk::UniqueCommandPool, 4>               _command_pools;
    std::array<u32, 4>                                 _queue_families{};
};
}    // namespace v1
}    // namespace gfx