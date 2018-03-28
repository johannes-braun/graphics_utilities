#pragma once

#include "enums.hpp"

#include <cinttypes>
#include <vector>

namespace gl
{
    enum class native_handle : uint64_t { null = 0 };

    class context
    {
    public: 
        template<typename T, typename = std::enable_if_t<sizeof(T) == sizeof(void*)>>
        context(T hnd, std::vector<std::pair<context_attribs, int>> context_attributes)
            : context((check_handle(reinterpret_cast<native_handle&>(hnd)), reinterpret_cast<native_handle&>(hnd)), std::move(context_attributes))
        {};
        context(native_handle window, std::vector<std::pair<context_attribs, int>> context_attributes);

        ~context();
        void set_pixel_format(std::vector<std::pair<pixel_format_attribs, int>> attributes);

        void make_current() const noexcept;
        void swap_buffers() const noexcept;
        void set_swap_interval(int i) const noexcept;

    private:
        void check_handle(native_handle hnd) const;

        int(*swapIntervalEXT)(int) = nullptr;
        native_handle _device_context;
        native_handle _context;
    };
}