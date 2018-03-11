#include "buffer.hpp"
#pragma once

namespace gl::v2
{
    template<typename T>
    buffer<T>::buffer(GLbitfield usage) noexcept
        : _usage(usage)
    {
        glCreateBuffers(1, &_id);
    }

    template<typename T>
    buffer<T>::~buffer() noexcept
    {
        glDeleteBuffers(1, &_id);
    }

    template<typename T>
    buffer<T>::buffer(size_t count, GLbitfield usage)
        : buffer(count, T(), usage)
    {
    }

    template<typename T>
    buffer<T>::buffer(size_t count, const T& init, GLbitfield usage)
        : buffer(usage)
    {
        resize(count, init, usage);
    }

    template<typename T>
    buffer<T>::buffer(std::initializer_list<T> list, GLbitfield usage)
        : buffer(list.begin(), list.end(), usage)
    {
    }

    template<typename T>
    template<typename It, typename, typename, typename, typename, typename, typename>
    buffer<T>::buffer(It begin, It end, GLbitfield usage)
        : buffer(usage)
    {
        if constexpr(iterator_has_difference_v<It>)
        {
            _size = end - begin;
            if (_size > 0)
            {
                const T* data = &*begin;
                glNamedBufferStorage(_id, _size * sizeof(T), data, _usage);
            }
        }
        else
        {
            std::vector<T> temp(begin, end);
            _size = temp.size();
            if (_size > 0)
            {
                glNamedBufferStorage(_id, _size * sizeof(T), temp.data(), _usage);
            }
        }
        _reserved_size = _size;
    }

    template<typename T>
    void buffer<T>::push_back(T && value)
    {
        const size_t last_size = _size;
        if (_size + 1 >= _reserved_size)
        {
            reserve(static_cast<size_t>(std::max(1.0, static_cast<double>(_reserved_size)) * compute_size_increase()));
        }
        glNamedBufferSubData(_id, last_size * sizeof(T), sizeof(T), &value);
        ++_size;
        glFinish();
    }

    template<typename T>
    void buffer<T>::push_back(const T& value)
    {
        const size_t last_size = _size;
        const bool was_data_full_size = _data_full_size;
        const GLbitfield map_access = _data_access;
        if (was_data_full_size && _size != 0)
            unmap();
        if (_size + 1 >= _reserved_size)
        {
            reserve(static_cast<size_t>(std::max(1.0, static_cast<double>(_reserved_size)) * compute_size_increase()));
        }
        glNamedBufferSubData(_id, last_size * sizeof(T), sizeof(T), &value);
        ++_size;
        if(was_data_full_size)
            map(map_access);
        glFinish();
    }

    template<typename T>
    template<typename ...As>
    void buffer<T>::emplace_back(As && ...value)
    {
        push_back(std::move(T(std::forward<As&&>(value)...)));
    }

    template<typename T>
    template<typename It, typename, typename, typename, typename, typename, typename>
    inline typename buffer<T>::iterator buffer<T>::insert(iterator at, It begin, It end)
    {
        iterator retval;
        const bool was_data_full_size = _data_full_size;
        const GLbitfield map_access = _data_access;
        auto end_it = this->end();
        if (was_data_full_size && _size != 0)
            unmap();
        if constexpr(iterator_has_difference_v<It>)
        {
            const ptrdiff_t size = end - begin;
            if (_size > 0)
            {
                const size_t new_size = _size + size;
                if (new_size > _reserved_size)
                {
                    reserve(static_cast<size_t>(std::max(1.0, static_cast<double>(_reserved_size)) * compute_size_increase()));
                }
                _size = new_size;
                const T* data = &*begin;
                const size_t insert_position = at._offset;
                end_it._size += size;
                at._size += size;
                retval = std::rotate(at, at + size-1, end_it);
                glNamedBufferSubData(_id, insert_position * sizeof(T), size * sizeof(T), data);
            }
        }
        else
        {
            std::vector<T> temp(begin, end);
            const ptrdiff_t size = temp.size();

            if (_size > 0)
            {
                const size_t new_size = _size + size;
                if (new_size > _reserved_size)
                {
                    reserve(static_cast<size_t>(std::max(1.0, static_cast<double>(_reserved_size)) * compute_size_increase()));
                }
                _size = new_size;
                const T* data = &*begin;
                const size_t insert_position = at._offset;
                end_it._size += size;
                at._size += size;
                retval = std::rotate(at, at + size, this->end());
                glNamedBufferSubData(_id, insert_position * sizeof(T), size * sizeof(T), temp.data());
            }
        }
        if (was_data_full_size)
            map(map_access);

        return retval;
    }

