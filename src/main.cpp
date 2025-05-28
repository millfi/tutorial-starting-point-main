#include "window.hpp"
#include "shaders/shared.inl"

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

// GPUにメッシュを転送するタスク
void upload_vertex_data_task(daxa::TaskGraph& tg, daxa::TaskBufferView vertices)
{
	tg.add_task({
		.attachments = {
			daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, vertices),
		},
		.task = [=](daxa::TaskInterface ti)
		{
			auto data = std::array{
				MyVertex{.position = {-0.8f, +0.3f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},
				MyVertex{.position = {-0.4f, +0.3f, 0.0f}, .color = {0.0f, 1.0f, 0.0f}},
				MyVertex{.position = {-0.4f, -0.3f, 0.0f}, .color = {0.0f, 0.0f, 1.0f}},
				MyVertex{.position = {-0.8f, -0.3f, 0.0f}, .color = {1.0f, 1.0f, 0.0f}},
			};
			// 上の行のdataのためのバッファ
			auto staging_buffer_id = ti.device.create_buffer({
				.size = sizeof(data),
				.allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
				.name = "my staging buffer",
			});
		}
	});
}
// 実際に描画する関数
void draw_vertices_task(daxa::TaskGraph& tg, 
std::shared_ptr<daxa::RasterPipeline> pipeline, 
daxa::TaskBufferView vertices, 
daxa::TaskImageView render_target){
	tg.add_task({
		.attachments = {
			daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, vertices),
			daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, daxa::ImageViewType::REGULAR_2D, render_target),
		},
		.task = [=](daxa::TaskInterface ti){
			auto const size = ti.device.info(ti.get(render_target).ids[0]).value().size;
			daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
				.color_attachments = std::array{
					daxa::RenderAttachmentInfo{
						.image_view = ti.get(render_target).view_ids[0],
						.load_op = daxa::AttachmentLoadOp::CLEAR,
						.clear_value = std::array<daxa::f32, 4>{0.1f, 0.0f, 0.5f, 1.0f},
					},
				},
				.render_area = {.width = size.x, .height = size.y},
			});
			render_recorder.set_pipeline(*pipeline);
			render_recorder.push_constant(MyPushConstant{
				.my_vertex_ptr = ti.device.device_address(ti.get(vertices).ids[0]).value(),
			});
			render_recorder.draw({.vertex_count = 3});
			ti.recorder = std::move(render_recorder).end_renderpass();
		},
		.name = "draw vertives",
	});
}
int main(int argc, char const *argv[]){
	auto window = AppWindow("Learn Daxa", 860, 600);

	daxa::Instance instance = daxa::create_instance({});
	daxa::Device device = instance.create_device_2(instance.choose_device({}, {}));
	daxa::Swapchain swapchain = device.create_swapchain({
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
                "./src/shader",
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .name = "my pipeline manager"
    });
	// 前の行で作ったpipeline_managerにシェーダーを登録
	std::shared_ptr<daxa::RasterPipeline> pipeline;
	auto result = pipeline_manager.add_raster_pipeline({
		.vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
		.fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
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


	auto buffer_id = device.create_buffer({
		.size = sizeof(MyVertex) * 3,
		.name = "my vertex data"
	});
	auto task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
	auto task_vertex_buffer = daxa::TaskBuffer({
		.initial_buffers = {.buffers = std::span{&buffer_id, 1}},
		.name = "task vertex buffer",
	});

	auto loop_task_graph = daxa::TaskGraph({
		.device = device,
		.swapchain = swapchain,
		.name = "loop",
	});
	loop_task_graph.use_persistent_buffer(task_vertex_buffer);
	loop_task_graph.use_persistent_image(task_swapchain_image);
	draw_vertices_task(loop_task_graph, pipeline, task_vertex_buffer, task_swapchain_image);
	return 0;
}
