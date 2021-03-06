#include "file.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <gfx/log.hpp>
#include <glm/glm.hpp>

namespace gfx
{
void handle_node(scene_file& file, aiNode* node, const aiScene* scene, const glm::mat4& transform) noexcept
{
    const glm::mat4 node_trafo = transform * transpose(reinterpret_cast<glm::mat4&>(node->mTransformation));

    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    { file.mesh.geometries.at(node->mMeshes[i]).transformation = static_cast<gfx::transform>(node_trafo); }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
    { handle_node(file, node->mChildren[i], scene, node_trafo); } 
}

scene_file::scene_file(const files::path& path) : file(path)
{
    if (!exists(file::path))
    {
        elog << "Scene \"" << path << "\" could not be loaded.";
        return;
    }
    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        file::path.string(), aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality
                                 | aiProcess_LimitBoneWeights | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate
                                 | aiProcess_GenUVCoords | aiProcess_SortByPType | aiProcess_FindDegenerates | aiProcess_FindInvalidData);

    materials.resize(scene->mNumMaterials);

#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i <static_cast<int>(scene->mNumMaterials); ++i)
    {
        const auto                ai_material = scene->mMaterials[i];
        aiString                  name;
        [[maybe_unused]] aiReturn r                = ai_material->Get(AI_MATKEY_NAME, name);
        material&                 current_material = materials[i];

        current_material.name = std::string(name.data, name.length);
        ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor4D&>(current_material.color_diffuse));
        ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, reinterpret_cast<aiColor4D&>(current_material.color_emissive));
        ai_material->Get(AI_MATKEY_COLOR_REFLECTIVE, reinterpret_cast<aiColor4D&>(current_material.color_reflective));
        ai_material->Get(AI_MATKEY_COLOR_SPECULAR, reinterpret_cast<aiColor4D&>(current_material.color_specular));
        ai_material->Get(AI_MATKEY_COLOR_TRANSPARENT, reinterpret_cast<aiColor4D&>(current_material.color_transparent));

        const auto load_if = [&](auto& into, auto... matkey) {
            aiString p;
            ai_material->Get(matkey..., p);
            std::filesystem::path texpath = file::path.parent_path() / p.C_Str();
            if (p.length != 0 && !std::filesystem::is_directory(texpath) && std::filesystem::exists(texpath))
            {
                ilog << "Loading texture from " << texpath;
                into = image_file(texpath, bits::b8, 4);
            }
        };


        load_if(current_material.texture_diffuse, AI_MATKEY_TEXTURE_DIFFUSE(0));
        load_if(current_material.texture_bump, AI_MATKEY_TEXTURE_HEIGHT(0));
    }
    const auto to_vec3 = [](const aiVector3D& vec) { return glm::vec3(vec.x, vec.y, vec.z); };
    for (int m = 0; m < static_cast<int>(scene->mNumMeshes); ++m)
    {
        const auto ai_mesh = scene->mMeshes[m];
        mesh3d       current_mesh;
        current_mesh.indices.resize(ai_mesh->mNumFaces * 3);
#pragma omp parallel for schedule(static)
        for (auto i = 0; i < static_cast<int>(ai_mesh->mNumFaces); ++i)
        {
            if (ai_mesh->mFaces[0].mNumIndices != 3) continue;
            current_mesh.indices[3 * i]     = ai_mesh->mFaces[i].mIndices[0];
            current_mesh.indices[3 * i + 1] = ai_mesh->mFaces[i].mIndices[1];
            current_mesh.indices[3 * i + 2] = ai_mesh->mFaces[i].mIndices[2];
        }

        current_mesh.vertices.resize(ai_mesh->mNumVertices);
#pragma omp parallel for schedule(static)
        for (auto i = 0; i < static_cast<int>(ai_mesh->mNumVertices); ++i)
        {
            current_mesh.vertices[i] =
                vertex3d(to_vec3(ai_mesh->mVertices[i]),
                              ai_mesh->HasTextureCoords(0) ? glm::vec2(to_vec3(ai_mesh->mTextureCoords[0][i])) : glm::vec2(0),
                              to_vec3(ai_mesh->mNormals[i]));
        }

		auto& geo = current_mesh.geometries.emplace_back();
		geo.index_count = static_cast<u32>(current_mesh.indices.size());
		geo.vertex_count = static_cast<u32>(current_mesh.vertices.size());
		mesh += current_mesh;
        mesh_material_indices[&mesh.geometries.back()] = ai_mesh->mMaterialIndex;
    }

    handle_node(*this, scene->mRootNode, scene, glm::mat4(1.f));
}
}    // namespace gfx
