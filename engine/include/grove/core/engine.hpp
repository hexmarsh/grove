#pragma once
#include <memory>
#include "grove/rhi/vulkan/vk_context.hpp"
#include "grove/rhi/vulkan/vk_device.hpp"

namespace grove
{
	struct WindowCreateInfo;
	class Window;

	class Engine 
	{
	public:
		Engine();
		~Engine();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(Engine&&) = delete;

		Status Init();
		void Run();
		void Shutdown();

	private:
		Status InitWindow(const WindowCreateInfo& windowCreateInfo);
		Status InitGraphics();

	private:
		std::unique_ptr<Window> window_;
		VKContext context_;
		VKDevice device_;
	};
}
