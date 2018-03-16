#include "../texture.hpp"
#include "../device.hpp"
#include <any>

namespace vkn
{
    namespace command
    {
        void transform_image_layout(const vk::CommandBuffer buffer,
            const vk::Image image, vk::ImageSubresourceRange range,
            const vk::AccessFlags source_access, const vk::AccessFlags destination_access,
            const vk::ImageLayout source_layout, const vk::ImageLayout destination_layout,
            const vk::PipelineStageFlags source_stage, const vk::PipelineStageFlags destination_stage)
        {
            buffer.pipelineBarrier(source_stage, destination_stage, {}, {}, {}, vk::ImageMemoryBarrier(
                source_access, destination_access, source_layout, destination_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, range
            ));
        }
    }

    texture::texture(device* device)
        : _device(device)
    {
        _device->inc_ref();
    }

    texture::~texture()
    {
        if (_memory) _device->memory()->free(_memory);
        if (_image) _device->destroyImage(_image);
        _device->dec_ref();
    }

    void texture::assign_2d(const uint32_t width, const uint32_t height, const vk::Format format, const size_t data_size, void* data, const vk::QueueFlagBits transfer_queue_type)
    {
        auto&& transfer_queue = _device->queue(transfer_queue_type).queue;
        auto&& transfer_pool = _device->command_pool(transfer_queue_type);

        _extent = vk::Extent3D(width, height, 1);
        _mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))) + 1);

        _format = format;
        const vk::DeviceSize texture_data_size = data_size;

        const auto staging_buffer = _device->createBuffer(vk::BufferCreateInfo({}, texture_data_size, vk::BufferUsageFlagBits::eTransferSrc));
        const auto staging_buffer_req = _device->getBufferMemoryRequirements(staging_buffer);
        const auto staging_memory = _device->memory()->allocate(staging_buffer_req.size, staging_buffer_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible);
        _device->bindBufferMemory(staging_buffer, staging_memory->memory, staging_memory->offset);

        const vk::MappedMemoryRange memory_range(staging_memory->memory, staging_memory->offset, staging_memory->size);
        memcpy(staging_memory->data, data, texture_data_size);
        _device->flushMappedMemoryRanges(memory_range);

        vk::ImageCreateInfo image_info;
        image_info.setArrayLayers(1)
            .setImageType(vk::ImageType::e2D)
            .setFormat(_format)
            .setMipLevels(_mip_levels)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setExtent(_extent);
        _image = _device->createImage(image_info);
        const auto texture_requirements = _device->getImageMemoryRequirements(_image);
        _memory = _device->memory()->allocate(texture_requirements.size, texture_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        _device->bindImageMemory(_image, _memory->memory, _memory->offset);

        auto transfer_buffer = _device->allocateCommandBuffers(vk::CommandBufferAllocateInfo(transfer_pool, vk::CommandBufferLevel::ePrimary, 1))[0];
        transfer_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        {
            const vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
            command::transform_image_layout(transfer_buffer, _image, range,
                vk::AccessFlags(), vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            vk::BufferImageCopy copy;
            copy.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setImageExtent(_extent);
            transfer_buffer.copyBufferToImage(staging_buffer, _image, vk::ImageLayout::eTransferDstOptimal, copy);

            command::transform_image_layout(transfer_buffer, _image, range,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics);
        }
        transfer_buffer.end();

        vk::SubmitInfo texture_copy_submit;
        texture_copy_submit.setCommandBufferCount(1)
            .setPCommandBuffers(&transfer_buffer);
        transfer_queue.submit(texture_copy_submit, nullptr);
        transfer_queue.waitIdle();

        _device->freeCommandBuffers(transfer_pool, transfer_buffer);
        _device->memory()->free(staging_memory);
        _device->destroyBuffer(staging_buffer);
    }

    void texture::create_empty(const vk::ImageCreateInfo info)
    {
        if (_image) _device->destroyImage(_image);
        if (_memory) _device->memory()->free(_memory);

        _extent = info.extent;
        _format = info.format;
        _mip_levels = info.mipLevels;

        _image = _device->createImage(info);
        const auto texture_requirements = _device->getImageMemoryRequirements(_image);
        _memory = _device->memory()->allocate(texture_requirements.size, texture_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
        _device->bindImageMemory(_image, _memory->memory, _memory->offset);
    }

    void texture::generate_mipmaps() const
    {
        _device->unique_command(vk::QueueFlagBits::eTransfer, [&](vk::CommandBuffer command_buffer) {
            generate_mipmaps(command_buffer);
        });
    }

    void texture::generate_mipmaps(const vk::CommandBuffer command_buffer) const
    {
        const vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        command::transform_image_layout(command_buffer, _image, range,
            vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eTransfer);

        for (uint32_t level = 1; level < _mip_levels; ++level)
        {
            const vk::ImageSubresourceRange mip_range(vk::ImageAspectFlagBits::eColor, level, 1, 0, 1);

            vk::ImageBlit image_blit;
            image_blit.setSrcOffsets({ vk::Offset3D{}, vk::Offset3D{ static_cast<int32_t>(_extent.width >> (level - 1)), static_cast<int32_t>(_extent.height >> (level - 1)), 1 } })
                .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, level - 1, 0, 1))
                .setDstOffsets({ vk::Offset3D{}, vk::Offset3D{ static_cast<int32_t>(_extent.width >> level), static_cast<int32_t>(_extent.height >> level), 1 } })
                .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, level, 0, 1));

            command::transform_image_layout(command_buffer, _image, mip_range,
                vk::AccessFlags(), vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);

            command_buffer.blitImage(_image, vk::ImageLayout::eTransferSrcOptimal, _image, vk::ImageLayout::eTransferDstOptimal, { image_blit }, vk::Filter::eLinear);

            command::transform_image_layout(command_buffer, _image, mip_range,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer);
        }

        command::transform_image_layout(command_buffer, _image,
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, _mip_levels, 0, 1),
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics);
    }

    vk::ImageView texture::create_image_view(vk::ImageSubresourceRange resource_range) const
    {
        if (resource_range.levelCount == 0)
            resource_range.setLevelCount(_mip_levels);
        return _device->createImageView(vk::ImageViewCreateInfo({}, _image, vk::ImageViewType::e2D, _format, {}, resource_range));
    }

    vk::Format texture::format() const
    {
        return _format;
    }

    const vk::Extent3D& texture::extent() const
    {
        return _extent;
    }

    uint32_t texture::levels() const
    {
        return _mip_levels;
    }

    vk::Image texture::image() const
    {
        return _image;
    }

    texture_view::texture_view(texture* texture, const vk::ImageSubresourceRange resource_range)
        : _texture(texture), _resource_range(resource_range), _image_view(_texture->create_image_view(_resource_range))
    {
        _texture->inc_ref();
    }

    texture_view::texture_view(device* device, const vk::ImageViewCreateInfo create_info)
        : _texture(new texture(device)), _resource_range(create_info.subresourceRange),
        _image_view(_texture->_device->createImageView(create_info))
    {
        _texture->inc_ref();
    }

    texture_view::~texture_view()
    {
        _texture->_device->destroyImageView(_image_view);
        _texture->dec_ref();
    }

    texture_view::operator vk::ImageView() const
    {
        return _image_view;
    }
}