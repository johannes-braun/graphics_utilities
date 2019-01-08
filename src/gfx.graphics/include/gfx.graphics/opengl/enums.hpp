#pragma once

namespace gfx {
inline namespace v1 {
namespace opengl {
enum context_profile_mask
{
    GL_CONTEXT_CORE_PROFILE_BIT_ARB          = 0x00000001,
    GL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002
};

enum context_release_behavior
{
    GL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB  = 0,
    GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB = 0x2098
};

enum context_flags
{
    GL_CONTEXT_DEBUG_BIT_ARB              = 0x00000001,
    GL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x00000002,
    GL_CONTEXT_ROBUST_ACCESS_BIT_ARB      = 0x00000004,
    GL_CONTEXT_RESET_ISOLATION_BIT_ARB    = 0x00000008,
};

enum context_attribs
{
    GL_CONTEXT_MAJOR_VERSION_ARB               = 0x2091,
    GL_CONTEXT_MINOR_VERSION_ARB               = 0x2092,
    GL_CONTEXT_FLAGS_ARB                       = 0x2094,
    GL_CONTEXT_OPENGL_NO_ERROR_ARB             = 0x31B3,
    GL_CONTEXT_PROFILE_MASK_ARB                = 0x9126,
    GL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB = 0x8256,
    GL_CONTEXT_RELEASE_BEHAVIOR_ARB            = 0x2097,
};

enum pixel_format_attribs
{
    GL_NUMBER_PIXEL_FORMATS_ARB    = 0x2000,
    GL_DRAW_TO_WINDOW_ARB          = 0x2001,
    GL_DRAW_TO_BITMAP_ARB          = 0x2002,
    GL_ACCELERATION_ARB            = 0x2003,
    GL_NEED_PALETTE_ARB            = 0x2004,
    GL_NEED_SYSTEM_PALETTE_ARB     = 0x2005,
    GL_SWAP_LAYER_BUFFERS_ARB      = 0x2006,
    GL_SWAP_METHOD_ARB             = 0x2007,
    GL_NUMBER_OVERLAYS_ARB         = 0x2008,
    GL_NUMBER_UNDERLAYS_ARB        = 0x2009,
    GL_TRANSPARENT_ARB             = 0x200A,
    GL_TRANSPARENT_RED_VALUE_ARB   = 0x2037,
    GL_TRANSPARENT_GREEN_VALUE_ARB = 0x2038,
    GL_TRANSPARENT_BLUE_VALUE_ARB  = 0x2039,
    GL_TRANSPARENT_ALPHA_VALUE_ARB = 0x203A,
    GL_TRANSPARENT_INDEX_VALUE_ARB = 0x203B,
    GL_SHARE_DEPTH_ARB             = 0x200C,
    GL_SHARE_STENCIL_ARB           = 0x200D,
    GL_SHARE_ACCUM_ARB             = 0x200E,
    GL_SUPPORT_GDI_ARB             = 0x200F,
    GL_SUPPORT_OPENGL_ARB          = 0x2010,
    GL_DOUBLE_BUFFER_ARB           = 0x2011,
    GL_STEREO_ARB                  = 0x2012,
    GL_PIXEL_TYPE_ARB              = 0x2013,
    GL_COLOR_BITS_ARB              = 0x2014,
    GL_RED_BITS_ARB                = 0x2015,
    GL_RED_SHIFT_ARB               = 0x2016,
    GL_GREEN_BITS_ARB              = 0x2017,
    GL_GREEN_SHIFT_ARB             = 0x2018,
    GL_BLUE_BITS_ARB               = 0x2019,
    GL_BLUE_SHIFT_ARB              = 0x201A,
    GL_ALPHA_BITS_ARB              = 0x201B,
    GL_ALPHA_SHIFT_ARB             = 0x201C,
    GL_ACCUM_BITS_ARB              = 0x201D,
    GL_ACCUM_RED_BITS_ARB          = 0x201E,
    GL_ACCUM_GREEN_BITS_ARB        = 0x201F,
    GL_ACCUM_BLUE_BITS_ARB         = 0x2020,
    GL_ACCUM_ALPHA_BITS_ARB        = 0x2021,
    GL_DEPTH_BITS_ARB              = 0x2022,
    GL_STENCIL_BITS_ARB            = 0x2023,
    GL_AUX_BUFFERS_ARB             = 0x2024,
    GL_SAMPLE_BUFFERS_ARB          = 0x2041,
    GL_SAMPLES_ARB                 = 0x2042,
};

enum acceleration
{
    GL_NO_ACCELERATION_ARB      = 0x2025,
    GL_GENERIC_ACCELERATION_ARB = 0x2026,
    GL_FULL_ACCELERATION_ARB    = 0x2027
};

enum swap_method
{
    GL_SWAP_EXCHANGE_ARB  = 0x2028,
    GL_SWAP_COPY_ARB      = 0x2029,
    GL_SWAP_UNDEFINED_ARB = 0x202A
};

enum pixel_type
{
    GL_TYPE_RGBA_ARB       = 0x202B,
    GL_TYPE_COLORINDEX_ARB = 0x202C
};
}    // namespace opengl
}    // namespace v1
}    // namespace gfx