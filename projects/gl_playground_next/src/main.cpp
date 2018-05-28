#include "light.hpp"
#include "mesh.hpp"
#include <execution>
#include <gfx/gfx.hpp>
#include <numeric>
#include <opengl/opengl.hpp>
#include <random>

std::shared_ptr<gfx::window>     window;
std::unique_ptr<gl::framebuffer> main_framebuffer;

void build_framebuffer(int width, int height)
{
    main_framebuffer = std::make_unique<gl::framebuffer>();
    main_framebuffer->at(GL_COLOR_ATTACHMENT0) =
            std::make_shared<gl::texture>(GL_TEXTURE_2D_MULTISAMPLE, width, height, gl::samples::x8, GL_RGBA16F);
    main_framebuffer->at(GL_DEPTH_ATTACHMENT) =
            std::make_shared<gl::texture>(GL_TEXTURE_2D_MULTISAMPLE, width, height, gl::samples::x8, GL_DEPTH32F_STENCIL8, 1);
}
void dbg(GLenum source, GLenum type, unsigned int id, GLenum severity, int length, const char* message, const void* userParam)
{
    log_i << message;
}

int main()
{
    gl::shader::set_include_directories(std::vector<gfx::files::path>{"../shd", SOURCE_DIRECTORY "/global/shd"});
    gfx::window_hints hints{{GLFW_SAMPLES, 8}, {GLFW_OPENGL_DEBUG_CONTEXT, true}};
    window = std::make_shared<gfx::window>(gfx::apis::opengl::name, "[GL] PlaygroundNext", 1280, 720, hints);
    gfx::imgui gui(window);
    window->key_callback.add([](GLFWwindow*, int k, int sc, int ac, int md) {
        if(k == GLFW_KEY_F5 && ac == GLFW_PRESS)
            gl::pipeline::reload_all();
    });
    glfwSwapInterval(0);
    window->set_max_framerate(5000.0);

    glDebugMessageCallback(&dbg, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, false);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, false);

    gfx::camera            camera;
    gfx::camera_controller controller(window);

    gfx::scene_file scene("Bistro_Research_Exterior.fbx");

    std::mt19937                                          gen;
    std::uniform_real_distribution<float>                 dist(0.f, 1.f);
    std::vector<std::shared_ptr<gfx::tri_mesh>>           meshes;
    std::vector<std::unique_ptr<gfx::tri_mesh::instance>> instances;
    for(const auto& mesh : scene.meshes)
    {
        auto&& m                                     = meshes.emplace_back(std::make_shared<gfx::tri_mesh>(mesh.vertices, mesh.indices));
        auto&& instance                              = instances.emplace_back(m->instantiate());
        (instance->transform = mesh.transform).scale = glm::vec3(0.01f);
        instance->material.color                     = glm::vec3(dist(gen)) * glm::vec3(1.f);
        instance->material.roughness                 = dist(gen);
    }

    gl::buffer<gfx::camera::data> matrix_buffer(camera.info(), GL_DYNAMIC_STORAGE_BIT);

    gl::pipeline mesh_pipeline;
    mesh_pipeline[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("mesh.vert");
    mesh_pipeline[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>("mesh.frag");

    gl::pipeline mesh_shadow_pipeline;
    mesh_shadow_pipeline[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("mesh_shadow.vert");
    mesh_shadow_pipeline[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>("mesh_shadow.frag");

    std::vector<gfx::light> lights(4);
    lights[0].map_camera.transform = inverse(glm::lookAt(glm::vec3(4, 24, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
    lights[0].color                = glm::vec4(1.f, 0.6f, 0.4f, 15.f);
    lights[1].map_camera.transform = inverse(glm::lookAt(glm::vec3(4, 20, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
    lights[1].color                = glm::vec4(0.3f, 0.7f, 1.f, 15.f);
    lights[2].map_camera.transform = inverse(glm::lookAt(glm::vec3(-7, 8, -4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
    lights[2].color                = glm::vec4(0.4f, 0.8f, 0.1f, 13.f);
    lights[3].color                = glm::vec4(0.9f, 0.8f, 0.7f, 10.f);

    gl::buffer<gfx::light::info> light_buffer(GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT |
                                              GL_MAP_PERSISTENT_BIT);
    for(const auto& l : lights)
        light_buffer.push_back(l.make_info());
    light_buffer.map(GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);

    const auto[w, h, c] = gfx::image_file::info("indoor/posx.hdr");
    gl::texture cubemap(GL_TEXTURE_CUBE_MAP, w, h, GL_R11F_G11F_B10F);
    cubemap.assign(0, 0, 0, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posx.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 1, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/negx.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 2, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posy.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 3, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/negy.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 4, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/posz.hdr", gfx::bits::b32, 3).bytes());
    cubemap.assign(0, 0, 5, w, h, 1, GL_RGB, GL_FLOAT, gfx::image_file("indoor/negz.hdr", gfx::bits::b32, 3).bytes());
    cubemap.generate_mipmaps();
    const gl::sampler sampler;

    build_framebuffer(1280, 720);

    gl::texture           noise_texture(GL_TEXTURE_3D, 128, 128, 128, GL_R16F, 1);
    gl::compute_pipeline  simplex_pipeline(std::make_shared<gl::shader>("simplex.comp"));
    int                   sx = 1.f, sy = 1.f, sz = 1.f;
    gl::buffer<glm::vec3> box_vertices(
            {{0, 0, 0},    {0, 0, sz},  {0, sy, sz}, {sx, sy, 0},  {0, 0, 0},   {0, sy, 0},  {sx, 0, sz},  {0, 0, 0},    {sx, 0, 0},
             {sx, sy, 0},  {sx, 0, 0},  {0, 0, 0},   {0, 0, 0},    {0, sy, sz}, {0, sy, 0},  {sx, 0, sz},  {0, 0, sz},   {0, 0, 0},
             {0, sy, sz},  {0, 0, sz},  {sx, 0, sz}, {sx, sy, sz}, {sx, 0, 0},  {sx, sy, 0}, {sx, 0, 0},   {sx, sy, sz}, {sx, 0, sz},
             {sx, sy, sz}, {sx, sy, 0}, {0, sy, 0},  {sx, sy, sz}, {0, sy, 0},  {0, sy, sz}, {sx, sy, sz}, {0, sy, sz},  {sx, 0, sz}});
    gl::vertex_array box_vao;

    box_vao.attrib(0).enable(true).format(3, GL_FLOAT, false, 0).bind(0);
    box_vao.vertex_buffer(0, box_vertices);
    gl::pipeline box_pipeline;
    box_pipeline[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("inbox.vert");
    box_pipeline[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>("noise_texture.frag");
    gl::sampler       noise_sampler;
    gl::buffer<float> time(static_cast<float>(glfwGetTime()),
                           GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
    time.map(GL_MAP_WRITE_BIT);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);

    struct timer_query : gl::query
    {
        timer_query()
                : gl::query(GL_TIME_ELAPSED)
        {
        }
    };
    struct qmap : std::map<std::string, qmap>
    {
        std::map<std::string, timer_query> queries;
    };
    qmap queries;

    struct indirect
    {
        uint32_t count = 0;
        uint32_t instances = 1;
        uint32_t bla = 0;
        uint32_t blub = 0;
        uint32_t asp = 0;
    };

    gl::buffer<indirect> ind(indirect{static_cast<uint32_t>(box_vertices.size())}, GL_DYNAMIC_STORAGE_BIT);
    
                for(const auto& i : instances)
                    i->render(camera);
    while(window->update())
    {
        gui.new_frame();

        gl::framebuffer::zero().clear(0, {0.3f, 0.3f, 0.4f, 1.f});
        gl::framebuffer::zero().clear(0.f, 0);

        queries["scene"]["lights"].queries["shadowmap"].start();
        mesh_shadow_pipeline.bind();
        for(auto& l : lights)
        {
            if(l.begin_shadowmap())
            {
                gfx::mesh_provider::render_all();
                for(const auto& i : instances)
                    i->render(l.map_camera);
            }
        }
        queries["scene"]["lights"].queries["shadowmap"].finish();

        queries["scene"]["lights"].queries["update"].start();
        light_buffer.reserve(lights.size());
        int enabled_lights = 0;
        for(int i = 0; i < lights.size(); ++i)
        {
            if(lights[i].enabled)
            {
                light_buffer[enabled_lights++] = lights[i].make_info();
            }
        }

        lights[1].map_camera.transform = inverse(
                glm::lookAt(glm::vec3(7 * glm::sin(static_cast<float>(glfwGetTime())), 20, 7 * glm::cos(static_cast<float>(glfwGetTime()))),
                            glm::vec3(0, 0, 0),
                            glm::vec3(0, 1, 0)));

        lights[3].map_camera.transform.position = glm::lerp(
                lights[3].map_camera.transform.position, camera.transform.position, static_cast<float>(10.f * window->delta_time()));
        lights[3].map_camera.transform.rotation = glm::slerp(
                lights[3].map_camera.transform.rotation, camera.transform.rotation, static_cast<float>(10.f * window->delta_time()));
        queries["scene"]["lights"].queries["update"].finish();

        queries["scene"]["camera"].queries["update"].start();
        glViewport(0, 0, 1280, 720);
        controller.update(camera);
        matrix_buffer[0].view       = inverse(camera.transform.matrix());
        matrix_buffer[0].projection = camera.projection;
        matrix_buffer[0].position   = camera.transform.position;
        matrix_buffer.synchronize();
        queries["scene"]["camera"].queries["update"].finish();

        gl::framebuffer::zero().bind();

        queries["scene"].queries["render"].start();
        cubemap.bind(0);
        sampler.bind(0);
        matrix_buffer.bind(GL_UNIFORM_BUFFER, 0);
        light_buffer.bind(GL_SHADER_STORAGE_BUFFER, 0, 0, enabled_lights);
        mesh_pipeline.bind();
        gfx::mesh_provider::render_all();
        for(const auto& i : instances)
            i->render(camera);
        queries["scene"].queries["render"].finish();

        time[0] = static_cast<float>(glfwGetTime());
        time.bind(GL_UNIFORM_BUFFER, 5);
        noise_sampler.bind(15);
        noise_texture.bind(15);
        queries["simplex"].queries["update"].start();
        noise_texture.bind_image(0, GL_WRITE_ONLY);
        simplex_pipeline.dispatch(128, 128, 128);
        queries["simplex"].queries["update"].finish();
#if USE_CLOUDS
        queries["simplex"].queries["render"].start();
        glEnable(GL_BLEND);
        glCullFace(GL_FRONT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        matrix_buffer.bind(GL_UNIFORM_BUFFER, 0);
        noise_sampler.bind(0);
        noise_texture.bind(0);
        box_pipeline.bind();
        box_vao.draw(GL_TRIANGLES, box_vertices.size());
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);
        queries["simplex"].queries["render"].finish();
#endif

        ImGui::Begin("Test", nullptr, ImGuiWindowFlags_MenuBar);

        enum class tab
        {
            timings,
            lights
        };
        static tab current_tab = tab::timings;
        if(ImGui::BeginMenuBar())
        {
            if(ImGui::BeginMenu("Timings", current_tab != tab::timings))
            {
                current_tab = tab::timings;
                ImGui::CloseCurrentPopup();
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Lights", current_tab != tab::lights))
            {
                current_tab = tab::lights;
                ImGui::CloseCurrentPopup();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if(current_tab == tab::timings)
        {
            const auto qprinter = [](const qmap& map) -> void {
                auto gentimes = [](const qmap& m, auto& gt) -> std::vector<float> {
                    std::vector<float> v;
                    for(const auto& x : m)
                    {
                        const auto vrt = gt(x.second, gt);
                        v.insert(v.end(), vrt.begin(), vrt.end());
                    }
                    if(!m.queries.empty())
                    {
                        for(const auto& qp : m.queries)
                            v.emplace_back(static_cast<float>(qp.second.get<uint64_t>() / 1'000'000.0));
                    }
                    return v;
                };

                auto qprint_impl = [&gentimes](const std::string& lbl, const qmap& m, auto& impl) -> void {
                    if(!m.empty())
                    {
                        for(const auto& x : m)
                        {
                            std::vector<float> times = gentimes(x.second, gentimes);
                            const float        t     = std::reduce(std::execution::par, times.begin(), times.end(), 0.f);

                            const bool tn = ImGui::TreeNode(x.first.c_str());
                            ImGui::SameLine(0, 0);
                            ImGui::Text((": " + std::to_string(t)).c_str());
                            if(tn)
                            {
                                if(!x.second.queries.empty())
                                {
                                    for(const auto& qp : x.second.queries)
                                        ImGui::Value(qp.first.c_str(), static_cast<float>(qp.second.get<uint64_t>() / 1'000'000.0));
                                }
                                impl(x.first, x.second, impl);
                                ImGui::TreePop();
                            }
                        }
                    }
                };

                qprint_impl("Timings", map, qprint_impl);
            };

            qprinter(queries);
        }
        else if(current_tab == tab::lights)
        {
            for(int i = 0; i < lights.size(); ++i)
            {
                const std::string id = "light[" + std::to_string(i) + "]";
                ImGui::DragFloat4(id.c_str(), glm::value_ptr(lights[i].color), 0.001f, 0.f, 1000.f);
                ImGui::Checkbox((id + ".use_shadowmap").c_str(), &lights[i].use_shadowmap);
                ImGui::Checkbox((id + ".enabled").c_str(), &lights[i].enabled);

                if(i != 0)
                    ImGui::Spacing();
            }
        }
        ImGui::End();
        gui.render();
    }
}