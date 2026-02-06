#pragma once
#include <memory>
#include "grove/rhi/vulkan/vulkan_context.hpp"

namespace grove
{
	struct WindowCreateInfo;
	class Window;

	struct GRVEngineCreateInfo
	{
		WindowCreateInfo& windowCreateInfo;
	};

	class GroveEngine
	{
	public:
		GroveEngine();
		~GroveEngine();

		GroveEngine(const GroveEngine &) = delete;
		GroveEngine &operator=(const GroveEngine &) = delete;
		GroveEngine(GroveEngine &&) = delete;
		GroveEngine &operator=(GroveEngine &&) = delete;

		void Init(GRVEngineCreateInfo& grvCreateInfo);
		void Shutdown();

	private:
		void InitWindow(WindowCreateInfo& windowCreateInfo);
		void InitGraphics();

	private:
		std::unique_ptr<Window> window_;
		VulkanContext vkContext_;
	};
}
