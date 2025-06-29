#include <functional>
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// =============================================================================
// アプローチ1: constメンバ関数を用いたクラスベース実装
// =============================================================================

// FPSスタイルカメラ（一人称視点）
class FPSCamera {
private:
    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 world_up_;
    float yaw_;
    float pitch_;
    float fov_;
    float aspect_ratio_;
    float near_plane_;
    float far_plane_;

    // ベクトルを更新する内部関数（const関数内で使用）
    void update_camera_vectors(glm::vec3& front, glm::vec3& right, glm::vec3& up, float yaw, float pitch, const glm::vec3& world_up) const {
        // 新しいfront vectorを計算
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        
        // rightとupベクトルを再計算
        right = glm::normalize(glm::cross(front, world_up));
        up = glm::normalize(glm::cross(right, front));
    }

public:
    // コンストラクタ
    FPSCamera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
              const glm::vec3& world_up = glm::vec3(0.0f, 1.0f, 0.0f),
              float yaw = -90.0f,
              float pitch = 0.0f,
              float fov = 45.0f,
              float aspect_ratio = 16.0f / 9.0f,
              float near_plane = 0.1f,
              float far_plane = 100.0f)
        : position_(position), world_up_(world_up), yaw_(yaw), pitch_(pitch),
          fov_(fov), aspect_ratio_(aspect_ratio), near_plane_(near_plane), far_plane_(far_plane) {
        
        glm::vec3 front, right, up;
        update_camera_vectors(front, right, up, yaw_, pitch_, world_up_);
        front_ = front;
        right_ = right;
        up_ = up;
    }

    // 基本移動（すべてconst関数で新しいオブジェクトを返す）
    FPSCamera move_forward(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ += front_ * distance;
        return new_camera;
    }

    FPSCamera move_backward(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ -= front_ * distance;
        return new_camera;
    }

    FPSCamera move_left(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ -= right_ * distance;
        return new_camera;
    }

    FPSCamera move_right(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ += right_ * distance;
        return new_camera;
    }

    FPSCamera move_up(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ += up_ * distance;
        return new_camera;
    }

    FPSCamera move_down(float distance) const {
        FPSCamera new_camera = *this;
        new_camera.position_ -= up_ * distance;
        return new_camera;
    }

    // 回転
    FPSCamera rotate(float delta_yaw, float delta_pitch) const {
        FPSCamera new_camera = *this;
        new_camera.yaw_ += delta_yaw;
        new_camera.pitch_ += delta_pitch;
        
        // ピッチを制限（真上・真下を向きすぎないように）
        new_camera.pitch_ = std::clamp(new_camera.pitch_, -89.0f, 89.0f);
        
        // ベクトルを更新
        update_camera_vectors(new_camera.front_, new_camera.right_, new_camera.up_, 
                            new_camera.yaw_, new_camera.pitch_, world_up_);
        
        return new_camera;
    }

    // ズーム（FOV調整）
    FPSCamera zoom(float delta_fov) const {
        FPSCamera new_camera = *this;
        new_camera.fov_ -= delta_fov;
        new_camera.fov_ = std::clamp(new_camera.fov_, 1.0f, 120.0f);
        return new_camera;
    }

    // 特定の点を見る
    FPSCamera look_at(const glm::vec3& target) const {
        FPSCamera new_camera = *this;
        glm::vec3 direction = glm::normalize(target - position_);
        
        // 方向ベクトルからyawとpitchを計算
        new_camera.yaw_ = glm::degrees(atan2(direction.z, direction.x));
        new_camera.pitch_ = glm::degrees(asin(direction.y));
        
        // ベクトルを更新
        update_camera_vectors(new_camera.front_, new_camera.right_, new_camera.up_, 
                            new_camera.yaw_, new_camera.pitch_, world_up_);
        
        return new_camera;
    }

    // アスペクト比設定
    FPSCamera set_aspect_ratio(float aspect_ratio) const {
        FPSCamera new_camera = *this;
        new_camera.aspect_ratio_ = aspect_ratio;
        return new_camera;
    }

    // 行列計算
    glm::mat4 get_view_matrix() const {
        return glm::lookAt(position_, position_ + front_, up_);
    }

    glm::mat4 get_projection_matrix() const {
        return glm::perspective(glm::radians(fov_), aspect_ratio_, near_plane_, far_plane_);
    }

    glm::mat4 get_mvp_matrix(const glm::mat4& model = glm::mat4(1.0f)) const {
        return get_projection_matrix() * get_view_matrix() * model;
    }

    // ゲッター
    glm::vec3 get_position() const { return position_; }
    glm::vec3 get_front() const { return front_; }
    glm::vec3 get_up() const { return up_; }
    glm::vec3 get_right() const { return right_; }
    float get_yaw() const { return yaw_; }
    float get_pitch() const { return pitch_; }
    float get_fov() const { return fov_; }
};

