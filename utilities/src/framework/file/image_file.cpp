#include "../file.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#define NANOSVG_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include <nanosvg.h>
#include <nanosvgrast.h>
#include <algorithm>

namespace gfx
{
    image_info image_file::info(const files::path& path) noexcept
    {
        int w, h, c; stbi_info(gfx::file("indoor/posx.hdr").path.string().c_str(), &w, &h, &c);
        image_info info;
        info.width      = uint32_t(w);
        info.height     = uint32_t(h);
        info.channels   = uint16_t(c);
        return info;
    }

    image_file::image_file(const files::path& path, bits channel_bits, uint16_t channels)
        : file(path), channel_bits(channel_bits)
    {
        assert(std::clamp<uint16_t>(channels, 1, 4) == channels && "Invalid channel count.");
        struct deleter { void operator()(void* d) { free(d); } };

        int w, h;
        switch (channel_bits)
        {
        case bits::b8:
        {
            std::unique_ptr<void, deleter> data (stbi_load(file::path.string().c_str(), &w, &h, nullptr, channels));
            uint8_t* ptr = static_cast<uint8_t*>(data.get());
            this->channels = channels;
            this->data = std::vector<uint8_t>(ptr, ptr + w*h*channels);
            _raw = std::get<std::vector<uint8_t>>(this->data).data();
        }
        break;
        case bits::b16:
        {
            std::unique_ptr<void, deleter> data (stbi_load_16(file::path.string().c_str(), &w, &h, nullptr, channels));
            uint16_t* ptr = static_cast<uint16_t*>(data.get());
            this->channels = channels;
            this->data = std::vector<uint16_t>(ptr, ptr + w*h*channels);
            _raw = std::get<std::vector<uint16_t>>(this->data).data();
        }
        break;
        case bits::b32:
        {
            std::unique_ptr<void, deleter> data (stbi_loadf(file::path.string().c_str(), &w, &h, nullptr, channels));
            float* ptr = static_cast<float*>(data.get());
            this->channels = channels;
            this->data = std::vector<float>(ptr, ptr + w*h*channels);
            _raw = std::get<std::vector<float>>(this->data).data();
        }
        break;
        }
        width = uint32_t(w);
        height = uint32_t(h);
    }

    image_file::image_file(const files::path& svg, float raster_scale)
        : file(svg)
    {
        assert(svg.extension() == ".svg");

        struct rasterizer_deleter { void operator()(NSVGrasterizer* r) const {
                nsvgDeleteRasterizer(r);
            }
        };

        static std::unique_ptr<NSVGrasterizer, rasterizer_deleter> rasterizer{ nsvgCreateRasterizer() };

        NSVGimage* parsed = nsvgParseFromFile(file::path.string().c_str(), "px", 96);

        auto&& vec = std::get<std::vector<uint8_t>>(data = std::vector<uint8_t>(raster_scale*raster_scale*parsed->width*parsed->height * 4));
        channels = 4;
        channel_bits = bits::b8;
        width = static_cast<int>(raster_scale*parsed->width);
        height = static_cast<int>(raster_scale*parsed->height);
        nsvgRasterize(rasterizer.get(), parsed, 0, 0, raster_scale, static_cast<uint8_t*>(vec.data()),
            static_cast<int>(raster_scale*parsed->width), static_cast<int>(raster_scale*parsed->height), static_cast<int>(raster_scale*parsed->width) * 4);
        nsvgDelete(parsed);

        _raw = vec.data();
    }

    int image_file::pixel_count() const noexcept
    {
        return width * height;
    }

    void* image_file::bytes() const noexcept
    {
        return _raw;
    }
}