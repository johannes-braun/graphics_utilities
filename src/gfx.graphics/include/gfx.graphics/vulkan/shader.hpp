#pragma once

#include <filesystem>
#include <gfx.core/types.hpp>
#include <gsl/span>
#include <optional>
#include <vulkan/vulkan.hpp>

namespace gfx {
inline namespace v1 {
namespace vulkan {
class device;
class shader
{
public:
    explicit shader(device& dev, gsl::span<u32 const> source);

    shader(shader const& other) = delete;
    shader& operator=(shader const& other) = delete;
    shader(shader&& other)                 = default;
    shader& operator=(shader&& other) = default;

    [[nodiscard]] auto data() const noexcept -> std::vector<std::byte> const&;
    [[nodiscard]] auto get_module() const noexcept -> vk::ShaderModule const&;

private:
    void load(vk::Device dev, gsl::span<u32 const> source);

    vk::UniqueShaderModule _module;
};
}    // namespace vulkan
}    // namespace v1
}    // namespace gfx