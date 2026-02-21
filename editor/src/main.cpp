#include "grove/core/engine.hpp"

int main()
{
	grove::Engine engine;
	if (!engine.Init())
	{
		return 1;
	}

	engine.Run();
	engine.Shutdown();
}