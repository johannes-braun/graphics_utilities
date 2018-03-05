#include <random>
#include <jpu/memory>
#include <jpu/data>
#include <jpu/log>
#include "io/window.hpp"
#include "opengl/framebuffer.hpp"
#include "io/camera.hpp"
#include "res/geometry.hpp"
#include "jpu/impl/geometry/bvh/bvh.hpp"
#include "res/image.hpp"
#include "opengl/query.hpp"

#include "tinyfd/tinyfiledialogs.h"
#include "opengl/pipeline.hpp"
#include "opengl/texture.hpp"
#include "opengl/buffer.hpp"
#include "stb_image_write.h"
#include "res/presets.hpp"
#include "framework/renderer.hpp"
#include <GLFW/glfw3.h>
#include "framework/gizmo.hpp"

glm::ivec2 resolution{ 1280, 720 };
jpu::ref_ptr<io::window> main_window;
jpu::named_vector<std::string, jpu::ref_ptr<gl::compute_pipeline>> compute_pipelines;
std::unique_ptr<gfx::renderer> main_renderer;

jpu::ref_ptr<gl::framebuffer> target_framebuffer;
std::array<jpu::ref_ptr<gl::texture>, 3> target_textures;
jpu::ref_ptr<gl::image> target_image;

struct mesh;
class mesh_proxy : public jpu::ref_count
{
public:
    friend mesh;
    mesh_proxy(const res::mesh& mesh)
    {
        std::vector<uint32_t> indices = mesh.indices;
        jpu::bvh<3> bvh;
        bvh.assign_to(indices, mesh.vertices, &res::vertex::position, jpu::bvh_primitive_type::triangles);
        _vertex_buffer = jpu::make_ref<gl::buffer>(mesh.vertices);
        _index_buffer = jpu::make_ref<gl::buffer>(indices);
        _bvh_buffer = jpu::make_ref<gl::buffer>(bvh.pack());
        _transform = mesh.transform;
        _idx_count = indices.size();
    }

    mesh_proxy(const std::vector<res::mesh>& meshes)
    {
        std::vector<uint32_t> indices;
        std::vector<res::vertex> vertices;

        int base_offset = 0;
        int index_offset = 0;
        for (int i = 0; i < meshes.size(); ++i)
        {
            indices.resize(meshes[i].indices.size() + indices.size());
            vertices.resize(meshes[i].vertices.size() + vertices.size());

            const glm::mat4 model = static_cast<glm::mat4>(meshes[i].transform);
            const glm::mat4 normal_matrix = transpose(inverse(model));

#pragma omp parallel for
            for (int vtx = 0; vtx < meshes[i].vertices.size(); ++vtx)
            {
                vertices[base_offset + vtx].position = model * glm::vec4(meshes[i].vertices[vtx].position, 1);
                vertices[base_offset + vtx].normal = normalize(normal_matrix * glm::vec4(meshes[i].vertices[vtx].normal, 0));
                vertices[base_offset + vtx].uv = meshes[i].vertices[vtx].uv;
                vertices[base_offset + vtx].meta = i;   // mesh material sub-index-offset
            }

#pragma omp parallel for
            for (int idx = 0; idx < meshes[i].indices.size(); ++idx)
            {
                indices[idx + index_offset] = meshes[i].indices[idx] + base_offset;
            }
            index_offset += meshes[i].indices.size();
            base_offset += meshes[i].vertices.size();
        }

        jpu::bvh<3> bvh;
        bvh.assign_to(indices, vertices, &res::vertex::position, jpu::bvh_primitive_type::triangles);


        std::vector<res::vertex> opt_vertices(indices.size());
#pragma omp parallel for
        for (int i = 0; i < opt_vertices.size(); ++i)
        {
            opt_vertices[i] = vertices[indices[i]];
            indices[i] = i;
        }

        _vertex_buffer = jpu::make_ref<gl::buffer>(opt_vertices);
        _index_buffer = jpu::make_ref<gl::buffer>(indices);
        _bvh_buffer = jpu::make_ref<gl::buffer>(bvh.pack());
        _transform = glm::mat4(1.f);
        _idx_count = indices.size();
    }

