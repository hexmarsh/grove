#include "grove/core/application.hpp"
#include "grove/core/window.hpp"
#include "grove/core/logging.hpp"

namespace grove
{
	Application::Application(const ApplicationCreateInfo& createInfo)
		: createInfo_(createInfo)
	{}

	Application::~Application() = default;

	bool Application::Init()
	{
		WindowCreateInfo windowCreateInfo
		{
			.title = createInfo_.name,
			.width = createInfo_.width,
			.height = createInfo_.height
		};

		GRVEngineCreateInfo engineCreateInfo
		{
			.windowCreateInfo = windowCreateInfo
		};

		engine_.Init(engineCreateInfo);

		return true;
	}

	void Application::Shutdown()
	{
		engine_.Shutdown();
	}

	void Application::Run()
	{
		// TODO: Implement main loop with proper delta time
		GRV_LOG_INFO("Application running");
	}
}