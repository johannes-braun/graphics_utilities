#include "mesh.hpp"
#include <executable.hpp>
#include <random>

void executable::init(gfx::context_options& opt)
{
    opt.window_title        = "[11] Incremental Pathtracer";
    opt.framebuffer_samples = gfx::sample_count::x1;
    opt.graphics_api        = gfx::gapi::vulkan;
    opt.max_framerate       = 100000.f;
}

struct helper_info
{
    glm::mat4 inverse_view_proj;
    float     time;
    gfx::u32  reset;
    float     rngval;
    uint32_t  bounce_limit;
};

void executable::run()
{
    user_entity->get<gfx::transform_component>()->value.position.z += 4.f;

    gfx::image accumulation_cache(gfx::img_type::image2d, gfx::rgba32f, gfx::extent(1280, 720), 1);
    gfx::image bounce_cache(gfx::img_type::image2d, gfx::rgba32f, gfx::extent(1280, 720), 1);
    gfx::image direction_cache(gfx::img_type::image2d, gfx::rgba32f, gfx::extent(1280, 720), 1);
    gfx::image origin_cache(gfx::img_type::image2d, gfx::rgba32f, gfx::extent(1280, 720), 1);
    gfx::image counter_cache(gfx::img_type::image2d, gfx::rg32u, gfx::extent(1280, 720), 1);

    const auto accumulation_cache_view = accumulation_cache.view(gfx::imgv_type::image2d);
    const auto bounce_cache_view       = bounce_cache.view(gfx::imgv_type::image2d);
    const auto direction_cache_view    = direction_cache.view(gfx::imgv_type::image2d);
    const auto origin_cache_view       = origin_cache.view(gfx::imgv_type::image2d);
    const auto counter_cache_view      = counter_cache.view(gfx::imgv_type::image2d);

    gfx::image&           cubemap      = res.cubemaps_hdr["hdri/hdr"];
    const gfx::image_view cubemap_view = cubemap.view(gfx::imgv_type::image_cube);
    const gfx::sampler    sampler;

    gfx::binding_layout trace_images_layout;
    trace_images_layout.push(gfx::binding_type::sampled_image);

    gfx::renderpass_layout trace_pass_layout;
    trace_pass_layout.add_color_attachment(gfx::rgba16f);
    std::vector<gfx::framebuffer> trace_fbos;
    std::vector<gfx::image>       trace_fbo_images;
    std::vector<gfx::image_view>  trace_fbo_image_views;
    std::vector<gfx::binding_set> trace_images_sets;
    for (int i = 0; i < context->swapchain()->image_views().size(); ++i)
    {
        auto& img = trace_fbo_images.emplace_back(gfx::img_type::attachment, gfx::rgba16f,
                                                  gfx::extent(options.window_width, options.window_height), 1);
        auto& iv  = trace_fbo_image_views.emplace_back(gfx::imgv_type::image2d, img);
        trace_fbos.emplace_back(options.window_width, options.window_height, 1, trace_pass_layout);
        trace_fbos.back().attach(gfx::attachment::color, 0, iv);
        trace_images_sets.emplace_back(trace_images_layout).bind(0, iv, sampler);
    }

    gfx::binding_layout trace_layout;
    trace_layout.push(gfx::binding_type::uniform_buffer);
    trace_layout.push(gfx::binding_type::uniform_buffer);
    trace_layout.push(gfx::binding_type::storage_buffer);
    trace_layout.push(gfx::binding_type::storage_buffer);
    trace_layout.push(gfx::binding_type::storage_buffer);
    trace_layout.push(gfx::binding_type::storage_buffer);
    trace_layout.push(gfx::binding_type::sampled_image);
    trace_layout.push(gfx::binding_type::storage_image);
    trace_layout.push(gfx::binding_type::storage_image);
    trace_layout.push(gfx::binding_type::storage_image);
    trace_layout.push(gfx::binding_type::storage_image);
    trace_layout.push(gfx::binding_type::storage_image);

    gfx::pipe_state::binding_layouts trace_layouts;
    trace_layouts.layouts.push_back(&trace_layout);

    gfx::pipe_state trace_state;
    trace_state.state_bindings    = &trace_layouts;
    trace_state.state_multisample = &msaa_state;

    gfx::graphics_pipeline trace_pipeline(trace_state, trace_pass_layout,
                                          {gfx::shader(gfx::shader_type::vert, "postfx/screen.vert"),
                                           gfx::shader(gfx::shader_type::frag, "11_pathtracer_incremental/trace.frag")});

    gfx::pipe_state::binding_layouts filter_layouts;
    filter_layouts.layouts.push_back(&trace_images_layout);
    gfx::pipe_state filter_state;
    filter_state.state_bindings    = &filter_layouts;
    filter_state.state_multisample = &msaa_state;
    gfx::graphics_pipeline filter_pipeline(
        filter_state, pass_layout,
        {gfx::shader(gfx::shader_type::vert, "postfx/screen.vert"), gfx::shader(gfx::shader_type::frag, "postfx/filter/bilateral.frag")});

    mesh_allocator meshes;
    std::mt19937   gen;

    std::uniform_real_distribution<float> dist;
    auto span = meshes.allocate_meshes(res.scenes["tree.dae"]);
	auto leaves = span[1], trunks = span[0];
    for (int c = 0; c <= 5; ++c)
    {
        const auto scale = 0.5f + 2.f * dist(gen);
        const auto pos   = glm::vec3(40.f * dist(gen), 0.f, 40.f * dist(gen));
        meshes.add_instance(leaves,
                            gfx::transform(pos, glm::vec3(scale), glm::angleAxis(glm::radians(90.f), glm::vec3(-1, 0, 0))),
                            glm::vec4(0.2f, 1.f, 0.01f, 1.f), 0.5f, 0.1f);
        meshes.add_instance(trunks,
                            gfx::transform(pos, glm::vec3(scale), glm::angleAxis(glm::radians(90.f), glm::vec3(-1, 0, 0))),
                            glm::vec4(0.4f, 0.3f, 0.1f, 1.f), 0.5f, 0.1f);
    }
     const auto xube = meshes.allocate_meshes(res.scenes["floor.dae"])[0];
     meshes.add_instance(xube,
    	{ {3, -0.5f, 0}, glm::vec3(8, 8, 1), glm::angleAxis(glm::radians(-90.f), glm::vec3(1, 0, 0)) }, glm::vec4(1, 1, 1, 1), 0.15f,
    	0.5f);

    gfx::hbuffer<helper_info>     helper_info_buffer(1);
    std::vector<gfx::binding_set> trace_sets;

    for (auto i = 0ull; i < gfx::context::current()->swapchain()->image_views().size(); ++i)
    {
        auto& trace_set = trace_sets.emplace_back(trace_layout);
        trace_set.bind(0, *camera_buffer);
        trace_set.bind(1, helper_info_buffer);
        trace_set.bind(2, meshes.bvh_buffer());
        trace_set.bind(3, meshes.vertex_buffer());
        trace_set.bind(4, meshes.index_buffer());
        trace_set.bind(5, meshes.instances());
        trace_set.bind(6, cubemap_view, sampler);
        trace_set.bind(7, accumulation_cache_view);
        trace_set.bind(8, bounce_cache_view);
        trace_set.bind(9, direction_cache_view);
        trace_set.bind(10, origin_cache_view);
        trace_set.bind(11, counter_cache_view);
    }
    gfx::transform last_cam = user_entity->get<gfx::transform_component>()->value;

    int iteration   = 0;
    int max_bounces = 5;
    while (frame())
    {
        auto& trace_set = trace_sets[gfx::context::current()->swapchain()->current_image()];
        trace_set.bind(2, meshes.bvh_buffer());
        trace_set.bind(3, meshes.vertex_buffer());
        trace_set.bind(4, meshes.index_buffer());
        trace_set.bind(5, meshes.instances());
        const auto reset = [&] { iteration = 0; };

        ImGui::Begin("Settings");
        if (ImGui::Button("Reset Tracer")) reset();
        ImGui::Value("Iteration", iteration);
        ImGui::Value("Min. Samples", iteration / max_bounces);
        ImGui::Value("Time", time);

        ImGui::End();

        if (last_cam != user_entity->get<gfx::transform_component>()->value)
        {
            last_cam = user_entity->get<gfx::transform_component>()->value;
            reset();
        }

        const auto then = std::chrono::steady_clock::now();

        helper_info_buffer[0].inverse_view_proj =
            glm::mat4(glm::mat3(glm::inverse(camera_buffer->at(0).view))) * inverse(camera_buffer->at(0).projection);
        helper_info_buffer[0].time         = float(glfwGetTime()) * dist(gen);
        helper_info_buffer[0].reset        = iteration++;
        helper_info_buffer[0].rngval       = dist(gen);
        helper_info_buffer[0].bounce_limit = max_bounces;

        current_command->begin_pass(trace_fbos[context->swapchain()->current_image()]);
        current_command->bind_pipeline(trace_pipeline, {&trace_set});
        current_command->draw(3);
        current_command->end_pass();

        current_command->begin_pass(*current_framebuffer);
        current_command->bind_pipeline(filter_pipeline, {&trace_images_sets[context->swapchain()->current_image()]});
        current_command->draw(3);
        current_command->end_pass();
    }
}