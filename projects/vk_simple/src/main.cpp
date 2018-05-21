#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>

#include <vector>
#include <array>
#include <functional>
#include <gfx/log.hpp>
#include <gfx/window.hpp>

#include "gfx/vk/instance.hpp"
#include "gfx/vk/debug_callback.hpp"
#include "gfx/vk/physical_device.hpp"
#include "gfx/vk/surface.hpp"
#include "gfx/vk/swapchain.hpp"
#include "gfx/vk/semaphore.hpp"
#include "gfx/vk/fence.hpp"
#include "gfx/vk/commands.hpp"
#include "gfx/vk/image.hpp"
#include "gfx/vk/renderpass.hpp"
#include "gfx/vk/framebuffer.hpp"
#include "gfx/vk/memory.hpp"
#include "gfx/vk/buffer.hpp"
#include <mutex>

constexpr gfx::vk::app_info default_app_info                { "My App", { 1, 0 }, "My Engine", { 1, 0 } };
constexpr std::array<const char*, 3> layers                 { "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_parameter_validation" };
constexpr std::array<const char*, 2> instance_extensions    { "VK_KHR_surface", "VK_EXT_debug_report" };
constexpr std::array<const char*, 2> device_extensions      { "VK_KHR_swapchain", "VK_KHR_push_descriptor" };

constexpr uint32_t queue_index_graphics      = 0;
constexpr uint32_t queue_index_compute       = 1;
constexpr uint32_t queue_index_transfer      = 2;
constexpr uint32_t queue_index_present       = 3;
constexpr std::array<float, 4> queue_priorities = { 0.9f, 1.f, 0.5f, 0.9f };

struct worker {
    worker() = default;
    
    template<typename Fun>
    worker(Fun fun) : _thread(fun) {
        _thread.detach();
    }

    worker(worker&& other) noexcept
    {
        _thread = std::move(other._thread);
        _commands = std::move(other._commands);
    }
    worker& operator=(worker&& other) noexcept
    {
        _thread = std::move(other._thread);
        _commands = std::move(other._commands);
        return *this;
    }

    void consume() {
        std::unique_lock<std::mutex> lock(_command_mutex);
        while (!_commands.empty()) {
            _commands.front()();
            _commands.pop();
        }
    }

    void enqueue(const std::function<void()> func)
    {
        std::unique_lock<std::mutex> lock(_command_mutex);
        _commands.push(func);
    }

private:
    std::thread _thread;
    std::queue<std::function<void()>> _commands;
    std::mutex _command_mutex;
};

std::atomic_bool close_window = false;
std::atomic_bool terminate_threads = false;
worker main_thread_worker;

std::unique_ptr<gfx::window> window;

