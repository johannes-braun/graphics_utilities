#pragma once

#include <array>
#include <mygl/gl.hpp>
#include <jpu/memory.hpp>
#include <jpu/flags.hpp>
#include <jpu/log.hpp>

namespace gl
{
    enum class buffer_flag_bits : uint32_t
    {
        dynamic_storage = GL_DYNAMIC_STORAGE_BIT,
        map_read = GL_MAP_READ_BIT,
        map_write = GL_MAP_WRITE_BIT,
        map_persistent = GL_MAP_PERSISTENT_BIT,
        map_coherent = GL_MAP_COHERENT_BIT,
        client_storage = GL_CLIENT_STORAGE_BIT,
        map_dynamic_persistent = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_DYNAMIC_STORAGE_BIT,
        map_dynamic_persistent_read = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_DYNAMIC_STORAGE_BIT,
        map_dynamic_persistent_write = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_DYNAMIC_STORAGE_BIT,
    };
    using buffer_flags = jpu::flags<uint32_t, buffer_flag_bits>;

    class buffer : public jpu::ref_count
    {
    public:
        template<typename... T>
        buffer(const std::vector<T...>& data, buffer_flags flags = {}) noexcept;

        template<typename T, size_t S>
        buffer(const std::array<T, S>& data, buffer_flags flags = {}) noexcept;

        template<typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>>>
        buffer(const T& object, buffer_flags flags = {}) noexcept : buffer(&object, 1, flags) {}

        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
        explicit buffer(T size, buffer_flags flags = {}) noexcept : buffer(static_cast<uint8_t*>(nullptr), size, flags) {};

        template<typename TValue>
        explicit buffer(TValue* data, size_t count, buffer_flags flags = {}) noexcept;


        buffer(buffer&& other) noexcept = default;
        buffer& operator=(buffer&& other) noexcept = default;
        ~buffer() noexcept;
        operator gl_buffer_t() const noexcept;

        template<typename TContainer, typename = decltype(std::data(std::declval<TContainer>()))>
        void assign(TContainer data, size_t offset_bytes = 0) const noexcept;
        template<typename TValue>
        void assign(TValue* data, size_t count, size_t offset_bytes = 0) const noexcept;

        void clear_to_float(float value) const noexcept;

        template<typename T>
        T& at(size_t position) const;
        
        template<typename T>
        T* data_as() const noexcept;

        void map(GLenum map_access) noexcept;
        void unmap() const noexcept;
        void flush(size_t size_bytes, size_t offset_bytes) const noexcept;
        void bind(GLenum type, uint32_t binding_point) const noexcept;
        void bind(GLenum type, uint32_t binding_point, size_t size, ptrdiff_t offset = 0) const noexcept;

        size_t size() const noexcept;
        uint64_t address() const noexcept;

    private:
        void allocate(size_t size, const void* data, buffer_flags flags) const noexcept;
        void map_if_needed() noexcept;
        void make_persistent_address() noexcept;

        gl_buffer_t _id;
        size_t _size;
        void* _mapped_data = nullptr;
        buffer_flags _flags;
        GLenum _map_access{ GL_ZERO };
        uint64_t _persistent_address;
    };
}

#include "buffer.inl"