#include "vertex_input.hpp"
#include "opengl/vertex_input_opengl.hpp"
#include <gfx/context.hpp>

namespace gfx
{
std::unique_ptr<detail::vertex_input_implementation> detail::make_vertex_input_implementation()
{
    switch (context::current()->options().graphics_api)
    {
    case gapi::opengl: return std::make_unique<opengl::vertex_input_implementation>();
    case gapi::vulkan: break;
    default:;
    }
    return nullptr;
}

vertex_input::vertex_input() : _implementation(detail::make_vertex_input_implementation()) {}

uint32_t vertex_input::add_attribute(uint32_t binding, gfx::format fmt, size_t offset)
{
    return _implementation->add_attribute(binding, fmt, offset);
}

void vertex_input::set_binding_info(uint32_t binding, size_t stride, input_rate rate)
{
    _implementation->set_binding_info(binding, stride, rate);
}

void vertex_input::set_assembly(topology mode, bool enable_primitive_restart)
{
    _topology = mode;
    _implementation->set_assembly(mode, enable_primitive_restart);
}

void vertex_input::draw(uint32_t vertices, uint32_t instances, uint32_t base_vertex, uint32_t base_instance) const
{
    _implementation->draw(vertices, instances, base_vertex, base_instance);
}

void vertex_input::draw_indexed(uint32_t indices, uint32_t instances, uint32_t base_index, int32_t base_vertex,
                                uint32_t base_instance) const
{
    _implementation->draw_indexed(indices, instances, base_index, base_vertex, base_instance);
}
}    // namespace gfx