    void draw(const gl::graphics_pipeline& p) const
    {
        p.set_input_buffer(0, _vertex_buffer, sizeof(res::vertex), offsetof(res::vertex, position));
        p.set_index_buffer(_index_buffer, gl::index_type::u32);
        p.draw_indexed(gl::primitive::triangles, _idx_count);
    }

private:
    int _idx_count;
    jpu::ref_ptr<gl::buffer> _vertex_buffer;
    jpu::ref_ptr<gl::buffer> _index_buffer;
    jpu::ref_ptr<gl::buffer> _bvh_buffer;
    res::transform _transform;
};

struct material
{
    glm::vec3 glass_tint{ 1, 1, 1 };
    float roughness_sqrt = 1.0f;
    glm::vec3 reflection_tint{ 1 };
    float glass = 0.0f;
    glm::vec3 base_color{ 1, 1, 1 };
    float ior = 1.5f;
    glm::vec3 glass_scatter_color = glm::vec3(1, 1, 1);
    float glass_density = 4;

    alignas(16) float glass_density_falloff = 4;
    float extinction_coefficient = 0.01f;

};

struct mesh
{
    mesh(mesh_proxy* proxy, const intptr_t material_addr, const glm::mat4 model)
        : vertices(proxy->_vertex_buffer->address()),
        elements(proxy->_index_buffer->address()),
        bvh(proxy->_bvh_buffer->address()),
        material(material_addr),
        inv_model(inverse(model)),
        model(model)
    {}

    intptr_t vertices;
    intptr_t elements;
    intptr_t bvh;
    intptr_t material;
    glm::mat4 inv_model;
    glm::mat4 model;
};

jpu::named_vector<std::string, std::pair<jpu::ref_ptr<mesh_proxy>, std::vector<mesh*>>> meshes;
int selected_mesh = 0;

void resize(GLFWwindow*, int w, int h)
{
    resolution = { w, h };
    main_renderer->resize(resolution.x, resolution.y, 8);
    target_textures[0] = jpu::make_ref<gl::texture>(gl::texture_type::simple2d);
    target_textures[0]->storage_2d(resolution.x, resolution.y, GL_RGBA32F);
    target_textures[1] = jpu::make_ref<gl::texture>(gl::texture_type::simple2d);
    target_textures[1]->storage_2d(resolution.x, resolution.y, GL_RGBA16F);
    target_textures[2] = jpu::make_ref<gl::texture>(gl::texture_type::simple2d);
    target_textures[2]->storage_2d(resolution.x, resolution.y, GL_RGBA16F);
    target_image = jpu::make_ref<gl::image>(target_textures[0], 0, false, 0, GL_RGBA32F, GL_READ_WRITE);
    target_framebuffer = jpu::make_ref<gl::framebuffer>();
    target_framebuffer->attach(GL_COLOR_ATTACHMENT0, target_textures[0]);
    target_framebuffer->attach(GL_COLOR_ATTACHMENT1, target_textures[1]);
    target_framebuffer->attach(GL_COLOR_ATTACHMENT2, target_textures[2]);
    glViewport(0, 0, resolution.x, resolution.y);
}

