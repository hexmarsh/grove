#pragma once

#include "grove/core/logging/logger.hpp"

namespace grove
{
	class GroveEngine
	{
	public:
		GroveEngine();
		~GroveEngine();

		GroveEngine(const GroveEngine &) = delete;
		GroveEngine &operator=(const GroveEngine &) = delete;
		GroveEngine(GroveEngine &&) = delete;
		GroveEngine &operator=(GroveEngine &&) = delete;

		void Init();
		void Shutdown();

	private:
		log::Logger logger_{ log::Level::Trace };
	};
}
