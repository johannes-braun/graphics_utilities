#include <gfx/gfx.hpp>
#include <opengl/opengl.hpp>

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

class gauss_blur
{
    gl::pipeline make_pipeline(int direction) const;

public:
    gauss_blur(int kernel_size, float sigma)
            : _kernel_size(kernel_size)
            , _sigma(sigma)
    {
        std::stringstream kernel;
        for(int i = 0; i < (_kernel_size >> 1) + 1; ++i)
            kernel << 1.025f * glm::gauss(1.f * i, 0.f, _sigma) << ", ";
        _kernel                       = kernel.str();
        _blur_ppx[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("postfx/screen.vert");
        _blur_ppx[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>(
                "blurhw.frag",
                std::vector<gl::definition>{
                        gl::definition("DIRECTION", 0), gl::definition("KERNEL_SIZE", _kernel_size), gl::definition("KERNEL", _kernel)});
        _blur_ppy[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("postfx/screen.vert");
        _blur_ppy[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>(
                "blurhw.frag",
                std::vector<gl::definition>{
                        gl::definition("DIRECTION", 1), gl::definition("KERNEL_SIZE", _kernel_size), gl::definition("KERNEL", _kernel)});
    }

    void operator()(const std::shared_ptr<gl::texture>& in, const std::shared_ptr<gl::texture>& out, int level = 0)
    {
        mygl::texture texture_views[2];
        glGenTextures(2, texture_views);

        /*  assert(out->width() == in->width()
              && out->height() == in->height()
              && out->internal_format() == in->internal_format());*/
        if(!_texture || !(_texture->width() == in->width() && _texture->height() == in->height() &&
                          _texture->internal_format() == in->internal_format() && _texture->width() == out->width() &&
                          _texture->height() == out->height() && _texture->internal_format() == out->internal_format()))
            build(in->width(), in->height(), GL_R32F);

        _vao.bind();
        _fbos[0][GL_COLOR_ATTACHMENT0].set(_texture, level);
        _fbos[1][GL_COLOR_ATTACHMENT0].set(out, level);

        glViewport(0, 0, in->width() >> level, in->height() >> level);
        _blur_ppx.bind();
        glBindSampler(0, _sampler);
        // glTextureView(texture_views[0],
        //              GL_TEXTURE_2D,
        //              *in,
        //              GL_R32F,
        //              level,
        //              1,
        //              0,
        //              1);
        glBindTextureUnit(0, *in);
        _fbos[0].bind();
        _fbos[0].set_drawbuffer(GL_COLOR_ATTACHMENT0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        _blur_ppy.bind();
        glBindSampler(0, _sampler);
        /*      glTextureView(texture_views[1],
                            GL_TEXTURE_2D,
                            *_texture,
                            GL_R32F,
                            level,
                            1,
                            0,
                            1);*/
        glBindTextureUnit(0, *_texture);
        _fbos[1].bind();
        _fbos[1].set_drawbuffer(GL_COLOR_ATTACHMENT0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glDeleteTextures(2, texture_views);
    }

    const gl::framebuffer& fbo(int index) const { return _fbos[index]; }

private:
    void build(int width, int height, GLenum format) { _texture = std::make_shared<gl::texture>(GL_TEXTURE_2D, width, height, format); }

    int                          _kernel_size;
    float                        _sigma;
    std::string                  _kernel;
    gl::pipeline                 _blur_ppx;
    gl::pipeline                 _blur_ppy;
    std::shared_ptr<gl::texture> _texture;
    gl::framebuffer              _fbos[2];
    gl::sampler                  _sampler;
    gl::vertex_array             _vao;
};

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

    glDebugMessageCallback(&dbg, nullptr);

    gfx::camera            camera;
    gfx::camera_controller controller(window);

    gfx::scene_file           scene("shadowtest.dae");
    gl::buffer<gfx::vertex3d> mesh_vertices = scene.meshes[0].vertices;
    gl::buffer<gfx::index32>  mesh_indices  = scene.meshes[0].indices;
    gl::buffer<glm::mat4>     mesh_buffer(glm::scale(glm::vec3(3)), GL_DYNAMIC_STORAGE_BIT);
    gl::vertex_array          mesh_vao;
    mesh_vao.attrib(0).enable(true).format(3, GL_FLOAT, false, offsetof(gfx::vertex3d, position)).bind(0);
    mesh_vao.attrib(1).enable(true).format(2, GL_FLOAT, false, offsetof(gfx::vertex3d, uv)).bind(0);
    mesh_vao.attrib(2).enable(true).format(3, GL_FLOAT, false, offsetof(gfx::vertex3d, normal)).bind(0);
    mesh_vao.vertex_buffer(0, mesh_vertices);
    mesh_vao.element_buffer(mesh_indices);

    struct matrices
    {
        glm::mat4 view, projection;
        glm::vec3 position;
    };
    gl::buffer<matrices> matrix_buffer(matrices(), GL_DYNAMIC_STORAGE_BIT);

    gl::pipeline mesh_pipeline;
    mesh_pipeline[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("mesh.vert");
    mesh_pipeline[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>("mesh.frag");

    struct light
    {
        gfx::transform       transform;
        gfx::projection      projection{glm::radians(70.f), 1, 1, 0.01f, 1000.f, false, true};
        gl::framebuffer      framebuffer;
        gl::buffer<matrices> matrix_buffer{matrices(), GL_DYNAMIC_STORAGE_BIT};
        gl::sampler          sampler;
        glm::vec4            color{1, 1, 1, 15.f};

        std::shared_ptr<gl::texture> depth_attachment = std::make_shared<gl::texture>(GL_TEXTURE_2D, 256, 256, GL_DEPTH_COMPONENT32F, 1);
        std::shared_ptr<gl::texture> depth_resolve    = std::make_shared<gl::texture>(GL_TEXTURE_2D, 256, 256, GL_R32F, 1);

        struct info
        {
            alignas(16) glm::mat4 shadow_matrix;
            alignas(16) glm::vec4 position{0};
            alignas(16) glm::vec4 direction{0};
            alignas(16) glm::vec4 color{1, 1, 1, 15.f};
            alignas(16) uint64_t shadow_map;
        };

        light()
        {
            depth_attachment                 = std::make_shared<gl::texture>(GL_TEXTURE_2D, 256, 256, GL_DEPTH_COMPONENT32F, 1);
            framebuffer[GL_DEPTH_ATTACHMENT] = depth_attachment;
            depth_resolve                    = std::make_shared<gl::texture>(GL_TEXTURE_2D, 256, 256, GL_R32F, 1);
        }

        void begin_shadowmap()
        {
            glViewport(0, 0, 256, 256);
            matrix_buffer[0].projection = projection;
            matrix_buffer[0].position   = transform.position;
            matrix_buffer[0].view       = inverse(transform.matrix());
            matrix_buffer.synchronize();
            framebuffer.bind();
            glClear(GL_DEPTH_BUFFER_BIT);
            matrix_buffer.bind(GL_UNIFORM_BUFFER, 0);
        }

        void end_shadowmap()
        {
            static gauss_blur blur(3, 0.75f);
            blur(static_cast<std::shared_ptr<gl::texture>>(framebuffer[GL_DEPTH_ATTACHMENT]), depth_resolve);
        }

        info make_info() const
        {
            info i;
            i.shadow_map    = sampler.sample(*depth_resolve);
            i.position      = glm::vec4(transform.position, 1);
            i.direction     = glm::mat4(transform) * glm::vec4(0, 0, -1, 0);
            i.color         = color;
            i.shadow_matrix = glm::mat4(projection) * inverse(transform.matrix());
            return i;
        }
    };

    gl::pipeline mesh_shadow_pipeline;
    mesh_shadow_pipeline[GL_VERTEX_SHADER]   = std::make_shared<gl::shader>("mesh_shadow.vert");
    mesh_shadow_pipeline[GL_FRAGMENT_SHADER] = std::make_shared<gl::shader>("mesh_shadow.frag");

    std::vector<light> lights(2);
    lights[0].transform = inverse(glm::lookAt(glm::vec3(0, 14, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)));
    lights[0].color = glm::vec4(1.f, 0.6f, 0.4f, 15.f);
    lights[1].transform = inverse(glm::lookAt(glm::vec3(4, 10, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
    lights[1].color = glm::vec4(0.3f, 0.7f, 1.f, 15.f);
    gl::buffer<light::info> light_buffer({lights[0].make_info(), lights[1].make_info()}, GL_DYNAMIC_STORAGE_BIT);

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

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    while(window->update())
    {
        gui.new_frame();

        ImGui::Begin("Test");
        ImGui::End();

        main_framebuffer->clear(0, {0.5f, 0.55f, 0.6f, 1.f});
        main_framebuffer->clear(0.f, 0);

        for(auto& l : lights)
        {
            l.begin_shadowmap();
            mesh_shadow_pipeline.bind();
            mesh_vao.bind();
            mesh_buffer.bind(GL_UNIFORM_BUFFER, 1);
            glDrawElements(GL_TRIANGLES, mesh_indices.size(), GL_UNSIGNED_INT, nullptr);
            l.end_shadowmap();
        }

        light_buffer[0] = lights[0].make_info();
        light_buffer[1] = lights[1].make_info();
        light_buffer.synchronize();

        lights[1].transform = inverse(
                glm::lookAt(glm::vec3(7 * glm::sin(static_cast<float>(glfwGetTime())), 10, 7 * glm::cos(static_cast<float>(glfwGetTime()))),
                            glm::vec3(0, 0, 0),
                            glm::vec3(0, 1, 0)));

        glViewport(0, 0, 1280, 720);
        controller.update(camera);
        matrix_buffer[0].view       = inverse(camera.transform.matrix());
        matrix_buffer[0].projection = camera.projection;
        matrix_buffer[0].position   = camera.transform.position;
        matrix_buffer.synchronize();

        glBindTextureUnit(0, cubemap);
        glBindSampler(0, sampler);

        main_framebuffer->bind();
        mesh_vao.bind();
        matrix_buffer.bind(GL_UNIFORM_BUFFER, 0);
        mesh_buffer.bind(GL_UNIFORM_BUFFER, 1);
        light_buffer.bind(GL_SHADER_STORAGE_BUFFER, 0);
        mesh_pipeline.bind();
        glDrawElements(GL_TRIANGLES, mesh_indices.size(), GL_UNSIGNED_INT, nullptr);

        main_framebuffer->blit(gl::framebuffer::zero(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
        gl::framebuffer::zero().bind();
        gui.render();
    }
}