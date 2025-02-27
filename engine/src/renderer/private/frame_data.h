﻿#pragma once

#include "common.h"
#include "shader_interop_renderer.h"
#include "renderer/public/renderer_module.h"
#include <array>

namespace ad_astris::renderer::impl
{
	constexpr uint32_t MAX_CAMERA_COUNT = 16;
	
	class FrameData
	{
		public:
			void init();
			void update_uniform_buffers(DrawContext& drawContext);
		
		private:
			FrameUB _frameData{};
			std::array<RendererCamera, MAX_CAMERA_COUNT> _cameras{};

			void setup_cameras(DrawContext& drawContext);
			void setup_frame_data(DrawContext& drawContext);

			FORCE_INLINE std::string get_buffer_name(const std::string& name)
			{
				return name + std::to_string(FRAME_INDEX);
			}
		
			FORCE_INLINE std::string get_buffer_name(const std::string& name, uint32_t frameIndex)
			{
				return name + std::to_string(frameIndex);
			}
	};
}