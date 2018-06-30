#pragma once

#include "../device_image.hpp"
#include <mygl/mygl.hpp>

namespace gfx::opengl
{
class device_image_implementation : public detail::device_image_implementation
{
public:
    void initialize(uint32_t layer_dimensions, img_format format, const extent& size, uint32_t levels) override;
    void fill_from(const host_image& image, uint32_t level, uint32_t layer) override;

private:
    mygl::texture _handle;
    GLenum        _internal_format;
    GLenum        _external_format;
    GLenum        _external_type;
    extent        _extent;
    GLenum        _type;
    uint32_t      _levels;
};
} // namespace gfx::opengl