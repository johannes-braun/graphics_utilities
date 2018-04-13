#include <memory>
#include <random>

#include <io/window.hpp>
#include <io/camera.hpp>

#include <res/geometry.hpp>
#include <res/image.hpp>

#include <tinyfd/tinyfiledialogs.h>

#include <opengl/pipeline.hpp>
#include <opengl/texture.hpp>
#include <opengl/framebuffer.hpp>
#include <opengl/query.hpp>

#include <framework/data/bvh.hpp>
#include <framework/data/grid_line_space.hpp>
#include <framework/data/gpu_data.hpp>

#include "splash.hpp"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <framework/file.hpp>

std::shared_ptr<io::window> window;
std::unique_ptr<gl::compute_pipeline> tracer;

struct tracer_data
{
    alignas(16) glm::mat4 camera_matrix;
    alignas(16) glm::vec4 camera_position;

    alignas(16) uintptr_t img;
    alignas(16) uintptr_t bvh;
    alignas(8) uintptr_t vbo;
    alignas(8) uintptr_t ibo;
    alignas(8) uintptr_t linespace;
    alignas(8) uintptr_t cubemap;
    alignas(4) float seed;
    alignas(4) int frames;
};

int main()
{
    gl::shader::set_include_directories(std::vector<gfx::files::path>{ "../shd", SOURCE_DIRECTORY "/global/shd" });

    window = std::make_unique<io::window>(io::api::opengl, 800, 600, "Simple PT");
    tracer = std::make_unique<gl::compute_pipeline>(std::make_shared<gl::shader>("trace.comp"));

    game::splash splash(window);
    splash.set_progress(0.05f, L"Loading Meshes");

    gfx::scene_file file("sphaear.dae");
    gfx::scene_file::mesh& mesh = file.meshes.begin()->second;
    splash.set_progress(0.08f, L"Building BVH");

    gfx::bvh<3> gen_bvh(gfx::shape::triangle, gfx::bvh_mode::persistent_iterators);
    gen_bvh.sort(mesh.indices.begin(), mesh.indices.end(), [&](uint32_t index) { return mesh.vertices[index].position; });
    const std::vector<gl::byte>& packed = gen_bvh.pack(sizeof(res::vertex), offsetof(res::vertex, position), sizeof(uint32_t), 0);

    splash.set_progress(0.1f, L"Building Line Space");
    gfx::grid_updated = [&](float f) mutable { splash.set_progress(0.1f + 0.3f * f, L"Building Line Space: " + std::to_wstring(int(f*100)) + L"%"); };
    gfx::grid_line_space grid(6, 6, 6, 2, 2, 2);
    grid.generate(gen_bvh);

    std::vector<std::array<std::array<std::unique_ptr<gl::buffer<gfx::line_space::line>>, 6>, 6>> line_space_storages(grid.size());
    gl::buffer<gfx::gpu::line_space_data> line_space_datas(grid.size(), GL_DYNAMIC_STORAGE_BIT);
    gl::buffer<gfx::gpu::grid_line_space_data> grid_line_space_datas(1, GL_DYNAMIC_STORAGE_BIT);

    for (int index = 0; index < grid.size(); ++index)
    {
        gfx::line_space& line_space = grid[index];
        gfx::gpu::line_space_data& line_space_data = line_space_datas[index];

        line_space_data.bounds = line_space.bounds();
        line_space_data.empty = line_space.empty();
        line_space_data.x = line_space.size_x();
        line_space_data.y = line_space.size_y();
        line_space_data.z = line_space.size_z();

        for (int start = 0; start < 6; ++start) for (int end = 0; end < 6; ++end)
        {
            line_space_storages[index][start][end] = line_space.empty() ? nullptr : std::make_unique<gl::buffer<gfx::line_space::line>>(line_space.storage()[start][end].begin(), line_space.storage()[start][end].end());
            line_space_data.storages[start][end] = (line_space.empty() || end == start) ? 0ui64 : line_space_storages[index][start][end]->handle();
        }
        splash.set_progress(0.4f + 0.3f * (float(index+1) / grid.size()), L"Uploading Line Space data: " + std::to_wstring(int(index/float(grid.size()) * 100)) + L"%");
    }
    grid_line_space_datas[0].bounds = grid.bounds();
    grid_line_space_datas[0].x = grid.size_x();
    grid_line_space_datas[0].y = grid.size_y();
    grid_line_space_datas[0].z = grid.size_z();
    grid_line_space_datas[0].line_spaces = line_space_datas.handle();

    grid_line_space_datas.synchronize();
    line_space_datas.synchronize();
    splash.set_progress(0.7f, L"Packing buffers");

    gl::buffer<res::vertex> vbo(mesh.vertices.begin(), mesh.vertices.end());
    gl::buffer<res::index32> ibo(mesh.indices.begin(), mesh.indices.end());
    gl::buffer<gl::byte> bvh(packed.begin(), packed.end());

    gl::query timer(GL_TIME_ELAPSED);

    auto render_texture = std::make_shared<gl::texture>(GL_TEXTURE_2D, 800, 600, GL_RGBA32F, 1);
    gl::image img(*render_texture, GL_RGBA32F, GL_READ_WRITE);

    gl::framebuffer framebuffer;
    framebuffer[GL_COLOR_ATTACHMENT0] = render_texture;

    io::camera camera;
    io::default_cam_controller controller;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist(0.f, 1.f);
    splash.set_progress(0.8f, L"Loading Cube Map");

    const auto [w, h, c] =  gfx::image_file::info("indoor/posx.hdr");
    gl::texture cubemap(GL_TEXTURE_CUBE_MAP, w, h, GL_R11F_G11F_B10F);
    cubemap.assign(0, 0, 0, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posx.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 1, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posy.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 3, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/negy.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 4, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posz.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 5, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/negz.hdr", gfx::bits::b32, 3).bytes());
    cubemap.generate_mipmaps();
    const gl::sampler sampler;
    splash.set_progress(0.9f, L"Finishing up");

    gl::buffer<tracer_data> data(1, GL_DYNAMIC_STORAGE_BIT);
    data[0].img = img.handle();
    data[0].vbo = vbo.handle();
    data[0].ibo = ibo.handle();
    data[0].bvh = bvh.handle();
    data[0].linespace = grid_line_space_datas.handle();
    data[0].cubemap = sampler.sample(cubemap);
    data[0].frames = 10;
    data.synchronize();
    splash.set_progress(1.f, L"Finished loading");

    while (window->update())
    {
        controller.update(camera, *window, window->delta_time());
        const glm::mat4 camera_matrix = inverse(camera.projection() * glm::mat4(glm::mat3(camera.view())));

        timer.start();
        data[0].camera_matrix = camera_matrix;
        data[0].camera_position = glm::vec4(camera.transform.position, 1.f);
        data[0].seed = dist(gen);
        data.synchronize();

        tracer->bind_uniform_buffer(0, data);
        tracer->dispatch(800, 600);

        render_texture->at(10, 10, 0) = glm::vec4(1, 0, 0, 1);

        framebuffer.blit(nullptr, GL_COLOR_BUFFER_BIT);
        timer.finish();

        ImGui::Begin("Settings");
        ImGui::Value("Time", float(timer.get<uint64_t>() / 1'000'000.0));

        static bool improve = false;
        ImGui::Checkbox("Improve", &improve);
        if (improve) data[0].frames++;
        ImGui::DragInt("Int. Samples", &data[0].frames, 0.1f, 1, 10000);

        if (ImGui::Button("Reload")) tracer->reload();
        if (ImGui::Button("Load"))
        {
            constexpr const char *fs[6] = { "*.dae", "*.fbx", "*.obj", "*.stl", "*.ply", "*.blend" };
            if (const char* item = tinyfd_openFileDialog("Open Mesh", "../", 6, fs, "mesh", false))
            {
                gfx::scene_file file(item);
                std::vector<uint32_t> indices;
                std::vector<res::vertex> vertices;
                size_t begin = 0;
                int mid = 0;
                for (const auto& mesh : file.meshes)
                {
                    vertices.reserve(vertices.size() + mesh.second.vertices.size());
                    const glm::mat4 model_matrix = static_cast<glm::mat4>(mesh.second.transform);
                    const glm::mat4 normal_matrix = transpose(inverse(model_matrix));
                    for (auto vertex : mesh.second.vertices) {
                        vertex.position = glm::vec3(model_matrix * glm::vec4(vertex.position, 1));
                        vertex.normal = glm::vec3(normal_matrix * glm::vec4(vertex.normal, 0));
                        if (mid >= 1)
                            vertex.meta = 1;
                        vertices.emplace_back(std::move(vertex));
                    }
                    ++mid;
                    indices.reserve(indices.size() + mesh.second.indices.size());
                    for (auto index : mesh.second.indices) indices.emplace_back(uint32_t(index + begin));
                    begin += mesh.second.vertices.size();
                }

                gen_bvh.sort(indices.begin(), indices.end(), [&](uint32_t index) { return vertices[index].position; });
                const std::vector<gl::byte>& packed = gen_bvh.pack(sizeof(res::vertex), offsetof(res::vertex, position), sizeof(uint32_t), 0);

                vbo = gl::buffer<res::vertex>(vertices.begin(), vertices.end());
                ibo = gl::buffer<res::index32>(indices.begin(), indices.end());
                bvh = gl::buffer<gl::byte>(packed.begin(), packed.end());
                data[0].vbo = vbo.handle();
                data[0].ibo = ibo.handle();
                data[0].bvh = bvh.handle();
                data.synchronize();
            }
        }
        if (ImGui::Button("Save"))
        {
            if (const char* path = tinyfd_saveFileDialog("Save Render", "../", 0, nullptr, "All Files"))
            {
                std::vector<float> data(800 * 600 * 4);
                render_texture->get_data(GL_RGBA, GL_FLOAT, 800 * 600 * 4 * sizeof(float), data.data());

                std::vector<uint8_t> u8data(800 * 600 * 4);
                for (int i = 0; i < 800 * 600 * 4; ++i)
                    u8data[i] = uint8_t(std::clamp(data[i] * 255, 0.f, 255.f));

                stbi_write_png(path, 800, 600, 4, u8data.data(), 0);
            }
        }
        ImGui::End();
    }

    return 0;
}