#include "window.hpp"
#include "shaders/shared.inl"
#include "camera.hpp"

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

// GLMライブラリのインクルード
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// debug
#include <iostream>
#include <string>
using namespace std::string_literals;
// GPUにメッシュを転送するタスク
template<typename T>
void upload_vertex_data_task(daxa::TaskGraph& tg, daxa::TaskBufferView vertices, const std::vector<T> & data)
{
	tg.add_task({
		.attachments = {
			daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, vertices),
		},
		.task = [=](daxa::TaskInterface ti)
		{
			// 上の行のdataのためのバッファ
			auto staging_buffer_id = ti.device.create_buffer({
				.size = sizeof(T) * data.size(),
				.allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
				.name = "my staging buffer",
			});
			ti.recorder.destroy_buffer_deferred(staging_buffer_id);
			auto * buffer_ptr = ti.device.buffer_host_address_as<std::vector<T>>(staging_buffer_id).value();
			memcpy(buffer_ptr, data.data(), data.size() * sizeof(T));
			ti.recorder.copy_buffer_to_buffer({
				.src_buffer = staging_buffer_id,
				.dst_buffer = ti.get(vertices).ids[0],
				.size = sizeof(T) * data.size(),
			});
		},
		.name = "upload vertices",
	});
}
// 実際に描画する関数
template<typename T>
void draw_vertices_task(daxa::TaskGraph& tg, 
std::shared_ptr<daxa::RasterPipeline> pipeline, 
daxa::TaskBufferView vertices, 
daxa::TaskImageView render_target, const std::vector<T>& data)
{
	tg.add_task({
		.attachments = {
			daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, vertices),
			daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, render_target),
		},
		.task = [=](daxa::TaskInterface ti)
		{
			auto const size = ti.device.info(ti.get(render_target).ids[0]).value().size;
			daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
				.color_attachments = std::array{
					daxa::RenderAttachmentInfo{
						.image_view = ti.get(render_target).view_ids[0],
						.load_op = daxa::AttachmentLoadOp::CLEAR,
						// 背景カラー
						.clear_value = std::array<daxa::f32, 4>{0.1f, 0.0f, 0.5f, 1.0f},
					},
				},
				.render_area = {.width = size.x, .height = size.y},
			});
			render_recorder.set_pipeline(*pipeline);

			float aspect_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);

			render_recorder.push_constant(MyPushConstant{
				.my_vertex_ptr = ti.device.device_address(ti.get(vertices).ids[0]).value(),
				.aspect_ratio = aspect_ratio,
			});
			render_recorder.draw({.vertex_count = static_cast<u32>(data.size())});
			ti.recorder = std::move(render_recorder).end_renderpass();
		},
		.name = "draw vertives",
	});
}

