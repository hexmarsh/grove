#include "grove/core/engine.hpp"
#include "grove/core/core.hpp"
#include "grove/core/window.hpp"
#include "grove/rhi/vulkan/vk_context.hpp"
#include "grove/rhi/command_buffer.hpp"

namespace grove
{
	Engine::Engine()
	{}

	Engine::~Engine()
	{
		Shutdown();
	}

	Status Engine::Init()
	{
		LogManager::Init();

		WindowCreateInfo windowInfo 
		{
			.title = "GroveEngine",
			.width = 800,
			.height = 600
		};

		GRV_TRY(InitWindow(windowInfo));
		GRV_TRY(InitGraphics());

		return GRV_OK;
	}

	void Engine::Shutdown()
	{
		if (window_)
		{
			window_.reset();
		}
	}

	void Engine::Run()
	{
		CommandBuffer cmdBuffer;

		while (!window_->ShouldClose())
		{
			window_->OnUpdate();
			cmdBuffer.Reset();
			cmdBuffer.Barrier({ .before = ResourceState::Undefined, .after = ResourceState::RenderTarget });
			cmdBuffer.BeginRendering(RGBAColor(0.0f, 0.0f, 0.0f, 1.0f));
			cmdBuffer.Draw(3);
			cmdBuffer.EndRendering();
			cmdBuffer.Barrier({ .before = ResourceState::RenderTarget, .after = ResourceState::Present });

			device_.BeginFrame();
			device_.RecordCommands(cmdBuffer);
			device_.EndFrame();
		}
	}

	Status Engine::InitWindow(const WindowCreateInfo& windowCreateInfo)
	{
		GRV_TRY_ASSIGN(window_, Window::Create(windowCreateInfo));

		return GRV_OK;
	}

	Status Engine::InitGraphics()
	{
		GRV_TRY_ASSIGN(context_, VKContext::Create(*window_));
		
		VKDeviceCreateInfo deviceCreateInfo
		{
			.vkContext = &context_,
			.window = window_.get()
		};
		GRV_TRY_ASSIGN(device_, VKDevice::Create(deviceCreateInfo));

		return GRV_OK;
	}
}