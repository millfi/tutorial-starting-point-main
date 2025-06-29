#pragma once

#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// =============================================================================
// 構造体とstd::functionによる高階関数実装
// =============================================================================

// FPSカメラの状態
struct CameraState {
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 world_up{0.0f, 1.0f, 0.0f};
    float yaw{-90.0f};
    float pitch{0.0f};
    float fov{45.0f};
    float aspect_ratio{16.0f / 9.0f};
    float near_plane{0.1f};
    float far_plane{100.0f};
};

// 軌道カメラの状態
struct OrbitCameraState {
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    float radius{5.0f};
    float azimuth{0.0f};
    float elevation{0.0f};
    float fov{45.0f};
    float aspect_ratio{16.0f / 9.0f};
    float near_plane{0.1f};
    float far_plane{100.0f};
};

// 関数型エイリアス
using CameraTransform = std::function<CameraState(const CameraState&)>;
using OrbitTransform = std::function<OrbitCameraState(const OrbitCameraState&)>;
using MatrixCalculator = std::function<glm::mat4(const CameraState&)>;
using OrbitMatrixCalculator = std::function<glm::mat4(const OrbitCameraState&)>;

// ヘルパー関数
CameraState update_camera_vectors(const CameraState& state);

// FPSカメラ変換関数群
namespace fps_transforms {
    CameraTransform move_forward(float distance);
    CameraTransform move_backward(float distance);
    CameraTransform move_left(float distance);
    CameraTransform move_right(float distance);
    CameraTransform move_up(float distance);
    CameraTransform move_down(float distance);
    CameraTransform rotate(float delta_yaw, float delta_pitch);
    CameraTransform zoom(float delta_fov);
    CameraTransform look_at(const glm::vec3& target);
    CameraTransform set_aspect_ratio(float aspect_ratio);
}

// 軌道カメラ変換関数群
namespace orbit_transforms {
    OrbitTransform orbit_horizontal(float delta_azimuth);
    OrbitTransform orbit_vertical(float delta_elevation);
    OrbitTransform zoom_in(float delta_radius);
    OrbitTransform zoom_out(float delta_radius);
    OrbitTransform pan(const glm::vec3& delta_target);
    OrbitTransform set_fov(float fov);
    OrbitTransform set_aspect_ratio(float aspect_ratio);
}

// 変換の合成関数
template<typename T>
std::function<T(const T&)> compose_transforms(const std::vector<std::function<T(const T&)>>& transforms) {
    return [transforms](const T& initial_state) -> T {
        T current_state = initial_state;
        for (const auto& transform : transforms) {
            current_state = transform(current_state);
        }
        return current_state;
    };
}

// 行列計算関数群
namespace matrix_calculators {
    MatrixCalculator view_matrix();
    MatrixCalculator projection_matrix();
    std::function<glm::mat4(const CameraState&, const glm::mat4&)> mvp_matrix();
    
    OrbitMatrixCalculator orbit_view_matrix();
    OrbitMatrixCalculator orbit_projection_matrix();
    std::function<glm::mat4(const OrbitCameraState&, const glm::mat4&)> orbit_mvp_matrix();
}

// ユーティリティ関数
glm::vec3 get_orbit_position(const OrbitCameraState& state);
