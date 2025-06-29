#include "camera.hpp"
#include <cmath>
#include <algorithm>

// =============================================================================
// 関数型実装
// =============================================================================

// ヘルパー関数：カメラベクトルの更新
auto update_camera_vectors(const CameraState& state) -> CameraState {
    CameraState new_state = state;
    
    // 新しいfront vectorを計算
    new_state.front.x = cos(glm::radians(state.yaw)) * cos(glm::radians(state.pitch));
    new_state.front.y = sin(glm::radians(state.pitch));
    new_state.front.z = sin(glm::radians(state.yaw)) * cos(glm::radians(state.pitch));
    new_state.front = glm::normalize(new_state.front);
    
    // rightとupベクトルを再計算
    new_state.right = glm::normalize(glm::cross(new_state.front, state.world_up));
    new_state.up = glm::normalize(glm::cross(new_state.right, new_state.front));
    
    return new_state;
}

// FPSカメラ変換関数群
namespace fps_transforms {
    CameraTransform move_forward(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position += state.front * distance;
            return new_state;
        };
    }

    CameraTransform move_backward(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position -= state.front * distance;
            return new_state;
        };
    }

    CameraTransform move_left(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position -= state.right * distance;
            return new_state;
        };
    }

    CameraTransform move_right(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position += state.right * distance;
            return new_state;
        };
    }

    CameraTransform move_up(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position += state.up * distance;
            return new_state;
        };
    }

    CameraTransform move_down(float distance) {
        return [distance](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.position -= state.up * distance;
            return new_state;
        };
    }

    CameraTransform rotate(float delta_yaw, float delta_pitch) {
        return [delta_yaw, delta_pitch](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.yaw += delta_yaw;
            new_state.pitch += delta_pitch;
            
            // ピッチを制限
            new_state.pitch = std::clamp(new_state.pitch, -89.0f, 89.0f);
            
            return update_camera_vectors(new_state);
        };
    }

    CameraTransform zoom(float delta_fov) {
        return [delta_fov](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.fov -= delta_fov;
            new_state.fov = std::clamp(new_state.fov, 1.0f, 120.0f);
            return new_state;
        };
    }

    CameraTransform look_at(const glm::vec3& target) {
        return [target](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            glm::vec3 direction = glm::normalize(target - state.position);
            
            // 方向ベクトルからyawとpitchを計算
            new_state.yaw = glm::degrees(atan2(direction.z, direction.x));
            new_state.pitch = glm::degrees(asin(direction.y));
            
            return update_camera_vectors(new_state);
        };
    }

    CameraTransform set_aspect_ratio(float aspect_ratio) {
        return [aspect_ratio](const CameraState& state) -> CameraState {
            CameraState new_state = state;
            new_state.aspect_ratio = aspect_ratio;
            return new_state;
        };
    }
}

// 軌道カメラ変換関数群
namespace orbit_transforms {
    OrbitTransform orbit_horizontal(float delta_azimuth) {
        return [delta_azimuth](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.azimuth += delta_azimuth;
            
            // 0-360度の範囲に正規化
            while (new_state.azimuth >= 360.0f) new_state.azimuth -= 360.0f;
            while (new_state.azimuth < 0.0f) new_state.azimuth += 360.0f;
            
            return new_state;
        };
    }

    OrbitTransform orbit_vertical(float delta_elevation) {
        return [delta_elevation](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.elevation += delta_elevation;
            new_state.elevation = std::clamp(new_state.elevation, -89.0f, 89.0f);
            return new_state;
        };
    }

    OrbitTransform zoom_in(float delta_radius) {
        return [delta_radius](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.radius -= delta_radius;
            new_state.radius = std::max(new_state.radius, 0.1f);
            return new_state;
        };
    }

    OrbitTransform zoom_out(float delta_radius) {
        return [delta_radius](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.radius += delta_radius;
            new_state.radius = std::min(new_state.radius, 100.0f);
            return new_state;
        };
    }

    OrbitTransform pan(const glm::vec3& delta_target) {
        return [delta_target](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.target += delta_target;
            return new_state;
        };
    }

    OrbitTransform set_fov(float fov) {
        return [fov](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.fov = std::clamp(fov, 1.0f, 120.0f);
            return new_state;
        };
    }

    OrbitTransform set_aspect_ratio(float aspect_ratio) {
        return [aspect_ratio](const OrbitCameraState& state) -> OrbitCameraState {
            OrbitCameraState new_state = state;
            new_state.aspect_ratio = aspect_ratio;
            return new_state;
        };
    }
}

// 行列計算関数群
namespace matrix_calculators {
    MatrixCalculator view_matrix() {
        return [](const CameraState& state) -> glm::mat4 {
            return glm::lookAt(state.position, state.position + state.front, state.up);
        };
    }

    MatrixCalculator projection_matrix() {
        return [](const CameraState& state) -> glm::mat4 {
            return glm::perspective(glm::radians(state.fov), state.aspect_ratio, state.near_plane, state.far_plane);
        };
    }

    std::function<glm::mat4(const CameraState&, const glm::mat4&)> mvp_matrix() {
        return [](const CameraState& state, const glm::mat4& model) -> glm::mat4 {
            auto view = view_matrix()(state);
            auto projection = projection_matrix()(state);
            return projection * view * model;
        };
    }

    OrbitMatrixCalculator orbit_view_matrix() {
        return [](const OrbitCameraState& state) -> glm::mat4 {
            float azimuth_rad = glm::radians(state.azimuth);
            float elevation_rad = glm::radians(state.elevation);
            
            float x = state.radius * cos(elevation_rad) * cos(azimuth_rad);
            float y = state.radius * sin(elevation_rad);
            float z = state.radius * cos(elevation_rad) * sin(azimuth_rad);
            
            glm::vec3 position = state.target + glm::vec3(x, y, z);
            return glm::lookAt(position, state.target, glm::vec3(0.0f, 1.0f, 0.0f));
        };
    }

    OrbitMatrixCalculator orbit_projection_matrix() {
        return [](const OrbitCameraState& state) -> glm::mat4 {
            return glm::perspective(glm::radians(state.fov), state.aspect_ratio, state.near_plane, state.far_plane);
        };
    }

    std::function<glm::mat4(const OrbitCameraState&, const glm::mat4&)> orbit_mvp_matrix() {
        return [](const OrbitCameraState& state, const glm::mat4& model) -> glm::mat4 {
            auto view = orbit_view_matrix()(state);
            auto projection = orbit_projection_matrix()(state);
            return projection * view * model;
        };
    }
}

// ユーティリティ関数：軌道カメラの位置取得
glm::vec3 get_orbit_position(const OrbitCameraState& state) {
    float azimuth_rad = glm::radians(state.azimuth);
    float elevation_rad = glm::radians(state.elevation);
    
    float x = state.radius * cos(elevation_rad) * cos(azimuth_rad);
    float y = state.radius * sin(elevation_rad);
    float z = state.radius * cos(elevation_rad) * sin(azimuth_rad);
    
    return state.target + glm::vec3(x, y, z);
}