    template<typename T>
    inline typename buffer<T>::iterator buffer<T>::erase(iterator it)
    {
        const ptrdiff_t diff = it - begin();
        --_size;
        if (diff < ptrdiff_t(_data_offset))
            _data_offset--;
        else if (diff < ptrdiff_t(_data_offset + _data_size))
            --_data_size;
        iterator i; i._parent = this; i._size = _size; i._offset = diff;
        return i;
    }

    template<typename T>
    double buffer<T>::compute_size_increase() const noexcept
    {
        return 1.0 + (10.0 / std::max(1.0, std::pow(_size, 0.35)));
    }

    template<typename T>
    void buffer<T>::shrink_to_fit()
    {
        const bool was_data_full_size = _data_full_size;
        const GLbitfield map_access = _data_access;
        if (was_data_full_size && _size != 0)
            unmap();
        resize(_size, _usage);
        if (was_data_full_size)
            map(map_access);
    }

    template<typename T>
    inline size_t buffer<T>::size() const noexcept
    {
        return _size;
    }

    template<typename T>
    inline size_t buffer<T>::capacity() const noexcept
    {
        return _reserved_size;
    }

    template<typename T>
    void buffer<T>::resize(size_t size, GLbitfield usage)
    {
        resize(size, T(), usage);
    }

    template<typename T>
    void buffer<T>::resize(size_t size, const T & init, GLbitfield usage)
    {
        if (usage != keep_usage) _usage = usage;
        reserve(size);
        if (_size < size)
        {
            const ptrdiff_t diff = size - _size;
            std::vector<T> additional_data(diff, init);
            glNamedBufferSubData(_id, _size * sizeof(T), diff * sizeof(T), additional_data.data());
        }
        _size = size;
    }

    template<typename T>
    void buffer<T>::reserve(size_t size)
    {
        if ((_usage & GL_DYNAMIC_STORAGE_BIT) != GL_DYNAMIC_STORAGE_BIT)
            throw std::invalid_argument("Cannot reserve or resize a buffer without GL_DYNAMIC_STORAGE_BIT set.");
        if (size == _reserved_size)
            return;
        const bool was_mapped = is_mapped();
        size_t map_size = _data_size;
        const ptrdiff_t map_offset = _data_offset;
        const GLbitfield map_access = _data_access;
        bool data_full_size = _data_full_size;
        if (was_mapped && _data_size != 0)
            unmap();

        std::unique_ptr<T[]> data = std::make_unique<T[]>(_size);
        glGetNamedBufferSubData(_id, 0, _size * sizeof(T), data.get());
        glFinish();
        glDeleteBuffers(1, &_id);
        glCreateBuffers(1, &_id);
        glNamedBufferStorage(_id, size * sizeof(T), nullptr, _usage);
        glNamedBufferSubData(_id, 0, std::min(_size, size) * sizeof(T), data.get());
        glFinish();
        if (was_mapped)
        {
            const ptrdiff_t max_size = static_cast<ptrdiff_t>(size) - map_offset;
            if (_size > size)
            {
                map_size = static_cast<size_t>(std::max(std::min(static_cast<ptrdiff_t>(map_size), max_size), 0i64));
            }
            if (!data_full_size && map_size > 0)
                map(map_offset, map_size, map_access);
        }
        _reserved_size = size;
        _size = std::min(_size, size);

        if (!glIsNamedBufferResidentNV(_id))
            glMakeNamedBufferResidentNV(_id, GL_READ_WRITE);
        glGetNamedBufferParameterui64vNV(_id, GL_BUFFER_GPU_ADDRESS_NV, &_handle);
    }

    template<typename T>
    void buffer<T>::map(GLbitfield access)
    {
        _data_full_size = true;
        map(_size, access);
    }

    template<typename T>
    void buffer<T>::map(size_t count, GLbitfield access)
    {
        map(0, count, access);
    }

    template<typename T>
    bool buffer<T>::is_mapped() const noexcept
    {
        return _data_full_size || _data != nullptr;
    }

