#pragma once

#include <jpu/memory.hpp>
#include "pipeline.hpp"

namespace gl
{
    class state : public jpu::ref_count
    {
    public:
        state() noexcept;
        ~state() noexcept;

        void capture(basic_primitive primitive) const noexcept;

        operator gl_state_nv_t() const noexcept { return _id; }

    private:
        gl_state_nv_t _id;
    };
}
