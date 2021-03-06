#pragma once

#include "gfx/ecs/ecs.hpp"
#include "gfx/input/input.hpp"
#include "gfx/log.hpp"
#include "gfx/math/geometry.hpp"
#include "gfx/math/math.hpp"
#include "input.hpp"
#include <optional>

namespace gfx {
inline namespace v1 {
struct transform_component : gfx::ecs::component<transform_component>
{
    gfx::transform value;
};

struct camera_component : ecs::component<camera_component>
{
    projection projection{glm::radians(70.f), 1280, 720, 0.01f, 1000.f, false, true};
};

struct camera_matrices
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 position;
    int       do_cull = 1;
};
inline std::optional<camera_matrices> get_camera_info(ecs::ecs& ecs, ecs::entity_handle entity)
{
    auto* cam = ecs.get_component<camera_component>(entity);
    auto* tfm = ecs.get_component<transform_component>(entity);
    if (!cam || !tfm) return std::nullopt;
    return camera_matrices{inverse(tfm->value.matrix()), cam->projection.matrix(), tfm->value.position, 1};
}
inline std::optional<camera_matrices> get_camera_info(const ecs::entity& entity)
{
    const auto* const cam = entity.get<camera_component>();
    const auto* const tfm = entity.get<transform_component>();
    if (!cam || !tfm) return std::nullopt;
    return camera_matrices{inverse(tfm->value.matrix()), cam->projection.matrix(), tfm->value.position, 1};
}

struct camera_controls : ecs::component<camera_controls>
{
    Qt::Key key_forward    = Qt::Key_W;
    Qt::Key key_backward   = Qt::Key_S;
    Qt::Key key_left       = Qt::Key_A;
    Qt::Key key_right      = Qt::Key_D;
    Qt::Key key_up         = Qt::Key_E;
    Qt::Key key_down       = Qt::Key_Q;
    Qt::Key key_mod_faster = Qt::Key_Shift;
    Qt::Key key_mod_slower = Qt::Key_Control;

    float             slow_factor    = 0.2f;
    float             fast_factor    = 12.f;
    float             rotation_speed = 1.f;
    float             movement_speed = 12.f;
    bool              inverse_y      = false;
    camera_component* last_camera    = nullptr;
    transform         target_transform;
};

class user_camera_system : public ecs::system
{
public:
    user_camera_system(key_event_filter& keys) : _keys(&keys)
    {
        add_component_type(camera_component::id);
        add_component_type(transform_component::id);
        add_component_type(camera_controls::id);
        add_component_type(grabbed_cursor_component::id);
    }

    void update(double delta, ecs::component_base** components) const override
    {
        camera_component&         cam  = components[0]->as<camera_component>();
        transform_component&      trn  = components[1]->as<transform_component>();
        camera_controls&          ctrl = components[2]->as<camera_controls>();
        grabbed_cursor_component& gcc  = components[3]->as<grabbed_cursor_component>();

        if (&cam != ctrl.last_camera)
        {
            ctrl.last_camera      = &cam;
            ctrl.target_transform = trn.value;
        }

        const auto delta_pos = -gcc.delta;
        ctrl.target_transform.rotation =
            glm::quat(glm::vec3(0, glm::radians(delta_pos.x / (2 * glm::pi<float>())) * ctrl.rotation_speed, 0.f))
            * ctrl.target_transform.rotation;
        ctrl.target_transform.rotation *= glm::quat(
            glm::vec3(glm::radians((ctrl.inverse_y ? -1 : 1) * delta_pos.y / (2 * glm::pi<float>())) * ctrl.rotation_speed, 0.f, 0.f));
        ctrl.target_transform.position +=
            rotate(
                ctrl.target_transform.rotation,
                glm::vec3{static_cast<float>(_keys->key_down(ctrl.key_right)) - static_cast<float>(_keys->key_down(ctrl.key_left)),
                          static_cast<float>(_keys->key_down(ctrl.inverse_y ? ctrl.key_down : ctrl.key_up))
                              - static_cast<float>(_keys->key_down(ctrl.inverse_y ? ctrl.key_up : ctrl.key_down)),
                          static_cast<float>(_keys->key_down(ctrl.key_backward)) - static_cast<float>(_keys->key_down(ctrl.key_forward))})
            * (_keys->key_down(ctrl.key_mod_faster) ? ctrl.fast_factor : (_keys->key_down(ctrl.key_mod_slower) ? ctrl.slow_factor : 1.f))
            * static_cast<float>(delta) * ctrl.movement_speed;

        const float alpha  = float(glm::clamp(15.0 * delta, 0.0, 1.0));
        trn.value.position = mix(trn.value.position, ctrl.target_transform.position, alpha);
        trn.value.scale    = mix(trn.value.scale, ctrl.target_transform.scale, alpha);
        trn.value.rotation = glm::slerp(trn.value.rotation, ctrl.target_transform.rotation, alpha);
    }

private:
    key_event_filter* _keys;
};
}    // namespace v1
}    // namespace gfx