int main()
{
    gl::shader::set_include_directories("../shaders");

    res::image logo = res::load_svg_rasterized("../res/ui/logo.svg", 1.0f);
    res::image cursor = load_image("../res/cursor.png", res::image_type::u8, res::image_components::rgb_alpha);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    main_window = jpu::make_ref<io::window>(io::api::opengl, resolution.x, resolution.y, "Grid");
    main_window->set_icon(logo.width, logo.height, logo.data.get());
    main_window->set_cursor(new io::cursor(cursor.width, cursor.height, cursor.data.get(), 0, 0));
    main_window->set_max_framerate(60.f);
    main_window->callbacks->framebuffer_size_callback.add(&resize);
    main_window->callbacks->key_callback.add([](GLFWwindow*, int key, int, int action, int mods) {
        if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput)
            return;
        if (key == GLFW_KEY_P && action == GLFW_PRESS)
            for (auto&& pipeline : compute_pipelines)
                pipeline->reload_stages();
        if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
            ++selected_mesh;
    });
    main_renderer = std::make_unique<gfx::renderer>(resolution.x, resolution.y, 8);
    resize(*main_window, resolution.x, resolution.y);

    io::camera cam;
    io::default_cam_controller ctrl;
    cam.transform.position = glm::vec3(0, 0, 4);

    const auto sampler = jpu::make_ref<gl::sampler>();

    std::vector<res::image> cubemap_images;
    cubemap_images.emplace_back(load_image("../res/ven/hdr/posx.hdr", res::image_type::f32, res::image_components::rgb));
    cubemap_images.emplace_back(load_image("../res/ven/hdr/negx.hdr", res::image_type::f32, res::image_components::rgb));
    cubemap_images.emplace_back(load_image("../res/ven/hdr/posy.hdr", res::image_type::f32, res::image_components::rgb));
    cubemap_images.emplace_back(load_image("../res/ven/hdr/negy.hdr", res::image_type::f32, res::image_components::rgb));
    cubemap_images.emplace_back(load_image("../res/ven/hdr/posz.hdr", res::image_type::f32, res::image_components::rgb));
    cubemap_images.emplace_back(load_image("../res/ven/hdr/negz.hdr", res::image_type::f32, res::image_components::rgb));

    const int w = cubemap_images[0].width;
    const int h = cubemap_images[0].height;
    auto cubemap = jpu::make_ref<gl::texture>(gl::texture_type::cube_map);
    cubemap->storage_2d(w, h, GL_R11F_G11F_B10F);
    cubemap->assign_3d(0, 0, 0, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[0].data.get());
    cubemap->assign_3d(0, 0, 1, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[1].data.get());
    cubemap->assign_3d(0, 0, 2, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[2].data.get());
    cubemap->assign_3d(0, 0, 3, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[3].data.get());
    cubemap->assign_3d(0, 0, 4, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[4].data.get());
    cubemap->assign_3d(0, 0, 5, w, h, 1, 0, GL_RGB, GL_FLOAT, cubemap_images[5].data.get());
    cubemap->generate_mipmaps();
    cubemap_images.clear();

    std::mt19937 gen;
    const std::uniform_real_distribution<float> dist(0.f, 1.f);

    jpu::ref_ptr<gl::buffer> material_buffer;
    jpu::ref_ptr<gl::buffer> meshes_buffer;

    const auto load_mesh = [&](const auto& path, const bool combine) {
        auto geometry = res::load_geometry(path);
        if (combine)
        {
            // Combine all vertex data into one object

            using namespace jpu::flags_operators;
            material_buffer = jpu::make_ref<gl::buffer>(geometry.meshes.size() * sizeof(material), gl::buffer_flag_bits::map_persistent | gl::buffer_flag_bits::map_write | gl::buffer_flag_bits::dynamic_storage);
            meshes_buffer = jpu::make_ref<gl::buffer>(sizeof(mesh), gl::buffer_flag_bits::map_persistent | gl::buffer_flag_bits::map_write | gl::buffer_flag_bits::dynamic_storage);
            for (int i = 0; i < geometry.meshes.size(); ++i)
            {
                material_buffer->data_as<material>()[i] = material();
            }
            meshes[geometry.meshes.id_by_index(0)] = std::make_pair(jpu::make_ref<mesh_proxy>(geometry.meshes.payloads()), std::vector<mesh*>());
            meshes.get_by_index(0).second.push_back(&(meshes_buffer->data_as<mesh>()[0] = mesh(meshes.get_by_index(0).first, material_buffer->address(), glm::mat4(1.f))));
        }
        else
        {
            // Keep each object as it's own mesh

            using namespace jpu::flags_operators;
            material_buffer = jpu::make_ref<gl::buffer>(geometry.meshes.size() * sizeof(material), gl::buffer_flag_bits::map_persistent | gl::buffer_flag_bits::map_write | gl::buffer_flag_bits::dynamic_storage);
            meshes_buffer = jpu::make_ref<gl::buffer>(geometry.meshes.size() * sizeof(mesh), gl::buffer_flag_bits::map_persistent | gl::buffer_flag_bits::map_write | gl::buffer_flag_bits::dynamic_storage);
            for (int i = 0; i < geometry.meshes.size(); ++i)
            {
                material_buffer->data_as<material>()[i] = material();
                meshes[geometry.meshes.id_by_index(i)] = std::make_pair(jpu::make_ref<mesh_proxy>(geometry.meshes.get_by_index(i)), std::vector<mesh*>());
                meshes.get_by_index(i).second.push_back(&(meshes_buffer->data_as<mesh>()[i] = mesh(meshes.get_by_index(i).first, material_buffer->address() + i * sizeof(material), geometry.meshes.get_by_index(i).transform)));
            }
        }
    };
    load_mesh("../res/cube.dae", true);

    gl::query query_time(GL_TIME_ELAPSED);

    const auto pp_trace = compute_pipelines.push("Trace", jpu::make_ref<gl::compute_pipeline>(new gl::shader("pathtracer/trace.comp")));
    gl::graphics_pipeline pp_chunk; pp_chunk.use_stages(new gl::shader("pathtracer/chunk.vert"), new gl::shader("pathtracer/chunk.frag"));
    gl::graphics_pipeline pp_mesh; pp_mesh.use_stages(new gl::shader("pathtracer/mesh.vert"), new gl::shader("pathtracer/mesh.frag"));
    pp_mesh.set_input_format(0, 3, GL_FLOAT, false);
    gl::graphics_pipeline bilateral_pipeline; bilateral_pipeline.use_stages(new gl::shader("postprocess/screen.vert"), new gl::shader("postprocess/filter/bilateral.frag"));

    gfx::gizmo gizmo;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    struct pathtracer_info
    {
        alignas(16) uint64_t target_image;
        alignas(16) glm::ivec2 offset;
        alignas(8)  uint64_t cubemap;
        alignas(4)  float random_gen;
        alignas(4)  int num_meshes;
        alignas(16) glm::mat4 camera_matrix;
        alignas(16) uint64_t meshes;
        alignas(16) glm::vec3 camera_position;
        alignas(4)  int max_samples;
        alignas(4)  int sample_blend_offset;
        alignas(4)  float max_bounces;
    };
    log_i << sizeof(pathtracer_info);
    gl::buffer pathtracer_info_buffer(sizeof(pathtracer_info), gl::buffer_flag_bits::dynamic_storage);
    reinterpret_cast<void(*)(GLenum)>(glfwGetProcAddress("glEnableClientState"))(GL_UNIFORM_BUFFER_UNIFIED_NV);

    while (main_window->update())
    {
        ctrl.update(cam, *main_window, main_window->delta_time());

        const glm::mat4 camera_matrix = inverse(cam.projection() * glm::mat4(glm::mat3(cam.view())));

        selected_mesh = selected_mesh % meshes.size();
        res::transform gizmo_transform = meshes.get_by_index(selected_mesh).second.front()->model;
        gizmo.transform = &gizmo_transform;

        static int size = 12;
        static int frame = 0;
        static int max_samples = 2;
        static int sample_blend_offset = 0;
        static bool show_grid = true;
        static bool show_gizmo = true;
        static bool show_renderchunk = true;
        static bool show_bfilter = true;
        static bool enable_continuous_improvement = false;
        static float gauss_sigma = 6.f;
        static float gauss_bsigma = 0.1f;

        const int size_x = pp_trace->work_group_sizes()[0] * size;
        const int size_y = pp_trace->work_group_sizes()[0] * size;
        const int count_x = ceil(static_cast<float>(resolution.x) / size_x);
        const int count_y = ceil(static_cast<float>(resolution.y) / size_y);


        double t = 0.0;
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, 0, pathtracer_info_buffer.address(), pathtracer_info_buffer.size());

        while (t < main_window->get_swap_delay() / 2.f)
        {
            query_time.begin();
            pp_trace->bind();
            pathtracer_info info;
            info.target_image = *target_image;
            info.random_gen = dist(gen);
            info.sample_blend_offset = sample_blend_offset;
            info.max_samples = max_samples;
            info.camera_position = cam.transform.position;
            info.camera_matrix = camera_matrix;
            info.cubemap = sampler->sample_texture(cubemap);
            info.meshes = meshes_buffer->address();
            info.num_meshes = meshes_buffer->size() / sizeof(mesh);
            info.max_bounces = 8;
            info.offset = glm::ivec2(size_x * (frame % count_x), size_y * (frame / count_x));
            pathtracer_info_buffer.assign(&info, 1);
            pp_trace->dispatch(size_x, size_y);
            frame = (frame + 1) % (count_x * count_y);
            if (frame == 0 && enable_continuous_improvement)
                ++sample_blend_offset;
            query_time.end();
            t += query_time.get_uint64() / 1'000'000'000.0;
        }

        target_framebuffer->draw_to_attachments({ GL_COLOR_ATTACHMENT1 });
        main_renderer->bind();
        bilateral_pipeline.bind();
        bilateral_pipeline.get_uniform<float>(gl::shader_type::fragment, "gauss_bsigma") = gauss_bsigma;
        bilateral_pipeline.get_uniform<uint64_t>(gl::shader_type::fragment, "src_textures[0]") = sampler->sample_texture(target_textures[0]);
        bilateral_pipeline.get_uniform<float>(gl::shader_type::fragment, "gauss_sigma") = show_bfilter ? gauss_sigma : 0;
        bilateral_pipeline.draw(gl::primitive::triangles, 3);
        main_renderer->draw(main_window->delta_time(), *target_framebuffer);

        target_framebuffer->read_from_attachment(GL_COLOR_ATTACHMENT1);
        target_framebuffer->blit(nullptr, gl::framebuffer::blit_rect{ 0, 0, resolution.x, resolution.y }, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        if (show_renderchunk)
        {
            glLineWidth(2.f);
            pp_chunk.bind();
            pp_chunk.get_uniform<glm::vec2>(gl::shader_type::vertex, "inv_resolution") = 1.f / glm::vec2(resolution);
            pp_chunk.get_uniform<glm::vec2>(gl::shader_type::vertex, "offset") = glm::vec2(size_x * (frame % count_x), size_y * (frame / count_x));
            pp_chunk.get_uniform<glm::vec2>(gl::shader_type::vertex, "size") = glm::vec2(size_x, size_y);
            pp_chunk.draw(gl::primitive::line_strip, 5);
        }

        if (show_grid)
        {
            for (auto&& p : meshes)
            {
                glLineWidth(0.1f);
                glPolygonMode(GL_FRONT, GL_LINE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                pp_mesh.bind();
                for (auto&& s : p.second)
                {
                    pp_mesh.get_uniform<glm::mat4>(gl::shader_type::vertex, "model_view_projection") = cam.projection() * cam.view() * s->model;
                    p.first->draw(pp_mesh);
                }
                glPolygonMode(GL_FRONT, GL_FILL);
                glDisable(GL_BLEND);
            }
        }

        double mx, my; glfwGetCursorPos(*main_window, &mx, &my);
        gizmo.update(cam.view(), cam.projection(), glfwGetMouseButton(*main_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS, mx / resolution.x, my / resolution.y);
        if (show_gizmo) gizmo.render();
        meshes.get_by_index(selected_mesh).second.front()->model = static_cast<glm::mat4>(gizmo_transform);
        meshes.get_by_index(selected_mesh).second.front()->inv_model = inverse(static_cast<glm::mat4>(gizmo_transform));

        ImGui::Begin("Window");
        ImGui::Value("Frametime", static_cast<float>(1'000 * main_window->delta_time()));
        ImGui::DragInt("Size", &size, 0.1f, 1.f, 100.f);
        ImGui::DragInt("Max. Samples", &max_samples, 0.01f, 1.f, 100.f);
        ImGui::DragInt("Sample Blend Offset", &sample_blend_offset, 0.01f, 1.f, 100.f);
        ImGui::Checkbox("Show Mesh", &show_grid);
        ImGui::Checkbox("Show Gizmo", &show_gizmo);
        ImGui::Checkbox("Show Chunk", &show_renderchunk);
        if (ImGui::Checkbox("Continuous Improvement", &enable_continuous_improvement) && enable_continuous_improvement)
        {
            sample_blend_offset = 0;
        }
        ImGui::Checkbox("Enable PostProcess", &main_renderer->enabled);
        ImGui::Checkbox("Enable Filtering", &show_bfilter);
        if (show_bfilter)
        {
            ImGui::DragFloat("Gauss Sigma", &gauss_sigma);
            ImGui::DragFloat("Gauss B-Sigma", &gauss_bsigma);
        }

        static bool with_bg_alpha = true;
        ImGui::Checkbox("Enable Background Alpha", &with_bg_alpha);
        if (ImGui::Button("Save", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
        {
            res::stbi_data tex_data(malloc(resolution.x * resolution.y * 4 * sizeof(float)));
            target_textures[1]->get_texture_data(GL_RGBA, GL_FLOAT, resolution.x * resolution.y * 4 * sizeof(float), tex_data.get());

            glm::vec4* ic = static_cast<glm::vec4*>(tex_data.get());
            std::vector<glm::u8vec4> convert(resolution.x * resolution.y);
            for (int x = 0; x < resolution.x; ++x)
            {
                for (int y = 0; y < resolution.y; ++y)
                {
                    convert[y * resolution.x + x] = clamp(ic[(resolution.y - 1 - y) * resolution.x + x] * 255.f, glm::vec4(0, 0, 0, with_bg_alpha ? 0 : 255.f), glm::vec4(255.f));
                }
            }

            constexpr const char *extensions[1] = {
                "*.png"
            };
            if (const auto dst = tinyfd_saveFileDialog("Save output", "../res/", 1, extensions, "*.png"))
                stbi_write_png(dst, resolution.x, resolution.y, 4, convert.data(), 0);
        }
        static bool combine_mesh = true;
        ImGui::Checkbox("Combine mesh when loading", &combine_mesh);
        if (ImGui::Button("Open", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
        {
            constexpr const char *fs[6] = {
                "*.dae", "*.fbx", "*.obj", "*.stl", "*.ply", "*.blend"
            };
            if (const auto src = tinyfd_openFileDialog("Open Mesh", "../res/", 6, fs, "mesh", false))
            {
                meshes.clear();
                load_mesh(src, combine_mesh);
            }
        }

        for (int i = 0; i < material_buffer->size() / sizeof(material); ++i)
        {
            const auto id = ("Material " + std::to_string(i));
            if (ImGui::CollapsingHeader(id.c_str()))
            {
                ImGui::PushID(id.c_str());
                auto&& item = material_buffer->data_as<material>() + i;
                ImGui::DragFloat("Roughness", &item->roughness_sqrt, 0.01f, 0.f, 1.f);
                ImGui::DragFloat("Glass", &item->glass, 0.01f, 0.f, 1.f);
                ImGui::DragFloat("IOR", &item->ior, 0.01f, 0.f, 100.f);
                ImGui::DragFloat("Ex. Coeff.", &item->extinction_coefficient, 0.01f, 0.f, 100.f);
                ImGui::Spacing();
                ImGui::ColorEdit3("Color", &item->base_color[0]);
                ImGui::ColorEdit3("Reflection Tint", &item->reflection_tint[0]);
                ImGui::ColorEdit3("Glass Tint", &item->glass_tint[0]);
                ImGui::ColorEdit3("Glass Scatter Tint", &item->glass_scatter_color[0]);
                ImGui::Spacing();
                ImGui::DragFloat("Glass Density", &item->glass_density, 0.01f, 0.f, 100.f);
                ImGui::DragFloat("Glass Density Falloff", &item->glass_density_falloff, 0.01f, 0.f, 100.f);
                ImGui::PopID();
            }
        }

        ImGui::End();
    }

    return 0;
}