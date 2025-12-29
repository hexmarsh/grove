#include "grove/core/grove_engine.hpp"

namespace grove
{
	GroveEngine::GroveEngine()
	{ }

	GroveEngine::~GroveEngine()
	{ }

	void GroveEngine::Init()
	{
		log::LogManager::Init(logger_);
	}

	void GroveEngine::Shutdown()
	{
		log::LogManager::Shutdown();
	}
}