#include "window.hpp"
#include "shaders/shared.inl"

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
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
			auto staging_buffer_id = ti.device.create_buffer({
				.size = sizeof(data),
				.allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
				.name = "my staging buffer",
			});
			 
		}
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
	while(!window.should_close()) {
		window.update();
	}

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
	return 0;
}