int main(int argc, char const *argv[]){
	// GLMのテスト - ベクトルと行列の基本的な操作
	glm::vec3 test_position(1.0f, 2.0f, 3.0f);
	glm::mat4 test_matrix = glm::translate(glm::mat4(1.0f), test_position);
	glm::vec4 test_vec4 = test_matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
	// GLMが正しく動作していることを確認
	std::cout << "GLM Test - Translated position: (" 
	          << test_vec4.x << ", " 
	          << test_vec4.y << ", " 
	          << test_vec4.z << ")" << std::endl;

	// =============================================================================
	// カメラシステムのテスト
	// =============================================================================
	
	std::cout << "\n=== Camera System Tests ===" << std::endl;
	
	// アプローチ2: 関数型実装のテスト
	std::cout << "\n--- Functional Approach Test ---" << std::endl;
	
	// 初期状態
	CameraState initial_state;
	initial_state.position = glm::vec3(0.0f, 0.0f, 5.0f);
	initial_state = update_camera_vectors(initial_state);
	
	std::wcout << L"Initial functional camera position: ("
	          << initial_state.position.x << L", " 
	          << initial_state.position.y << L", " 
	          << initial_state.position.z << L")" << std::endl;
	
	// 複数の変換を合成
	auto combined_transform = compose_transforms<CameraState>({
		fps_transforms::move_forward(2.0f),
		fps_transforms::move_right(1.0f),
		fps_transforms::rotate(45.0f, 30.0f)
	});
	
	auto transformed_state = combined_transform(initial_state);
	std::wcout << L"After combined transforms: position=(" 
	          << transformed_state.position.x << L", " 
	          << transformed_state.position.y << L", " 
	          << transformed_state.position.z << L"), yaw=" 
	          << transformed_state.yaw << L"°, pitch=" 
	          << transformed_state.pitch << L"°" << std::endl;
	
	// 軌道カメラの関数型テスト
	OrbitCameraState orbit_state;
	orbit_state.radius = 10.0f;
	orbit_state.elevation = 30.0f;
	
	auto orbit_combined = compose_transforms<OrbitCameraState>({
		orbit_transforms::orbit_horizontal(45.0f),
		orbit_transforms::orbit_vertical(15.0f),
		orbit_transforms::zoom_in(3.0f)
	});
	
	auto transformed_orbit = orbit_combined(orbit_state);
	auto final_orbit_pos = get_orbit_position(transformed_orbit);
	std::cout << "Functional orbit camera after transforms: position=(" 
	          << final_orbit_pos.x << ", " 
	          << final_orbit_pos.y << ", " 
	          << final_orbit_pos.z << "), radius=" 
	          << transformed_orbit.radius << std::endl;
	
	// 行列計算テスト
	std::cout << "\n--- Matrix Calculation Test ---" << std::endl;
	
	auto view_calc = matrix_calculators::view_matrix();
	auto proj_calc = matrix_calculators::projection_matrix();
	
	glm::mat4 view_matrix = view_calc(transformed_state);
	glm::mat4 proj_matrix = proj_calc(transformed_state);
	
	std::cout << "View matrix calculated successfully" << std::endl;
	std::cout << "Projection matrix calculated successfully" << std::endl;
	
	std::cout << "=== Camera System Tests Complete ===" << std::endl;

	auto window = AppWindow("Learn Daxa", 860, 600);

	daxa::Instance instance = daxa::create_instance({});
	daxa::Device device = instance.create_device_2(instance.choose_device({}, {}));
	daxa::Swapchain swapchain = device.create_swapchain
	({
		.native_window = window.get_native_handle(),
		.native_window_platform = window.get_native_platform(),
		.present_mode = daxa::PresentMode::FIFO,
		.image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
		.name = "my swapchain"
	});

	// シェーダーコンパイルなどを管理するらしい
		auto pipeline_manager = daxa::PipelineManager({
		.device = device,
		.shader_compile_options = {
			.root_paths = {
				DAXA_SHADER_INCLUDE_DIR,
				"./src/shaders",
			},
			.language = daxa::ShaderLanguage::GLSL,
			.enable_debug_info = true,
        },
		.name = "my pipeline manager",
	});

	// 前の行で作ったpipeline_managerにシェーダーを登録
	std::shared_ptr<daxa::RasterPipeline> pipeline;
	auto result = pipeline_manager.add_raster_pipeline
	({
		.vertex_shader_info = daxa::ShaderCompileInfo{
			.source = daxa::ShaderFile{"main.glsl"}
		},
		.fragment_shader_info = daxa::ShaderCompileInfo{
			.source = daxa::ShaderFile{"main.glsl"}
		},
		.color_attachments = {{.format = swapchain.get_format()}},
		.raster = {},
		.push_constant_size = sizeof(MyPushConstant),
		.name = "my pipeline"
	});
	if(result.is_err()){
		std::cerr << result.message() << std::endl;
		return -1;
	}
	pipeline = result.value();

	// 複数のポリゴンを定義（例：2つの三角形で四角形を作成）
	std::vector<MyVertex> mesh_data = {
		// 最初の三角形
		MyVertex{.position = {-1.0f, +1.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},
		MyVertex{.position = {+1.0f, +1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f}},
		MyVertex{.position = {-1.0f, -1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f}},
		
		// 2番目の三角形
		MyVertex{.position = {+1.0f, +1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f}},
		MyVertex{.position = {+1.0f, -1.0f, 0.0f}, .color = {1.0f, 1.0f, 0.0f}},
		MyVertex{.position = {-1.0f, -1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f}},
		
		// 追加の三角形（中央に小さい三角形）
		MyVertex{.position = {-0.2f, +0.2f, 0.0f}, .color = {1.0f, 0.0f, 1.0f}},
		MyVertex{.position = {+0.2f, +0.2f, 0.0f}, .color = {0.0f, 1.0f, 1.0f}},
		MyVertex{.position = {+0.0f, -0.2f, 0.0f}, .color = {1.0f, 1.0f, 1.0f}},
	};
	auto buffer_id = device.create_buffer({
		.size = sizeof(MyVertex) * mesh_data.size(),
		.name = "my vertex data"
	});
	auto task_swapchain_image = daxa::TaskImage{{
		.swapchain_image = true, .name = "swapchain image"
	}};
	auto task_vertex_buffer = daxa::TaskBuffer({
		.initial_buffers = {.buffers = std::span{&buffer_id, 1}},
		.name = "task vertex buffer",
	});

	// loop_task_graphに描画するコマンドを記録
	auto loop_task_graph = daxa::TaskGraph({
		.device = device,
		.swapchain = swapchain,
		.name = "loop",
	});
	// loop_task_graphが使うバッファを登録
	loop_task_graph.use_persistent_buffer(task_vertex_buffer);
	loop_task_graph.use_persistent_image(task_swapchain_image);
	
	
	// わからない
	draw_vertices_task(loop_task_graph, pipeline, task_vertex_buffer, task_swapchain_image, mesh_data);
	// わからない daxa::TaskGraphに登録したタスクを実行する定型文っぽい
	loop_task_graph.submit({});
	loop_task_graph.present({});
	loop_task_graph.complete({});

	auto upload_task_graph = daxa::TaskGraph({
		.device = device,
		.name = "upload",
	});

	
	upload_task_graph.use_persistent_buffer(task_vertex_buffer);
	upload_vertex_data_task(upload_task_graph, task_vertex_buffer, mesh_data);

	upload_task_graph.submit({});
	upload_task_graph.complete({});
	upload_task_graph.execute({});

	while(!window.should_close()){
		window.update();
		if(window.swapchain_out_of_date){
			swapchain.resize();

			window.swapchain_out_of_date = false;
		}
		// 次のswapchainの画像を取得
		auto swapchain_image = swapchain.acquire_next_image();
		if(swapchain_image.is_empty()){
			continue;
		}
		// 
		task_swapchain_image.set_images({
			.images = std::span{&swapchain_image, 1}
		});

		loop_task_graph.execute({});
		device.collect_garbage();
	}
	device.destroy_buffer(buffer_id);
	device.wait_idle();
	device.collect_garbage();
	return 0;
}
