#pragma once

#include "../../input/input.hpp"
#include "../../math/math.hpp"
#include "../../math/geometry.hpp"
#include "../ecs.hpp"

namespace gfx {
inline namespace v1 {
struct transform_component : gfx::ecs::component<transform_component>
{
	gfx::transform value;
};

struct camera_component : ecs::component<camera_component>
{
    projection projection{glm::radians(70.f), 1280, 720, 0.01f, 1000.f, true, true};
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
	if (!cam || !tfm)
		return std::nullopt;
	return camera_matrices{ inverse(tfm->value.matrix()), cam->projection.matrix(), tfm->value.position, 1 };
}
inline std::optional<camera_matrices> get_camera_info(const ecs::entity& entity)
{
	const auto* const cam = entity.get<camera_component>();
	const auto* const tfm = entity.get<transform_component>();
	if (!cam || !tfm)
		return std::nullopt;
	return camera_matrices{ inverse(tfm->value.matrix()), cam->projection.matrix(), tfm->value.position, 1 };
}

struct camera_controls : ecs::component<camera_controls>
{
    cursor_controls cursor;
    keyboard_button forward  = GLFW_KEY_W;
    keyboard_button backward = GLFW_KEY_S;
    keyboard_button left     = GLFW_KEY_A;
    keyboard_button right    = GLFW_KEY_D;
    keyboard_button up       = GLFW_KEY_E;
    keyboard_button down     = GLFW_KEY_Q;
    mouse_button    grab     = GLFW_MOUSE_BUTTON_RIGHT;

    float             rotation_speed = 1.f;
    float             movement_speed = 12.f;
    bool              inverse_y      = false;
    camera_component* last_camera    = nullptr;
    transform         target_transform;
};

class user_camera_system : public ecs::system
{
public:
    user_camera_system()
    {
        add_component_type(camera_component::id);
        add_component_type(transform_component::id);
        add_component_type(camera_controls::id);
    }

    void update(double delta, ecs::component_base** components) const override
    {
        camera_component& cam  = components[0]->as<camera_component>();
		transform_component& trn  = components[1]->as<transform_component>();
        camera_controls&  ctrl = components[2]->as<camera_controls>();

        if (&cam != ctrl.last_camera)
        {
            ctrl.last_camera      = &cam;
            ctrl.target_transform = trn.value;
        }

        const auto _window = context::current()->window();
        glm::ivec2 size;
        glfwGetFramebufferSize(_window, &size.x, &size.y);
        cam.projection.perspective().screen_width  = int(size.x);
        cam.projection.perspective().screen_height = int(size.y);

        ctrl.grab.update(_window);
        ctrl.forward.update(_window);
        ctrl.backward.update(_window);
        ctrl.left.update(_window);
        ctrl.right.update(_window);
        ctrl.up.update(_window);
        ctrl.down.update(_window);
        ctrl.cursor.update_position(_window);

        if (ctrl.grab.state() == button_state::press)
            ctrl.cursor.set_mode(_window,
                                 ctrl.cursor.current_mode(_window) == cursor_mode::free ? cursor_mode::grabbed : cursor_mode::free);

        if (ctrl.cursor.current_mode(_window) == cursor_mode::grabbed)
        {
            const auto delta = -ctrl.cursor.delta();
            ctrl.target_transform.rotation =
                glm::quat(glm::vec3(0, glm::radians(delta.x / (2 * glm::pi<float>())) * ctrl.rotation_speed, 0.f))
                * ctrl.target_transform.rotation;
            ctrl.target_transform.rotation *= glm::quat(
                glm::vec3(glm::radians((ctrl.inverse_y ? -1 : 1) * delta.y / (2 * glm::pi<float>())) * ctrl.rotation_speed, 0.f, 0.f));
        }

        ctrl.target_transform.position +=
            rotate(ctrl.target_transform.rotation,
                   glm::vec3{static_cast<float>(ctrl.right.state() == button_state::down)
                                 - static_cast<float>(ctrl.left.state() == button_state::down),
                             static_cast<float>((ctrl.inverse_y ? ctrl.down : ctrl.up).state() == button_state::down)
                                 - static_cast<float>((ctrl.inverse_y ? ctrl.up : ctrl.down).state() == button_state::down),
                             static_cast<float>(ctrl.backward.state() == button_state::down)
                                 - static_cast<float>(ctrl.forward.state() == button_state::down)})
            * (glfwGetKey(_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                   ? 10.f
                   : (glfwGetKey(_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? 0.1f : 1.f))
            * static_cast<float>(delta) * ctrl.movement_speed;

        const float alpha      = float(glm::clamp(15.0 * delta, 0.0, 1.0));
		trn.value.position = mix(trn.value.position, ctrl.target_transform.position, alpha);
		trn.value.scale    = mix(trn.value.scale, ctrl.target_transform.scale, alpha);
		trn.value.rotation = glm::slerp(trn.value.rotation, ctrl.target_transform.rotation, alpha);

        ctrl.cursor.update_position(context::current()->window());
    }
};
}    // namespace v1
}    // namespace gfx