// 軌道カメラ
class OrbitCamera {
private:
    glm::vec3 target_;
    float radius_;
    float azimuth_;    // 方位角（水平回転）
    float elevation_;  // 仰角（垂直回転）
    float fov_;
    float aspect_ratio_;
    float near_plane_;
    float far_plane_;

public:
    // コンストラクタ
    OrbitCamera(const glm::vec3& target = glm::vec3(0.0f, 0.0f, 0.0f),
                float radius = 5.0f,
                float azimuth = 0.0f,
                float elevation = 0.0f,
                float fov = 45.0f,
                float aspect_ratio = 16.0f / 9.0f,
                float near_plane = 0.1f,
                float far_plane = 100.0f)
        : target_(target), radius_(radius), azimuth_(azimuth), elevation_(elevation),
          fov_(fov), aspect_ratio_(aspect_ratio), near_plane_(near_plane), far_plane_(far_plane) {}

    // 軌道操作
    OrbitCamera orbit_horizontal(float delta_azimuth) const {
        OrbitCamera new_camera = *this;
        new_camera.azimuth_ += delta_azimuth;
        // 0-360度の範囲に正規化
        while (new_camera.azimuth_ >= 360.0f) new_camera.azimuth_ -= 360.0f;
        while (new_camera.azimuth_ < 0.0f) new_camera.azimuth_ += 360.0f;
        return new_camera;
    }

    OrbitCamera orbit_vertical(float delta_elevation) const {
        OrbitCamera new_camera = *this;
        new_camera.elevation_ += delta_elevation;
        // -89度から89度の範囲に制限
        new_camera.elevation_ = std::clamp(new_camera.elevation_, -89.0f, 89.0f);
        return new_camera;
    }

    // ズーム（距離調整）
    OrbitCamera zoom_in(float delta_radius) const {
        OrbitCamera new_camera = *this;
        new_camera.radius_ -= delta_radius;
        new_camera.radius_ = std::max(new_camera.radius_, 0.1f); // 最小距離制限
        return new_camera;
    }

    OrbitCamera zoom_out(float delta_radius) const {
        OrbitCamera new_camera = *this;
        new_camera.radius_ += delta_radius;
        new_camera.radius_ = std::min(new_camera.radius_, 100.0f); // 最大距離制限
        return new_camera;
    }

    // パン（ターゲット移動）
    OrbitCamera pan(const glm::vec3& delta_target) const {
        OrbitCamera new_camera = *this;
        new_camera.target_ += delta_target;
        return new_camera;
    }

    // FOV調整
    OrbitCamera set_fov(float fov) const {
        OrbitCamera new_camera = *this;
        new_camera.fov_ = std::clamp(fov, 1.0f, 120.0f);
        return new_camera;
    }

    // アスペクト比設定
    OrbitCamera set_aspect_ratio(float aspect_ratio) const {
        OrbitCamera new_camera = *this;
        new_camera.aspect_ratio_ = aspect_ratio;
        return new_camera;
    }

    // 球面座標からカルテシア座標への変換
    glm::vec3 get_position() const {
        float azimuth_rad = glm::radians(azimuth_);
        float elevation_rad = glm::radians(elevation_);
        
        float x = radius_ * cos(elevation_rad) * cos(azimuth_rad);
        float y = radius_ * sin(elevation_rad);
        float z = radius_ * cos(elevation_rad) * sin(azimuth_rad);
        
        return target_ + glm::vec3(x, y, z);
    }

    // 行列計算
    glm::mat4 get_view_matrix() const {
        glm::vec3 position = get_position();
        return glm::lookAt(position, target_, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 get_projection_matrix() const {
        return glm::perspective(glm::radians(fov_), aspect_ratio_, near_plane_, far_plane_);
    }

    glm::mat4 get_mvp_matrix(const glm::mat4& model = glm::mat4(1.0f)) const {
        return get_projection_matrix() * get_view_matrix() * model;
    }

    // ゲッター
    glm::vec3 get_target() const { return target_; }
    float get_radius() const { return radius_; }
    float get_azimuth() const { return azimuth_; }
    float get_elevation() const { return elevation_; }
    float get_fov() const { return fov_; }
};

// =============================================================================
// アプローチ2: 構造体とstd::functionによる高階関数実装
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

// ヘルパー関数：カメラベクトルの更新
CameraState update_camera_vectors(const CameraState& state) {
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
