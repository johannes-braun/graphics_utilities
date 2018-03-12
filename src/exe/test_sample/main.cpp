#include <atomic>
#include <array>
#include <jpu/log.hpp>
#include <jpu/memory.hpp>
#include <jpu/data.hpp>
#include <io/window.hpp>
#include <stack>
#include <opengl/query.hpp>
#include <numeric>
#include <res/image.hpp>



constexpr int logn = 20;

void bitonic(const int logn, uint32_t* a, const size_t len)
{
    for (int p = 0; p < logn; ++p)
    {
        for (int q = 0; q <= p; ++q)
        {
            const int d = 1 << (p - q);

#pragma omp parallel for schedule(static)
            for (int id = 0; id < len / 2; ++id)
            {

                const int gid = (id + d * (id / d));
                const bool up = ((gid >> p) & 2) == 0;

                if ((a[gid] > a[gid | d]) == up)
                    std::swap(a[gid], a[gid | d]);
            }
        }
    }
}

jpu::ref_ptr<io::window> main_window;
jpu::named_vector<std::string, jpu::ref_ptr<gl::graphics_pipeline>> graphics_pipelines;

int main()
{
    gl::shader::set_include_directories("../shaders");

    res::image icon = load_image("../res/ui/logo.png", res::image_type::u8, res::image_components::rgb_alpha);
    res::image cursor = load_image("../res/cursor.png", res::image_type::u8, res::image_components::rgb_alpha);

    main_window = jpu::make_ref<io::window>(io::api::opengl, 1280, 720, "My Window");
    main_window->set_icon(icon.width, icon.height, icon.data.get());
    main_window->set_cursor(new io::cursor(cursor.width, cursor.height, cursor.data.get(), 0, 0));
    main_window->set_max_framerate(60.f);

    std::vector<float> data2(1 << logn, GL_DYNAMIC_STORAGE_BIT);

    gl::buffer<float> buf(1 << logn, GL_DYNAMIC_STORAGE_BIT);
    std::generate(buf.begin(), buf.end(), []() { return static_cast<float>(rand()); });
    buf.bind(GL_SHADER_STORAGE_BUFFER, 0);
    glFinish();

    const auto sort_bitonic = [&]() {
        auto cmp = jpu::make_ref<gl::compute_pipeline>(new gl::shader("sort/bitonic.comp"));
        std::generate(buf.begin(), buf.end(), []() { return rand(); });
        buf.bind(GL_SHADER_STORAGE_BUFFER, 0);
        glFinish();
        const int trg = buf.size() >> 1;
        auto puni = cmp->get_uniform<int>("p");
        auto quni = cmp->get_uniform<int>("q");
        auto duni = cmp->get_uniform<int>("d");

        gl::query query(GL_TIME_ELAPSED);
        cmp->bind();
        query.begin();
        for (int p = 0; p < logn; ++p)
        {
            puni = p;
            for (int q = 0; q <= p; ++q)
            {
                const int d = 1 << (p - q);
                duni = d;
                quni = q;
                cmp->dispatch(trg);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }
        query.end();
        uint64_t time = query.get_uint64();
        log_i << time / 1000000.f;
    };

    std::string bla = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    std::string blu = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::string bli = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    log_i << bla << ", " << blu << ", " << bli;

    system("pause");
    return 0;
}