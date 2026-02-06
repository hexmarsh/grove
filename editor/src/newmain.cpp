#include "grove/core/application.hpp"

int main()
{
	using namespace grove;

	ApplicationCreateInfo createInfo
	{
		.name = "GroveEngine Editor",
		.width = 800,
		.height = 600,
	};

	Application app(createInfo);

	if (!app.Init())
	{
		return 1;
	}

	app.Run();
	app.Shutdown();
}