int main()
{
    std::atomic_bool window_created = false;
    main_thread_worker = worker([&]() {
        window = std::make_unique<gfx::window>(gfx::apis::vulkan::name, "Vulkan V2", 1280, 720);
        window->char_callback.add([](GLFWwindow* w, uint32_t c) {
            log_i << "Polled";
            if (c == 'x')
                glfwSetWindowShouldClose(w, true);
        });
        window_created = true;

        while (!terminate_threads)
        {
            auto now = std::chrono::steady_clock::now();
            main_thread_worker.consume();
            close_window = !window->update();
            while (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - now).count() < (1/240.f));
        }
    });

    while (!window_created) void(0);

    uint32_t count = 0; const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> final_instance_extensions(extensions, extensions+count);
    final_instance_extensions.insert(final_instance_extensions.end(), instance_extensions.begin(), instance_extensions.end());
    auto main_instance  = std::make_shared<gfx::vk::instance>(default_app_info, layers, final_instance_extensions);
    auto debug_callback = std::make_shared<gfx::vk::debug_callback>(main_instance, VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT,
        [](VkDebugReportFlagsEXT f, VkDebugReportObjectTypeEXT ot, uint64_t o, size_t l, int32_t m, const char* lp, const char* msg) {
        log_e << msg;
        return false;
    });

    auto main_gpu = [&]() -> std::shared_ptr<gfx::vk::physical_device> {
        for (const std::shared_ptr<gfx::vk::physical_device>& phys_device : main_instance->get_physical_devices())
            return phys_device;
        return nullptr;
    }();
    auto surface = std::make_shared<gfx::vk::surface>(main_gpu, *window);

    std::vector<VkQueueFamilyProperties> properties = main_gpu->get_queue_family_properties();
    const auto graphics_queue_iter = std::find_if(properties.begin(), properties.end(), [](const VkQueueFamilyProperties& p) { return (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0; });
    const auto compute_queue_iter  = std::find_if(properties.begin(), properties.end(), [](const VkQueueFamilyProperties& p) { return (p.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0; });
    const auto transfer_queue_iter = std::find_if(properties.begin(), properties.end(), [](const VkQueueFamilyProperties& p) { return (p.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0; });
    const auto present_queue_iter  = std::find_if(properties.begin(), properties.end(), [&](const VkQueueFamilyProperties& p) { 
       const uint32_t family = static_cast<uint32_t>(&p - &properties[0]);
        VkBool32 supported = true;
        vkGetPhysicalDeviceSurfaceSupportKHR(*main_gpu, family, *surface, &supported);
        return glfwGetPhysicalDevicePresentationSupport(*main_instance, *main_gpu, family) && supported;
    });

    struct queue_data { uint32_t family, index; };
    std::unordered_map<uint32_t, std::vector<float>> unique_families;
    std::vector<queue_data> queue_datas;
    
    unique_families[static_cast<uint32_t>(graphics_queue_iter - properties.begin())].push_back(queue_priorities[queue_index_graphics]);
    queue_datas.push_back({ uint32_t(graphics_queue_iter - properties.begin()), uint32_t(unique_families[static_cast<uint32_t>(graphics_queue_iter - properties.begin())].size() - 1) });
    unique_families[static_cast<uint32_t>(compute_queue_iter - properties.begin())].push_back(queue_priorities[queue_index_compute]);
    queue_datas.push_back({ uint32_t(compute_queue_iter - properties.begin()), uint32_t(unique_families[static_cast<uint32_t>(compute_queue_iter - properties.begin())].size() - 1) });
    unique_families[static_cast<uint32_t>(transfer_queue_iter - properties.begin())].push_back(queue_priorities[queue_index_transfer]);
    queue_datas.push_back({ uint32_t(transfer_queue_iter - properties.begin()), uint32_t(unique_families[static_cast<uint32_t>(transfer_queue_iter - properties.begin())].size() - 1) });
    unique_families[static_cast<uint32_t>(present_queue_iter - properties.begin())].push_back(queue_priorities[queue_index_present]);
    queue_datas.push_back({ uint32_t(present_queue_iter - properties.begin()), uint32_t(unique_families[static_cast<uint32_t>(present_queue_iter - properties.begin())].size() - 1) });

    std::vector<gfx::vk::queue_info> queue_infos;
    for (const auto& fam : unique_families) queue_infos.emplace_back(fam.first, fam.second);
    auto device = std::make_shared<gfx::vk::device>(main_gpu, layers, device_extensions, queue_infos, main_gpu->get_features());

    std::vector<gfx::vk::queue> queues;
    for (auto&& data : queue_datas) queues.emplace_back(device, data.family, data.index);
     
    auto capabilities           = surface->capabilities();
    auto formats                = surface->formats();
    auto present_modes          = surface->present_modes();
    const auto swapchain        = surface->create_default_swapchain(device, queues[queue_index_present].family(), 4);
    auto images                 = swapchain->images();
    auto command_pool           = std::make_shared<gfx::vk::command_pool>(device, queues[queue_index_graphics].family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    const auto command_buffer   = std::make_shared<gfx::vk::command_buffer>(command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto primary_buffers        = command_pool->allocate_buffers(uint32_t(images.size()), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto swap_semaphore   = std::make_shared<gfx::vk::semaphore>(device);
    const auto render_semaphore = std::make_shared<gfx::vk::semaphore>(device);

    std::vector<std::shared_ptr<gfx::vk::fence>> fences(uint32_t(images.size()));
    for(int i=0; i<images.size(); ++i) fences[i] = std::make_shared<gfx::vk::fence>(device, VK_FENCE_CREATE_SIGNALED_BIT);

    gfx::vk::subpass_info subpass(VK_PIPELINE_BIND_POINT_GRAPHICS, {}, gfx::vk::attachment_reference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    auto dependencies = gfx::vk::subpass_dependencies()
        .to(0, VK_DEPENDENCY_BY_REGION_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .finish(VK_DEPENDENCY_BY_REGION_BIT);
    auto color_attachment = gfx::vk::attachment_description({}, VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT)
        .on_load(VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_CLEAR)
        .on_store(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE);
    auto renderpass = std::make_shared<gfx::vk::renderpass>(device, color_attachment, subpass, dependencies);

    std::vector<std::shared_ptr<gfx::vk::image_view>> swapchain_views(images.size());
    std::vector<std::shared_ptr<gfx::vk::framebuffer>> swapchain_framebuffers(images.size());
    #pragma omp parallel for
    for (int i=0; i<images.size(); ++i)
    {
        swapchain_views[i] = images[i]->create_view(VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        swapchain_framebuffers[i] = std::make_shared<gfx::vk::framebuffer>(renderpass, 1280, 720, swapchain_views[i]);
    }

    gfx::vk::multi_block_allocator allocator(device);
    
    uint32_t buf_families[] ={ queue_datas[queue_index_transfer].family, queue_datas[queue_index_graphics].family };
    auto buffer = std::make_shared<gfx::vk::proxy_buffer>(device, 256, 0, VK_SHARING_MODE_CONCURRENT, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, buf_families);
    buffer->bind_memory(allocator.allocate(buffer->requirements(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    command_buffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    const auto barrier = buffer->memory_barrier(256, 0, 0, VK_ACCESS_TRANSFER_READ_BIT, queue_datas[queue_index_graphics].family, queue_datas[queue_index_transfer].family);
    command_buffer->barrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, barrier);
    command_buffer->end();
    queues[queue_index_transfer].submit(command_buffer);

    VkGraphicsPipelineCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.basePipelineHandle = nullptr;
    info.basePipelineIndex = 0;
    info.flags = 0;
    info.pNext = nullptr;
    info.renderPass = *renderpass;
    info.subpass = 0;

    VkPipelineLayoutCreateInfo loi;
    loi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    loi.flags = 0;
    loi.pNext = nullptr;
    loi.pushConstantRangeCount = 0;
    loi.pPushConstantRanges = nullptr;
    loi.setLayoutCount = 0;
    loi.pSetLayouts = nullptr;
    VkPipelineLayout layout;
    vkCreatePipelineLayout(*device, &loi, nullptr, &layout);
    info.layout = layout;

    VkPipelineColorBlendStateCreateInfo blend_state;
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.pNext = nullptr;
    blend_state.flags = 0;
    blend_state.logicOpEnable = false;
    blend_state.attachmentCount = 0;
    info.pColorBlendState = &blend_state;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.pNext = nullptr;
    depth_stencil_state.depthTestEnable = true;
    depth_stencil_state.depthWriteEnable = true;
    depth_stencil_state.minDepthBounds = 0.f;
    depth_stencil_state.maxDepthBounds = 1.f;
    depth_stencil_state.depthBoundsTestEnable = true;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER;
    depth_stencil_state.stencilTestEnable = false;
    info.pDepthStencilState = &depth_stencil_state;

    VkPipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext = nullptr;
    dynamic_state.flags = 0;
    const auto dynamic_states ={ VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
    dynamic_state.dynamicStateCount = uint32_t(std::size(dynamic_states));
    dynamic_state.pDynamicStates = std::data(dynamic_states);
    info.pDynamicState = &dynamic_state;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.flags = 0;
    input_assembly_state.pNext = nullptr;
    input_assembly_state.primitiveRestartEnable = true;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.pInputAssemblyState = &input_assembly_state;

    info.pMultisampleState = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterization_state;
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.flags = 0;
    rasterization_state.pNext = nullptr;
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.depthBiasEnable = false;
    rasterization_state.depthClampEnable = false;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.lineWidth = 1.f;
    rasterization_state.rasterizerDiscardEnable = false;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    info.pRasterizationState = &rasterization_state;

    info.pTessellationState = nullptr;
    
    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.flags = 0;
    vertex_input_state.pNext = nullptr;
    vertex_input_state.pVertexBindingDescriptions = nullptr;
    vertex_input_state.vertexBindingDescriptionCount = 0;
    vertex_input_state.pVertexAttributeDescriptions = nullptr;
    vertex_input_state.vertexAttributeDescriptionCount = 0;
    info.pVertexInputState = &vertex_input_state;

    VkPipelineViewportStateCreateInfo viewport_state;
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.flags = 0;
    viewport_state.pNext = nullptr;
    viewport_state.scissorCount = 0;
    viewport_state.viewportCount = 0;
    info.pViewportState = &viewport_state;



    while (!close_window)
    {
        const auto now = std::chrono::steady_clock::now();

        main_thread_worker.enqueue([&]() {
            if (glfwGetKey(*window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(*window, true);
        });
        log_i << "Updated";

        uint32_t image = swapchain->next_image(swap_semaphore).second;
        fences[image]->wait();
        fences[image]->reset();
        primary_buffers[image]->reset(0);
        primary_buffers[image]->begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
        primary_buffers[image]->begin_renderpass(*renderpass, *swapchain_framebuffers[image], { 0, 0, 1280, 720 }, VK_SUBPASS_CONTENTS_INLINE, {{ glm::vec4(1, 0, 0, 1) }});

        primary_buffers[image]->end_renderpass();
        primary_buffers[image]->end();
        queues[queue_index_present].submit(primary_buffers[image], render_semaphore, swap_semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, fences[image]);
        VkResult result = queues[queue_index_present].present(render_semaphore, swapchain, image);
        
        while (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - now).count() < (1/120.f));
    }
    device->wait_idle();
    terminate_threads = true;
}