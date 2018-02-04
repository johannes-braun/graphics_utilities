#pragma once
#include "glad/glad.h"
#include <filesystem>
#include <jpu/memory>
#include "uniform.hpp"
#include <any>
#include <map>
#include <optional>
#include "binshader/shader.hpp"

namespace gl
{
    enum class shader_type
    {
        vertex = GL_VERTEX_SHADER,
        fragment = GL_FRAGMENT_SHADER,
        geometry = GL_GEOMETRY_SHADER,
        tesselation_evaluation = GL_TESS_EVALUATION_SHADER,
        tesselation_control = GL_TESS_CONTROL_SHADER,
        compute = GL_COMPUTE_SHADER,
    };
    
    namespace
    {
        std::experimental::filesystem::path shader_root_path = "../shaders";
        std::vector<std::experimental::filesystem::path> shader_include_directories{
            shader_root_path
        };
        const std::experimental::filesystem::path& shader_root = shader_root_path;
    }
    

    inline void setup_shader_paths(std::experimental::filesystem::path root, const std::vector<std::experimental::filesystem::path>& include_directories = {})
    {
        shader_root_path = root;
        shader_include_directories.clear();
        shader_include_directories.push_back(root);
        shader_include_directories.insert(shader_include_directories.end(), include_directories.begin(), include_directories.end());
    }


    class shader : public jpu::ref_count
    {
    public:
        explicit shader(shader_type type, const std::experimental::filesystem::path& path, const std::vector<res::definition>& definitions = {});
        explicit shader(const std::experimental::filesystem::path& path, const std::vector<res::definition>& definitions = {});

        ~shader();
        operator bool() const;
        operator unsigned() const;
        shader_type type() const;

        void reload(bool force = false);
        void reload(const std::vector<res::definition>& definitions);

        template<typename T>
        uniform<T> get_uniform(std::string_view name)
        {
            if(_uniforms.count(name)==0)
            {
                std::pair<std::string_view, all_uniform_types> p(name, all_uniform_types(gl::uniform<T>(glGetUniformLocation(_id, name.data()), _id)));
                _uniforms.insert(p);
            }
               
            return std::get<gl::uniform<T>>(_uniforms.at(name));
        }

    private:
        static shader_type type_of(const std::experimental::filesystem::path& extension)
        {
            if(extension == ".vert")
                return shader_type::vertex;
            if(extension == ".frag")
                return shader_type::fragment;
            if(extension == ".geom")
                return shader_type::geometry;
            if(extension == ".tesc")
                return shader_type::tesselation_control;
            if(extension == ".tese")
                return shader_type::tesselation_evaluation;
            if(extension == ".comp")
                return shader_type::compute;

            throw std::invalid_argument("File extension " + extension.string() + " does not refer to any shader type.");
        }

        uint32_t _id;
        shader_type _type;
        std::map<std::string_view, all_uniform_types> _uniforms;
        std::experimental::filesystem::path _path;
        std::vector<res::definition> _definitions;
    };
}
