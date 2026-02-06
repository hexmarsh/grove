#pragma once
#include "grove/core/typedefs.hpp"
#include "grove/core/grove_engine.hpp"

namespace grove
{
	struct ApplicationCreateInfo
	{
		const char* name = "GroveEngine Application";
		u32 width = 800;
		u32 height = 600;
	};

	class Application
	{
	public:
		Application(const ApplicationCreateInfo& createInfo);
		virtual ~Application();

		bool Init();
		void Run();
		void Shutdown();

	private:
		GroveEngine engine_;
		ApplicationCreateInfo createInfo_;
	};
}