    template<typename T>
    void buffer<T>::map(size_t begin, size_t count, GLbitfield access)
    {
        const bool full = _data_full_size;
        if (!(_data_full_size && _size == 0))
        {
            if (begin + count > _size)
                throw std::out_of_range("Range out of range.");
            if (!is_mapped()) unmap();
        }
        _data_full_size = full;
        _data_size = count;
        _data_offset = begin;
        _data_access = access;
        _cached_position = 0;
        _cached = nullptr;
        if (count != 0)
            _data = static_cast<T*>(glMapNamedBufferRange(_id, begin * sizeof(T), count * sizeof(T), access));
        else
            _data = nullptr;
    }

    template<typename T>
    void buffer<T>::flush_element(size_t index)
    {
        flush_elements(index, 1);
    }

    template<typename T>
    void buffer<T>::flush_elements(size_t begin, size_t count)
    {
        flush_bytes(begin * sizeof(T), count * sizeof(T));
    }

    template<typename T>
    void buffer<T>::flush_bytes(size_t offset, size_t size)
    {
        if (offset + size >= _data_size * sizeof(T)) throw std::out_of_range("Range out of bounds.");
        glFlushMappedNamedBufferRange(_id, offset, size);
    }

    template<typename T>
    void buffer<T>::unmap() noexcept
    {
        synchronize();
        _data_full_size = false;
        _data = nullptr;
        _data_size = 0;
        _data_offset = 0;
        _data_access = GL_ZERO;
        glUnmapNamedBuffer(_id);
    }

    template<typename T>
    void buffer<T>::synchronize() const noexcept
    {
        if((_usage & GL_DYNAMIC_STORAGE_BIT) == GL_DYNAMIC_STORAGE_BIT && _cached)
            glNamedBufferSubData(_id, _cached_position * sizeof(T), sizeof(T), _cached.get());
    }

    template<typename T>
    inline uint64_t buffer<T>::handle() const noexcept
    {
        return _handle;
    }

    template<typename T>
    T & buffer<T>::at(size_t index)
    {
        if ((_data && index >= _data_offset + _data_size) || index >= _size)
            throw std::out_of_range("Index out of bounds.");
        if (!_data)
        {
            synchronize();
            if (!_cached)
                _cached = std::make_unique<T>();
            glGetNamedBufferSubData(_id, index * sizeof(T), sizeof(T), _cached.get());
            _cached_position = index;
            return *_cached;
        }
        return _data[index];
    }

    template<typename T>
    const T & buffer<T>::at(size_t index) const
    {
        if ((_data && index >= _data_offset + _data_size) || index >= _size)
            throw std::out_of_range("Index out of bounds.");
        if (!_data)
        {
            synchronize();
            if (!_cached)
                _cached = std::make_unique<T>();
            glGetNamedBufferSubData(_id, index * sizeof(T), sizeof(T), _cached.get());
            _cached_position = index;
            return *_cached;
        }
        return _data[index];
    }

    template<typename T>
    T & buffer<T>::operator[](size_t index)
    {
        if ((_data && index >= _data_offset + _data_size) || index >= _size)
            throw std::out_of_range("Index out of bounds.");
        if (!_data)
        {
            synchronize();
            if (!_cached)
                _cached = std::make_unique<T>();
            glGetNamedBufferSubData(_id, index * sizeof(T), sizeof(T), _cached.get());
            _cached_position = index;
            return *_cached;
        }
        return _data[index];
    }

    template<typename T>
    const T & buffer<T>::operator[](size_t index) const
    {
        if ((_data && index >= _data_offset + _data_size) || index >= _size)
            throw std::out_of_range("Index out of bounds.");
        if (!_data)
        {
            synchronize();
            if (!_cached)
                _cached = std::make_unique<T>();
            glGetNamedBufferSubData(_id, index * sizeof(T), sizeof(T), _cached.get());
            _cached_position = index;
            return *_cached;
        }
        return _data[index];
    }

    template<typename T>
    T* buffer<T>::data() noexcept
    {
        return _data;
    }

    template<typename T>
    const T* buffer<T>::data() const noexcept
    {
        return _data;
    }

    template<typename T>
    buffer<T>::operator gl_buffer_t() const noexcept
    {
        return _id;
    }
}